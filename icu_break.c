/*
 * icu_break.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2025. See LICENSE.md
 */


#include "icu_ext.h"

#include "access/htup_details.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"
#include "mb/pg_wchar.h"

#include "unicode/ubrk.h"
#include "unicode/ucnv.h"
#include "unicode/ucol.h"
#include "unicode/uloc.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"

/*
 * PG set-returning functions exposing ICU's BreakIterator API for
 * characters, words, line-wrapping, sentences
 */

PG_FUNCTION_INFO_V1(icu_character_boundaries);
PG_FUNCTION_INFO_V1(icu_word_boundaries);
PG_FUNCTION_INFO_V1(icu_sentence_boundaries);
PG_FUNCTION_INFO_V1(icu_line_boundaries);


struct ubreak_ctxt {
	UBreakIterator *iter;
	UText* ut;
	char* source_text;
	UChar* cnv_text;		/* unused and NULL if the database encoding is UTF-8 */
	int32_t len;
	TupleDesc tupdesc;
};

/*
 * Initialize the context to iterate on the input.
 * arg1=input string, arg2=locale
 * The main difference between break iterators is:
 * - UBRK_CHARACTER: return SETOF text
 * - others: return SETOF (int,text)
 */
static void
init_srf_first_call(UBreakIteratorType break_type, PG_FUNCTION_ARGS)
{
	MemoryContext oldcontext;
	const char *brk_locale;
	UErrorCode	status = U_ZERO_ERROR;
	FuncCallContext *funcctx;
	struct ubreak_ctxt *ctxt;

	funcctx = SRF_FIRSTCALL_INIT();

	/*
	 * Switch to memory context appropriate for multiple function calls
	 */
	oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

	ctxt = palloc(sizeof(struct ubreak_ctxt));

	if (break_type != UBRK_CHARACTER)
	{
		TupleDesc	tupdesc;
		/* Construct tuple descriptor */
		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function returning record called in context that cannot accept type record")));
		ctxt->tupdesc = BlessTupleDesc(tupdesc);
	}
	else
		ctxt->tupdesc = NULL;


	/* Use the UTF-8 ICU functions if our string is in UTF-8 */
	if (GetDatabaseEncoding() == PG_UTF8)
	{
		text *txt = PG_GETARG_TEXT_PP(0);
		ctxt->len = VARSIZE_ANY_EXHDR(txt);
		ctxt->source_text = (char*)palloc(ctxt->len);
		ctxt->cnv_text = NULL;
		memcpy(ctxt->source_text, VARDATA_ANY(txt), ctxt->len);

		ctxt->ut = utext_openUTF8(NULL,
								  ctxt->source_text,
								  ctxt->len,
								  &status);		
		if (U_FAILURE(status))
			elog(ERROR, "utext_openUTF8() failed: %s", u_errorName(status));
	}
	else
	{
		text *input = PG_GETARG_TEXT_PP(0);
		/* database encoding to UChar buffer */
		ctxt->len = icu_to_uchar(&ctxt->cnv_text,
								 text_to_cstring(input),
								 VARSIZE_ANY_EXHDR(input));

		ctxt->ut = utext_openUChars(NULL,
									ctxt->cnv_text,
									ctxt->len,
									&status);
		if (U_FAILURE(status))
			elog(ERROR, "utext_openUChars() failed: %s", u_errorName(status));
	}


	funcctx->user_fctx = (void *) ctxt;
	brk_locale = text_to_cstring(PG_GETARG_TEXT_PP(1));
	MemoryContextSwitchTo(oldcontext);

	ctxt->iter = ubrk_open(break_type, brk_locale, NULL, 0, &status);
	if (U_FAILURE(status))
	{
		utext_close(ctxt->ut);
		elog(ERROR, "ubrk_open failed: %s", u_errorName(status));
	}

	ubrk_setUText(ctxt->iter, ctxt->ut, &status);
	if (U_FAILURE(status))
	{
		ubrk_close(ctxt->iter);
		utext_close(ctxt->ut);
		elog(ERROR, "ubrk_setText() failed: %s", u_errorName(status));
	}
}

/*
 * Return substrings (SETOF text). In general, they're are
 * one-character only but CRLF are returned in one piece,
 * and combining+base characters are also pieced together.
 * In this respect it differs from regexp_split_to_table(text, '')
 */
Datum
icu_character_boundaries(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	int32_t pos0, pos;
	struct ubreak_ctxt *ctxt;

	if (SRF_IS_FIRSTCALL())
	{
		init_srf_first_call(UBRK_CHARACTER, fcinfo);
	}

	funcctx = SRF_PERCALL_SETUP();
	ctxt = (struct ubreak_ctxt*) funcctx->user_fctx;

	if (ctxt->len == 0)
		SRF_RETURN_DONE(funcctx); /* no result */

	pos0 = ubrk_current(ctxt->iter);
	pos = ubrk_next(ctxt->iter);

	if (pos != UBRK_DONE)
	{
		text *item;
		if (ctxt->source_text != NULL)
			item = cstring_to_text_with_len(ctxt->source_text+pos0, pos-pos0);
		else
		{
			char *buf;
			/* convert UChar to a buffer in the database encoding */
			int32_t len = icu_from_uchar(&buf, ctxt->cnv_text+pos0, pos-pos0);
			item = cstring_to_text_with_len(buf, len);
		}
		SRF_RETURN_NEXT(funcctx, PointerGetDatum(item));
	}
	else	/* end of SRF iteration */
	{
		ubrk_close(ctxt->iter);
		utext_close(ctxt->ut);
		SRF_RETURN_DONE(funcctx);
	}
}


/*
 * Return (tag,content) tuples
 */

static Datum
icu_boundaries_internal(UBreakIteratorType break_type, PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	int32_t pos0, pos1;
	struct ubreak_ctxt *ctxt;

	if (SRF_IS_FIRSTCALL())
	{
		init_srf_first_call(break_type, fcinfo);
	}

	funcctx = SRF_PERCALL_SETUP();
	ctxt = (struct ubreak_ctxt*) funcctx->user_fctx;

	if (ctxt->len == 0)
		SRF_RETURN_DONE(funcctx); /* no result */

	pos0 = ubrk_current(ctxt->iter);
	do {
		pos1 = ubrk_next(ctxt->iter);

		if (pos1 != UBRK_DONE)
		{
			Datum	values[2];
			bool	nulls[2];
			HeapTuple tuple;
			text *item;

			if (ctxt->source_text != NULL)
			{
				item = cstring_to_text_with_len(ctxt->source_text + pos0, pos1-pos0);
			}
			else
			{
				char *buf;
				/* convert back UChar to a buffer in the database encoding */
				int32_t len = icu_from_uchar(&buf, ctxt->cnv_text + pos0, pos1-pos0);
				item = cstring_to_text_with_len(buf, len);
			}

			values[0] = Int32GetDatum(ubrk_getRuleStatus(ctxt->iter));
			nulls[0] = false;
			values[1] = PointerGetDatum(item);
			nulls[1] = false;

			tuple = heap_form_tuple(ctxt->tupdesc, values, nulls);
			SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
		}

	} while (pos1 != UBRK_DONE);

	/* end of SRF iteration */
	ubrk_close(ctxt->iter);
	utext_close(ctxt->ut);
	SRF_RETURN_DONE(funcctx);
}

Datum
icu_word_boundaries(PG_FUNCTION_ARGS)
{
	return icu_boundaries_internal(UBRK_WORD, fcinfo);
}

Datum
icu_line_boundaries(PG_FUNCTION_ARGS)
{
	return icu_boundaries_internal(UBRK_LINE, fcinfo);
}

Datum
icu_sentence_boundaries(PG_FUNCTION_ARGS)
{
	return icu_boundaries_internal(UBRK_SENTENCE, fcinfo);
}
