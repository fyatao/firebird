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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_WINDOW_RSB_H
#define JRD_WINDOW_RSB_H

#include "../jrd/VirtualTable.h"

namespace Jrd {


class WindowRsb : public RecordStream
{
private:
	WindowRsb(RecordSource* aRsb);

public:
	static RecordSource* create(thread_db*, OptimizerBlk* opt, RecordSource* next);

public:
	virtual unsigned dump(UCHAR* buffer, unsigned bufferLen);
	virtual void open(thread_db* tdbb);
	virtual void close(thread_db* tdbb);
	virtual bool get(thread_db* tdbb);
	virtual void markRecursive();

private:
	RecordSource* rsb;
	RecordSource* next;
};


} // namespace Jrd

#endif // JRD_WINDOW_RSB_H
