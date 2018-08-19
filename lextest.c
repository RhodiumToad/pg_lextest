
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "catalog/pg_type.h"
#include "executor/tuptable.h"
#include "parser/gramparse.h"
#include "parser/parser.h"

PG_MODULE_MAGIC;

static void lex_str(const char *str)
{
	core_yyscan_t yyscanner;
	base_yy_extra_type yyextra;
	int			cur_token;
	YYLTYPE		cur_yylloc;
	YYSTYPE		lval;
	YYSTYPE	   *lvalp = &lval;

	/* initialize the flex scanner */
	yyscanner = scanner_init(str, &yyextra.core_yy_extra,
							 ScanKeywords, NumScanKeywords);

	for (;;)
	{
		cur_token = core_yylex(&(lvalp->core_yystype), &cur_yylloc, yyscanner);
		if (!cur_token)
			break;
	}

	/* Clean up (release memory) */
	scanner_finish(yyscanner);
}

static void lex_str_project(const char *str, TupleTableSlot *slot, Tuplestorestate *tstore)
{
	core_yyscan_t yyscanner;
	base_yy_extra_type yyextra;
	int			cur_token;
	YYLTYPE		cur_yylloc;
	YYSTYPE		lval;
	YYSTYPE	   *lvalp = &lval;
	text	   *tmpbuf;
	size_t		tmpbuflen;

	tmpbuf = palloc(VARHDRSZ + 256);
	tmpbuflen = 256;

	/* initialize the flex scanner */
	yyscanner = scanner_init(str, &yyextra.core_yy_extra,
							 ScanKeywords, NumScanKeywords);

	for (;;)
	{
		cur_token = core_yylex(&(lvalp->core_yystype), &cur_yylloc, yyscanner);
		if (!cur_token)
			break;
		ExecClearTuple(slot);
		slot->tts_values[0] = Int32GetDatum(cur_yylloc);
		slot->tts_values[1] = Int32GetDatum(cur_token);
		slot->tts_isnull[0] = false;
		slot->tts_isnull[1] = false;
		slot->tts_isnull[2] = true;
		if (cur_token == PARAM || cur_token == ICONST)
		{
			int len = snprintf(VARDATA(tmpbuf), tmpbuflen, "%d", lvalp->core_yystype.ival);
			if (len > 0)
			{
				SET_VARSIZE(tmpbuf, VARHDRSZ + len);
				slot->tts_values[2] = PointerGetDatum(tmpbuf);
				slot->tts_isnull[2] = false;
			}
		}
		else if (cur_token < 256)
		{
			*(unsigned char*)(VARDATA(tmpbuf)) = (unsigned char) cur_token;
			SET_VARSIZE(tmpbuf, VARHDRSZ + 1);
			slot->tts_values[2] = PointerGetDatum(tmpbuf);
			slot->tts_isnull[2] = false;
		}
		else if (cur_token > 255 && (cur_token < ICONST || cur_token > NOT_EQUALS))
		{
			size_t	len = strlen(lvalp->core_yystype.str);

			if (len > tmpbuflen)
			{
				tmpbuf = repalloc(tmpbuf, len + VARHDRSZ);
				tmpbuflen = len;
			}
			memcpy(VARDATA(tmpbuf), lvalp->core_yystype.str, len);
			SET_VARSIZE(tmpbuf, VARHDRSZ + len);
			slot->tts_values[2] = PointerGetDatum(tmpbuf);
			slot->tts_isnull[2] = false;
		}
		ExecStoreVirtualTuple(slot);
		tuplestore_puttupleslot(tstore,slot);
	}

	/* Clean up (release memory) */
	scanner_finish(yyscanner);
}

PG_FUNCTION_INFO_V1(lexer_test);
PG_FUNCTION_INFO_V1(lexer);

Datum
lexer_test(PG_FUNCTION_ARGS)
{
	const char *str = PG_GETARG_CSTRING(0);

	lex_str(str);

	PG_RETURN_VOID();
}

Datum
lexer(PG_FUNCTION_ARGS)
{
	const char   *str = PG_GETARG_CSTRING(0);
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext oldcontext;
	Tuplestorestate *tupstore;
	TupleTableSlot *slot;
	TupleDesc	tupdesc;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("failed to determine result record format")));

	if (tupdesc->natts != 3 ||
		TupleDescAttr(tupdesc,0)->atttypid != INT4OID ||
		TupleDescAttr(tupdesc,1)->atttypid != INT4OID ||
		TupleDescAttr(tupdesc,2)->atttypid != TEXTOID)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("incorrect result record format")));

	oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

	/* make sure we have a persistent copy of the result tupdesc */
	tupdesc = CreateTupleDescCopy(tupdesc);

	/* initialize our tuplestore in long-lived context */
	tupstore =
		tuplestore_begin_heap(rsinfo->allowedModes & SFRM_Materialize_Random,
							  false, work_mem);

	slot = MakeSingleTupleTableSlot(tupdesc);

	MemoryContextSwitchTo(oldcontext);

	lex_str_project(str, slot, tupstore);

	ExecDropSingleTupleTableSlot(slot);

	/* let the caller know we're sending back a tuplestore */
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	return (Datum) 0;
}
