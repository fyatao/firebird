/*
 *	PROGRAM:	JRD Rebuild scrambled database
 *	MODULE:		rmet.e
 *	DESCRIPTION:	Crawl around the guts of a database
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include "../include/jrd/gds.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/pag.h"
#include "../utilities/rebuild.h"
#include "../utilities/rebui_proto.h"
#include "../utilities/rmet_proto.h"
#include "../jrd/gds_proto.h"

DATABASE DB = STATIC FILENAME "rebuild.gdb";


ULONG *RMET_tips(TEXT * db_in)
{
/**************************************
 *
 *	R M E T _ t i p s
 *
 **************************************
 *
 * Functional description
 *	crawl into the pages relation and
 *	build a list of tips in order
 *
 **************************************/
	ULONG last, *tip, *tips;

	READY GDS_VAL(db_in) AS DB
	ON_ERROR
		gds__print_status(gds__status);
		ib_printf("can't open the databse so skip the tip list\n");
		return NULL;
	END_ERROR;
	START_TRANSACTION
	ON_ERROR
		gds__print_status(gds__status);
		ib_printf("can't start a transaction so skip the tip list\n");
		FINISH DB;
		return NULL;
	END_ERROR;

	last = 0;
	FOR X IN RDB$PAGES WITH X.RDB$PAGE_TYPE = pag_transactions
		last++;
	END_FOR
	ON_ERROR
		gds__print_status(gds__status);
		ib_printf("can't read RDB$PAGES, so skip the tip list\n");
		FINISH DB;
		return NULL;
	END_ERROR;

	tips = tip = (ULONG *) RBDB_alloc(++last * sizeof(ULONG));

	FOR X IN RDB$PAGES WITH X.RDB$PAGE_TYPE = pag_transactions
		{
			*tip = X.RDB$PAGE_NUMBER;
			tip++;
		}
	END_FOR
	ON_ERROR
		gds__print_status(gds__status);
		ib_printf("can't re-read RDB$PAGES, so skip the tip list\n");
		FINISH DB;
		return NULL;
	END_ERROR;

	COMMIT;
	FINISH;

	return tips;
}
