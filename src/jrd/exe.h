/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		exe.h
 *	DESCRIPTION:	Execution block definitions
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
 *
 * 2001.07.28: Added rse_skip to class RecordSelExpr to support LIMIT.
 * 2002.09.28 Dmitry Yemanov: Reworked internal_info stuff, enhanced
 *                            exception handling in SPs/triggers,
 *                            implemented ROWS_AFFECTED system variable
 * 2002.10.21 Nickolay Samofatov: Added support for explicit pessimistic locks
 * 2002.10.29 Nickolay Samofatov: Added support for savepoints
 */

#ifndef JRD_EXE_H
#define JRD_EXE_H

#include "../jrd/jrd_blks.h"
#include "../common/classes/array.h"

#include "gen/iberror.h"

#define NODE(type, name, keyword) type,

typedef enum nod_t {
#include "../jrd/nod.h"
	nod_MAX
#undef NODE
} NOD_T;

#include "../jrd/dsc.h"
#include "../jrd/rse.h"

#include "../jrd/err_proto.h"

// This macro enables DSQL tracing code
//#define CMP_DEBUG

#ifdef CMP_DEBUG
DEFINE_TRACE_ROUTINE(cmp_trace);
#define CMP_TRACE(args) cmp_trace args
#else
#define CMP_TRACE(args) /* nothing */
#endif

class str;
struct dsc;
class lls;

namespace Jrd {

class jrd_rel;
class jrd_nod;
struct sort_key_def;
class SparseBitmap;
class vec;
class jrd_prc;
struct index_desc;
struct IndexDescAlloc;
class Format;

// NOTE: The definition of structures RecordSelExpr and lit must be defined in
//       exactly the same way as structure jrd_nod through item nod_count.
//       Now, inheritance takes care of those common data members.
class jrd_node_base : public pool_alloc_rpt<jrd_nod*, type_nod>
{
public:
	jrd_nod*	nod_parent;
	SLONG	nod_impure;			/* Inpure offset from request block */
	NOD_T	nod_type;				/* Type of node */
	UCHAR	nod_flags;
	SCHAR	nod_scale;			/* Target scale factor */
	USHORT	nod_count;			/* Number of arguments */
	Firebird::Array<jrd_nod*> *nod_variables; /* Variables and arguments this node depends on */
};


class jrd_nod : public jrd_node_base
{
public:
/*	jrd_nod()
	:	nod_parent(0),
		nod_impure(0),
		nod_type(nod_nop),
		nod_flags(0),
		nod_scale(0),
		nod_count(0)
	{
		nod_arg[0] = 0;
	}*/
	jrd_nod*	nod_arg[1];
};

#define nod_comparison 	1
#define nod_id		1			/* marks a field node as a blr_fid guy */
#define nod_quad	2			/* compute in quad (default is long) */
#define nod_any_and     2		/* and node is mapping of quantified predicate */
#define nod_double	4
#define nod_date	8
#define nod_value	16			/* full value area required in impure space */
#define nod_evaluate	32		/* (Gateway only) */
#define nod_agg_dbkey	64		/* dbkey of an aggregate */
#define nod_invariant	128		/* node is recognized as being invariant */


/* Special RecordSelExpr node */

class RecordSelExpr : public jrd_node_base
{
public:
	USHORT		rse_count;
	USHORT		rse_jointype;		/* inner, left, full */
	bool		rse_writelock;
	RecordSource*	rse_rsb;
	jrd_nod*	rse_first;
    jrd_nod*	rse_skip;
	jrd_nod*	rse_boolean;
	jrd_nod*	rse_sorted;
	jrd_nod*	rse_projection;
	jrd_nod*	rse_aggregate;	/* singleton aggregate for optimizing to index */
	jrd_nod*	rse_plan;		/* user-specified access plan */
#ifdef SCROLLABLE_CURSORS
	jrd_nod*	rse_async_message;	/* asynchronous message to send for scrolling */
#endif
	jrd_nod*	rse_relation[1];
};


#define rse_stream	1			/* flags RecordSelExpr-type node as a blr_stream type */
#define rse_singular	2		/* flags RecordSelExpr-type node as from a singleton select */
#define rse_variant	4			/* flags RecordSelExpr as variant (not invariant?) */

// Number of nodes may fit into nod_arg of normal node to get to rse_relation
const size_t rse_delta = (sizeof(RecordSelExpr) - sizeof(jrd_nod)) / sizeof(jrd_nod::blk_repeat_type);

// Types of nulls placement for each column in sort order
#define rse_nulls_default 0
#define rse_nulls_first 1
#define rse_nulls_last 2


/* Literal value */

class Literal : public jrd_node_base
{
public:
	dsc		lit_desc;
	SINT64	lit_data[1]; // Defined this way to prevent SIGBUS error in 64-bit ports
};

#define lit_delta	((sizeof(Literal) - sizeof(jrd_nod) - sizeof(SINT64)) / sizeof(jrd_nod**))


/* Aggregate Sort Block (for DISTINCT aggregates) */

class AggregateSort : public pool_alloc<type_asb>
{
public:
	jrd_nod*	nod_parent;
	SLONG	nod_impure;			/* Impure offset from request block */
	NOD_T	nod_type;				/* Type of node */
	UCHAR	nod_flags;
	SCHAR	nod_scale;
	USHORT	nod_count;
	dsc		asb_desc;
	sort_key_def* asb_key_desc;	/* for the aggregate   */
	UCHAR	asb_key_data[1];
};

#define asb_delta	((sizeof(AggregateSort) - sizeof(jrd_nod)) / sizeof (jrd_nod**))


/* Various structures in the impure area */

struct impure_state {
	SSHORT sta_state;
};

struct impure_value {
	dsc vlu_desc;
	USHORT vlu_flags; // Computed/invariant flags
	str* vlu_string;
	union {
		SSHORT vlu_short;
		SLONG vlu_long;
		SINT64 vlu_int64;
		SQUAD vlu_quad;
		SLONG vlu_dbkey[2];
		float vlu_float;
		double vlu_double;
		GDS_TIMESTAMP vlu_timestamp;
		GDS_TIME vlu_sql_time;
		GDS_DATE vlu_sql_date;
		void* vlu_invariant; // Pre-compiled invariant object for nod_like and other string functions
	} vlu_misc;
};

struct impure_value_ex : public impure_value {
	SLONG vlux_count;
};


#define VLU_computed	1		/* An invariant sub-query has been computed */
#define VLU_null	2			/* An invariant sub-query computed to null */


/* Inversion (i.e. nod_index) impure area */

struct impure_inversion {
	SparseBitmap* inv_bitmap;
};


/* AggregateSort impure area */

struct impure_agg_sort {
	sort_context* iasb_sort_handle;
};


/* Various field positions */

#define	e_for_re		0
#define	e_for_statement		1
#define	e_for_stall		2
#define	e_for_rsb		3
#define	e_for_length		4

#define	e_arg_flag		0
#define e_arg_indicator		1
#define	e_arg_message		2
#define	e_arg_number		3
#define	e_arg_length		4

#define	e_msg_number		0
#define	e_msg_format		1
#define e_msg_invariants	2
#define	e_msg_next		3
#define	e_msg_length		4

#define	e_fld_stream		0
#define	e_fld_id		1
#define	e_fld_default_value	2	/* hold column default value info if any,
								   (Literal*) */
#define	e_fld_length		3

#define	e_sto_statement		0
#define	e_sto_statement2	1
#define e_sto_sub_store		2
#define e_sto_validate		3
#define	e_sto_relation		4
#define e_sto_stream		5
#define	e_sto_length		6

#define e_erase_statement	0
#define e_erase_sub_erase 	1
#define	e_erase_stream		2
#define e_erase_rsb		3
#define	e_erase_length		4

#define e_sav_operation		0
#define e_sav_name			1
#define e_sav_length		2

#define	e_mod_statement		0
#define e_mod_sub_mod		1
#define e_mod_validate		2
#define e_mod_map_view		3
#define	e_mod_org_stream	4
#define	e_mod_new_stream	5
#define e_mod_rsb		6
#define	e_mod_length		7

#define	e_send_statement	0
#define	e_send_message		1
#define	e_send_length		2

#define	e_asgn_from		0
#define	e_asgn_to		1
#define e_asgn_missing		2	/* Value for comparison for missing */
#define e_asgn_missing2		3	/* Value for substitute for missing */
#define	e_asgn_length		4

#define	e_rel_stream		0
#define	e_rel_relation		1
#define	e_rel_view		2		/* parent view for posting access */
#define e_rel_alias		3		/* SQL alias for the relation */
#define e_rel_context		4	/* user-specified context number for the relation reference */
#define	e_rel_length		5

#define	e_idx_retrieval		0
#define	e_idx_length		1

#define	e_lbl_statement		0
#define	e_lbl_label		1
#define	e_lbl_length		2

#define	e_any_rse		0
#define	e_any_rsb		1
#define	e_any_length		2

#define e_if_boolean		0
#define e_if_true		1
#define e_if_false		2
#define e_if_length		3

#define e_hnd_statement		0
#define e_hnd_length		1

#define e_val_boolean		0
#define e_val_value		1
#define e_val_length		2

#define e_uni_stream		0	/* Stream for union */
#define e_uni_clauses		1	/* RecordSelExpr's for union */
#define e_uni_length		2

#define e_agg_stream		0
#define e_agg_rse		1
#define e_agg_group		2
#define e_agg_map		3
#define e_agg_length		4

/* Statistical expressions */

#define	e_stat_rse		0
#define	e_stat_value		1
#define	e_stat_default		2
#define	e_stat_rsb		3
#define	e_stat_length		4

/* Execute stored procedure */

#define e_esp_inputs		0
#define e_esp_in_msg		1
#define e_esp_outputs		2
#define e_esp_out_msg		3
#define e_esp_procedure		4
#define e_esp_length		5

/* Stored procedure view */

#define e_prc_inputs		0
#define e_prc_in_msg		1
#define e_prc_stream		2
#define e_prc_procedure		3
#define e_prc_length		4

/* Function expression */

#define e_fun_args		0
#define e_fun_function		1
#define e_fun_length		2

/* Generate id */

#define e_gen_value		0
#define e_gen_relation		1
#define e_gen_id		1		/* Generator id (replaces e_gen_relation) */
#define e_gen_length		2

/* Protection mask */

#define e_pro_class		0
#define e_pro_relation		1
#define e_pro_length		2

/* Exception */

#define e_xcp_desc	0
#define e_xcp_msg	1
#define e_xcp_length	2

/* Variable declaration */

#define e_var_id		0
#define e_var_variable		1
#define e_var_length		2

#define e_dcl_id		0
#define e_dcl_invariants	1
#define e_dcl_desc		2
#define e_dcl_length		(2 + sizeof (DSC)/sizeof (::Jrd::jrd_nod*))	/* Room for descriptor */

#define e_dep_object		0	/* node for registering dependencies */
#define e_dep_object_type	1
#define e_dep_field		2
#define e_dep_length		3

#define e_scl_field		0		/* Scalar expression (blr_index) */
#define e_scl_subscripts	1
#define e_scl_length		2

#define	e_blk_action		0
#define	e_blk_handlers		1
#define	e_blk_length		2

#define	e_err_action		0
#define	e_err_conditions	1
#define	e_err_length		2

/* Datatype cast operator */

#define e_cast_source		0
#define e_cast_fmt		1
#define e_cast_length		2

/* IDAPI semantics nodes */

#define e_index_index		0	/* set current index (blr_set_index) */
#define e_index_stream		1
#define e_index_rsb		2
#define e_index_length		3

#define e_seek_offset		0	/* for seeking through a stream */
#define e_seek_direction	1
#define e_seek_rse		2
#define e_seek_length		3

#define e_find_args		0		/* for finding a key value in a stream */
#define e_find_operator		1
#define e_find_direction	2
#define e_find_stream		3
#define e_find_rsb		4
#define e_find_length		5

#define e_bookmark_id		0	/* nod_bookmark */
#define e_bookmark_length	1

#define e_setmark_id		0	/* nod_set_bookmark */
#define e_setmark_stream	1
#define e_setmark_rsb		2
#define e_setmark_length	3

#define e_getmark_stream	0	/* nod_get_bookmark */
#define e_getmark_rsb		1
#define e_getmark_length	2

#define e_relmark_id		0	/* nod_release_bookmark */
#define e_relmark_length	1

#define e_lockrel_relation	0	/* nod_lock_relation */
#define e_lockrel_level		1
#define e_lockrel_length	2

#define e_lockrec_level		0	/* nod_lock_record */
#define e_lockrec_stream	1
#define e_lockrec_rsb		2
#define e_lockrec_length	3

#define e_brange_number		0	/* nod_begin_range */
#define e_brange_length		1

#define e_erange_number		0	/* nod_end_range */
#define e_erange_length		1

#define e_drange_number		0	/* nod_delete_range */
#define e_drange_length		1

#define e_rellock_lock		0	/* nod_release_lock */
#define e_rellock_length	1

#define e_find_dbkey_dbkey	0	/* double duty for nod_find_dbkey and nod_find_dbkey_version */
#define e_find_dbkey_version	1
#define e_find_dbkey_stream	2
#define e_find_dbkey_rsb	3
#define e_find_dbkey_length	4

#define e_range_relation_number	  0	/* nod_range_relation */
#define e_range_relation_relation 1
#define e_range_relation_length	  2

#define e_retrieve_relation	0
#define e_retrieve_access_type	1
#define e_retrieve_length	2

#define e_reset_from_stream	0
#define e_reset_to_stream	1
#define e_reset_from_rsb	2
#define e_reset_length		3

#define e_card_stream		0
#define e_card_rsb		1
#define e_card_length		2

/* SQL Date supporting nodes */
#define e_extract_value		0	/* Node */
#define e_extract_part		1	/* Integer */
#define e_extract_count		1	/* Number of nodes */
#define e_extract_length	2	/* Number of entries in nod_args */

#define e_current_date_length	1
#define e_current_time_length	1
#define e_current_timestamp_length	1

#define e_dcl_cursor_number		0
#define e_dcl_cursor_rse		1
#define e_dcl_cursor_rsb		2
#define e_dcl_cursor_length		3

#define e_cursor_stmt_op		0
#define e_cursor_stmt_number	1
#define e_cursor_stmt_seek		2
#define e_cursor_stmt_into		3
#define e_cursor_stmt_length	4

// Request resources

struct Resource
{
	enum rsc_s
	{
		rsc_relation,
		rsc_procedure,
		rsc_index
	};

	enum rsc_s	rsc_type;
	USHORT		rsc_id;			/* Id of the resource */
	jrd_rel*	rsc_rel;		/* Relation block */
	jrd_prc*	rsc_prc;		/* Procedure block */

	static bool greaterThan(const Resource& i1, const Resource& i2) {
		// A few places of the engine depend on fact that rsc_type 
		// is the first field in ResourceList ordering
		if (i1.rsc_type != i2.rsc_type)
			return i1.rsc_type > i2.rsc_type;
		if (i1.rsc_type == rsc_index) {
			// Sort by relation ID for now
			if (i1.rsc_rel->rel_id != i2.rsc_rel->rel_id)
				return i1.rsc_rel->rel_id > i2.rsc_rel->rel_id;
		}
		return i1.rsc_id > i2.rsc_id;
	}

	Resource(rsc_s type, USHORT id, jrd_rel* rel, jrd_prc* prc) :
		rsc_type(type), rsc_id(id), rsc_rel(rel), rsc_prc(prc) { }
};

typedef Firebird::SortedArray<Resource, Firebird::EmptyStorage<Resource>, 
	Resource, Firebird::DefaultKeyValue<Resource>, Resource> ResourceList;

/* Access items */

struct AccessItem
{
	const TEXT*	acc_security_name;
	SLONG	acc_view_id;
	const TEXT*	acc_name;
	const TEXT*	acc_type;
	USHORT		acc_mask;

	static int strcmp_null(const char* s1, const char* s2) {
		return s1 == NULL ? s2 != NULL : s2 == NULL ? -1 : strcmp(s1, s2);
	}

	static bool greaterThan(const AccessItem& i1, const AccessItem& i2) {
		int v;
		if ((v = strcmp_null(i1.acc_security_name, i2.acc_security_name)) != 0)
			return v > 0;

		if (i1.acc_view_id != i2.acc_view_id)
			return i1.acc_view_id > i2.acc_view_id;

		if (i1.acc_mask != i2.acc_mask)
			return i1.acc_mask > i2.acc_mask;

		if ((v = strcmp(i1.acc_type, i2.acc_type)) != 0) 
			return v > 0;

		if ((v = strcmp(i1.acc_name, i2.acc_name)) != 0)
			return v > 0;

		return false; // Equal
	}

	AccessItem(const TEXT* security_name, SLONG view_id, const TEXT* name,
		const TEXT* type, USHORT mask) 
	: acc_security_name(security_name), acc_view_id(view_id), acc_name(name),
		acc_type(type), acc_mask(mask)
	{}
};

typedef Firebird::SortedArray<AccessItem, Firebird::EmptyStorage<AccessItem>, 
	AccessItem, Firebird::DefaultKeyValue<AccessItem>, AccessItem> AccessItemList;

// Triggers and procedures the request accesses
struct ExternalAccess
{
	enum exa_act {
		exa_procedure,
		exa_insert,
		exa_update,
		exa_delete
	};
	exa_act exa_action;
	USHORT exa_prc_id;
	USHORT exa_rel_id;
	USHORT exa_view_id;

	// Procedure
	ExternalAccess(USHORT prc_id) : 
		exa_action(exa_procedure), exa_prc_id(prc_id), exa_rel_id(0), exa_view_id(0)
	{ }

	// Trigger
	ExternalAccess(exa_act action, USHORT rel_id, USHORT view_id) :
		exa_action(action), exa_prc_id(0), exa_rel_id(rel_id), exa_view_id(view_id)
	{ }

	static bool greaterThan(const ExternalAccess& i1, const ExternalAccess& i2) {
		if (i1.exa_action != i2.exa_action) return i1.exa_action > i2.exa_action;
		if (i1.exa_prc_id != i2.exa_prc_id) return i1.exa_prc_id > i2.exa_prc_id;
		if (i1.exa_rel_id != i2.exa_rel_id) return i1.exa_rel_id > i2.exa_rel_id;
		if (i1.exa_view_id != i2.exa_view_id) return i1.exa_view_id > i2.exa_view_id;
		return false; // Equal
	}
};

typedef Firebird::SortedArray<ExternalAccess, Firebird::EmptyStorage<ExternalAccess>, 
	ExternalAccess, Firebird::DefaultKeyValue<ExternalAccess>, ExternalAccess> ExternalAccessList;

/* Compile scratch block */

/*
 * TMN: I had to move the enclosed csb_repeat outside this class,
 * since it's part of the C API. Compiling as C++ would enclose it.
 */
// CVC: Mike comment seems to apply only when the conversion to C++
// was being done. It's almost impossible that a repeating structure of
// the compiler scratch block be available to outsiders.

typedef Firebird::SortedArray<SLONG> VarInvariantArray;
typedef Firebird::Array<VarInvariantArray*> MsgInvariantArray;

class CompilerScratch : public pool_alloc<type_csb>
{
public:
	CompilerScratch(MemoryPool& p, size_t len)
	:	/*csb_blr(0),
		csb_running(0),
		csb_node(0),
		csb_variables(0),
		csb_dependencies(0),
#ifdef SCROLLABLE_CURSORS
		csb_current_rse(0),
#endif
		csb_async_message(0),
		csb_count(0),
		csb_n_stream(0),
		csb_msg_number(0),
		csb_impure(0),
		csb_g_flags(0),*/
		csb_external(p),
		csb_access(p),
		csb_resources(p),
		csb_fors(p),
		csb_invariants(p),
		csb_current_nodes(p),
		csb_pool(p),
		csb_rpt(p, len)
	{}

	static CompilerScratch* newCsb(MemoryPool& p, size_t len)
		{ return FB_NEW(p) CompilerScratch(p, len); }

	int nextStream(bool check = true)
	{
		if (csb_n_stream >= MAX_STREAMS && check)
		{
			ERR_post(isc_too_many_contexts, 0);
		}
		return csb_n_stream++;
	}

	const UCHAR*	csb_blr;
	const UCHAR*	csb_running;
	jrd_nod*		csb_node;
	ExternalAccessList csb_external;      /* Access to outside procedures/triggers to be checked */
	AccessItemList	csb_access;			/* Access items to be checked */
	vec*			csb_variables;		/* Vector of variables, if any */
	ResourceList	csb_resources;		/* Resources (relations and indexes) */
	lls*			csb_dependencies;	/* objects this request depends upon */
	Firebird::Array<RecordSource*> csb_fors;		/* stack of fors */
	Firebird::Array<jrd_nod*> csb_invariants;	/* stack of invariant nodes */
	Firebird::Array<jrd_node_base*> csb_current_nodes;	/* RecordSelExpr's and other invariant candidates within whose scope we are */
#ifdef SCROLLABLE_CURSORS
	RecordSelExpr*	csb_current_rse;	/* this holds the RecordSelExpr currently being processed;
									   unlike the current_rses stack, it references any expanded view RecordSelExpr */
#endif
	jrd_nod*		csb_async_message;	/* asynchronous message to send to request */
	USHORT			csb_n_stream;		/* Next available stream */
	USHORT			csb_msg_number;		/* Highest used message number */
	SLONG			csb_impure;			/* Next offset into impure area */
	USHORT			csb_g_flags;
	MemoryPool&		csb_pool;				/* Memory pool to be used by csb */

    struct csb_repeat
	{
		// We must zero-initialize this one
		csb_repeat()
		:	csb_stream(0),
			csb_view_stream(0),
			csb_flags(0),
			csb_indices(0),
			csb_relation(0),
			csb_alias(0),
			csb_procedure(0),
			csb_view(0),
			csb_idx(0),
			csb_idx_allocation(0),
			csb_message(0),
			csb_format(0),
			csb_fields(0),
			csb_cardinality(0.0f),	// TMN: Non-natural cardinality?!
			csb_plan(0),
			csb_map(0),
			csb_rsb_ptr(0)
		{}

		UCHAR csb_stream;			/* Map user context to internal stream */
		UCHAR csb_view_stream;		/* stream number for view relation, below */
		USHORT csb_flags;
		USHORT csb_indices;			/* Number of indices */

		jrd_rel* csb_relation;
		Firebird::string* csb_alias;	/* SQL alias name for this instance of relation */
		jrd_prc* csb_procedure;
		jrd_rel* csb_view;		/* parent view */

		index_desc* csb_idx;		/* Packed description of indices */
		IndexDescAlloc *csb_idx_allocation;	/* Memory allocated to hold index descriptions */
		jrd_nod* csb_message;			/* Msg for send/receive */
		Format* csb_format;		/* Default Format for stream */
		SparseBitmap* csb_fields;		/* Fields referenced */
		float csb_cardinality;		/* Cardinality of relation */
		jrd_nod* csb_plan;				/* user-specified plan for this relation */
		UCHAR* csb_map;				/* Stream map for views */
		RecordSource** csb_rsb_ptr;	/* point to rsb for nod_stream */
	};


	typedef csb_repeat* rpt_itr;
	typedef const csb_repeat* rpt_const_itr;
	Firebird::HalfStaticArray<csb_repeat, 5> csb_rpt;
};

#define csb_internal	     	0x1	/* "csb_g_flag" switch */
#define csb_get_dependencies 	0x2
#define csb_ignore_perm 	0x4	/* ignore permissions checks */
#define csb_blr_version4 	0x8	/* The blr is of version 4 */

#define csb_active 	1
#define csb_used	2           /* Context has already been defined (BLR parsing only) */
#define csb_view_update	4		/* View update w/wo trigger is in progress */
#define csb_trigger	8			/* NEW or OLD context in trigger */
#define csb_no_dbkey	16		/* Stream doesn't have a dbkey */
#define csb_validation	32		/* We're in a validation expression (RDB hack) */
#define csb_store	64			/* we are processing a store statement */
#define csb_modify	128			/* we are processing a modify */
#define csb_compute	256			/* compute cardinality for this stream */
#define csb_erase	512			/* we are processing an erase */
#define csb_unmatched	1024	/* stream has conjuncts unmatched by any index */

#define csb_dbkey	8192		/* Dbkey as been requested (Gateway only) */
#define csb_update	16384		/* Erase or modify for relation */
#define csb_made_river	32768	/* stream has been included in a river */

/* Exception condition list */

struct xcp_repeat {
	SSHORT xcp_type;
	SLONG xcp_code;
};

class PsqlException : public pool_alloc_rpt<xcp_repeat, type_xcp>
{
    public:
	SLONG xcp_count;
    xcp_repeat xcp_rpt[1];
};

#define xcp_sql_code	1
#define xcp_gds_code	2
#define xcp_xcp_code	3
#define xcp_default	4

class StatusXcp {
	ISC_STATUS_ARRAY status;

public:
	StatusXcp();

	void clear();
	void init(const ISC_STATUS*);
	void copyTo(ISC_STATUS*) const;
	bool success() const;
	SLONG as_gdscode() const;
	SLONG as_sqlcode() const;
};

#define XCP_MESSAGE_LENGTH	78	// must correspond to the size of
								// RDB$EXCEPTIONS.RDB$MESSAGE
} //namespace Jrd

#endif // JRD_EXE_H

