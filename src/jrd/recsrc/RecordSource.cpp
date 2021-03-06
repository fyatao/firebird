/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/btr.h"
#include "../jrd/intl.h"
#include "../jrd/req.h"
#include "../jrd/rse.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/DataTypeUtil.h"

#include "RecordSource.h"

using namespace Firebird;
using namespace Jrd;


// Record source class
// -------------------

string RecordSource::printName(thread_db* tdbb, const string& name)
{
	ULONG nameLength = (ULONG) name.length();
	const UCHAR* namePtr = (UCHAR*) name.c_str();

	MoveBuffer nameBuffer;

	const CHARSET_ID charset = tdbb->getCharSet();
	if (charset != CS_METADATA && charset != CS_NONE)
	{
		const ULONG bufferLength = INTL_convert_bytes(tdbb, charset, NULL, 0,
													  CS_METADATA, namePtr, nameLength, ERR_post);
		nameBuffer.getBuffer(bufferLength);
		nameLength = INTL_convert_bytes(tdbb, charset, nameBuffer.begin(), bufferLength,
										CS_METADATA, namePtr, nameLength, ERR_post);

		namePtr = nameBuffer.begin();
	}

	return string(namePtr, nameLength);
}

string RecordSource::printIndent(unsigned level)
{
	fb_assert(level);

	const string indent(level * 4, ' ');
	return string("\n" + indent + "-> ");
}

void RecordSource::printInversion(thread_db* tdbb, const InversionNode* inversion,
								  string& plan, bool detailed, unsigned level, bool navigation)
{
	if (detailed)
	{
		plan += printIndent(++level);
	}

	switch (inversion->type)
	{
	case InversionNode::TYPE_AND:
		if (detailed)
		{
			plan += "Bitmap And";
		}
		printInversion(tdbb, inversion->node1, plan, detailed, level);
		printInversion(tdbb, inversion->node2, plan, detailed, level);
		break;

	case InversionNode::TYPE_OR:
	case InversionNode::TYPE_IN:
		if (detailed)
		{
			plan += "Bitmap Or";
		}
		printInversion(tdbb, inversion->node1, plan, detailed, level);
		printInversion(tdbb, inversion->node2, plan, detailed, level);
		break;

	case InversionNode::TYPE_DBKEY:
		if (detailed)
		{
			plan += "DBKEY";
		}
		break;

	case InversionNode::TYPE_INDEX:
		{
			MetaName indexName;
			MET_lookup_index(tdbb, indexName, inversion->retrieval->irb_relation->rel_name,
							 (USHORT) (inversion->retrieval->irb_index + 1));

			if (detailed)
			{
				if (!navigation)
				{
					plan += "Bitmap" + printIndent(++level);
				}
				plan += "Index \"" + printName(tdbb, indexName.c_str()) + "\" Scan";
			}
			else
			{
				plan += (plan.hasData() ? ", " : "") + printName(tdbb, indexName.c_str());
			}
		}
		break;

	default:
		fb_assert(false);
	}
}

void RecordSource::saveRecord(thread_db* tdbb, record_param* rpb)
{
	MemoryPool* const pool = tdbb->getDefaultPool();

	SaveRecordParam* rpb_copy = rpb->rpb_copy;
	if (rpb_copy)
	{
		delete rpb_copy->srpb_rpb->rpb_record;
	}
	else
	{
		rpb->rpb_copy = rpb_copy = FB_NEW(*pool) SaveRecordParam();
	}

	memcpy(rpb_copy->srpb_rpb, rpb, sizeof(record_param));

	Record* const record = rpb->rpb_record;

	if (record)
	{
		const USHORT size = record->rec_length;
		Record* rec_copy = FB_NEW_RPT(*pool, size) Record(*pool);
		rpb_copy->srpb_rpb->rpb_record = rec_copy;

		rec_copy->rec_length = size;
		rec_copy->rec_format = record->rec_format;
		rec_copy->rec_number = record->rec_number;
		memcpy(rec_copy->rec_data, record->rec_data, size);
	}
	else
	{
		rpb_copy->srpb_rpb->rpb_record = NULL;
	}
}

void RecordSource::restoreRecord(thread_db* tdbb, record_param* rpb)
{
	MemoryPool* const pool = tdbb->getDefaultPool();

	SaveRecordParam* rpb_copy = rpb->rpb_copy;
	if (rpb_copy)
	{
		Record* record = rpb->rpb_record;
		Record* const rec_copy = rpb_copy->srpb_rpb->rpb_record;

		if (rec_copy)
		{
			if (!record)
				BUGCHECK(284);	// msg 284 cannot restore singleton select data

			const USHORT size = rec_copy->rec_length;
			if (size > record->rec_length)
			{
				// hvlad: saved copy of record has longer format, reallocate
				// given record to make enough space for saved data
				record = VIO_record(tdbb, rpb, rec_copy->rec_format, pool);
			}
			else
			{
				record->rec_length = size;
				record->rec_format = rec_copy->rec_format;
			}
			record->rec_number = rec_copy->rec_number;
			memcpy(record->rec_data, rec_copy->rec_data, size);

			delete rec_copy;
		}

		memcpy(rpb, rpb_copy->srpb_rpb, sizeof(record_param));

		rpb->rpb_record = record;

		delete rpb_copy;
		rpb->rpb_copy = NULL;
	}
}

RecordSource::~RecordSource()
{
}


// RecordStream class
// ------------------

RecordStream::RecordStream(CompilerScratch* csb, StreamType stream, const Format* format)
	: m_stream(stream), m_format(format ? format : csb->csb_rpt[stream].csb_format)
{
	fb_assert(m_format);
}

bool RecordStream::refetchRecord(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	jrd_tra* const transaction = request->req_transaction;

	record_param* const org_rpb = &request->req_rpb[m_stream];

	if (org_rpb->rpb_stream_flags & RPB_s_refetch)
	{
		if (!DPM_get(tdbb, org_rpb, LCK_read) ||
			!VIO_chase_record_version(tdbb, org_rpb, transaction, tdbb->getDefaultPool(), true))
		{
			return false;
		}

		VIO_data(tdbb, org_rpb, tdbb->getDefaultPool());

		org_rpb->rpb_stream_flags &= ~RPB_s_refetch;
	}

	return true;
}

bool RecordStream::lockRecord(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	jrd_tra* const transaction = request->req_transaction;

	record_param* const org_rpb = &request->req_rpb[m_stream];
	jrd_rel* const relation = org_rpb->rpb_relation;

	fb_assert(relation && !relation->rel_view_rse);

	RLCK_reserve_relation(tdbb, transaction, relation, true);

	return VIO_writelock(tdbb, org_rpb, transaction);
}

void RecordStream::markRecursive()
{
	m_recursive = true;
}

void RecordStream::findUsedStreams(StreamList& streams, bool /*expandAll*/) const
{
	if (!streams.exist(m_stream))
		streams.add(m_stream);
}

void RecordStream::invalidateRecords(jrd_req* request) const
{
	record_param* const rpb = &request->req_rpb[m_stream];
	rpb->rpb_number.setValid(false);
}

void RecordStream::nullRecords(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];

	rpb->rpb_number.setValid(false);

	// Make sure a record block has been allocated. If there isn't
	// one, first find the format, then allocate the record block.

	Record* record = rpb->rpb_record;

	if (!record)
	{
		record = VIO_record(tdbb, rpb, m_format, tdbb->getDefaultPool());
	}

	if (record->rec_format)
	{
		record->rec_fmt_bk = record->rec_format;
	}

	record->rec_format = NULL;
}

void RecordStream::saveRecords(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	saveRecord(tdbb, &request->req_rpb[m_stream]);
}

void RecordStream::restoreRecords(thread_db* tdbb) const
{
	jrd_req* const request = tdbb->getRequest();
	restoreRecord(tdbb, &request->req_rpb[m_stream]);
}
