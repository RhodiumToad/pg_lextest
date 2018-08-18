
#include "postgres.h"
#include "fmgr.h"

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
		/*elog(INFO, "token %d", cur_token);*/
		if (!cur_token)
			break;
	}

	/* Clean up (release memory) */
	scanner_finish(yyscanner);
}

PG_FUNCTION_INFO_V1(lexer_test);

Datum
lexer_test(PG_FUNCTION_ARGS)
{
	const char *str = PG_GETARG_CSTRING(0);

	lex_str(str);

	PG_RETURN_VOID();
}
