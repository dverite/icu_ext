/*
 * icu_transform.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2019. See LICENSE.md
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"
#include "utils/memutils.h"

#include "unicode/uenum.h"
#include "unicode/utrans.h"

PG_FUNCTION_INFO_V1(icu_transforms_list);
PG_FUNCTION_INFO_V1(icu_transform);

/*
 * List the available pre-defined transforms/transliterations.
 */
Datum
icu_transforms_list(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	UErrorCode status = U_ZERO_ERROR;
	UEnumeration *en;
	const char *elt;

	if (SRF_IS_FIRSTCALL())
	{
		funcctx = SRF_FIRSTCALL_INIT();
		en = utrans_openIDs(&status);
		if (U_FAILURE(status))
			elog(ERROR, "utrans_openIDs failed: %s", u_errorName(status));
		funcctx->user_fctx = (void *)en;
	}

	funcctx = SRF_PERCALL_SETUP();
	en = (UEnumeration*) funcctx->user_fctx;

	elt = uenum_next(en, NULL, &status);
	if (U_FAILURE(status))
		elog(ERROR, "uenum_next failed: %s", u_errorName(status));
	if (elt)
	{
		text* item = cstring_to_text(elt);
		SRF_RETURN_NEXT(funcctx, PointerGetDatum(item));
	}
	else
	{
		uenum_close(en);
		SRF_RETURN_DONE(funcctx);
	}

}


/*
 * Cache for the last transformation used.
 * This may come in handy in applications that use several times the same transformation
 */
static UTransliterator *utrans = NULL;
static char *cached_utrans_id = NULL;

/*
 * Main function to apply a tranformation based on UTransliterator.
 * Input:
 * 1st arg: string to transform
 * 2nd arg: name (system identifier) of the transliterator
 */
Datum
icu_transform(PG_FUNCTION_ARGS)
{
	text *arg1 = PG_GETARG_TEXT_PP(0);
	text *arg2 = PG_GETARG_TEXT_PP(1);
	int32_t len1 = VARSIZE_ANY_EXHDR(arg1);
	const char *input_id = text_to_cstring(arg2);
	UErrorCode status = U_ZERO_ERROR;
	int32_t ulen, limit, capacity, start, original_ulen;
	int32_t result_len, in_ulen;
	UChar* utext;
	UChar* trans_id;
	char* result;
	UChar* original;

	bool done = false;

	if (cached_utrans_id != NULL)
	{
		if (strcmp(cached_utrans_id, input_id) != 0)
		{
			pfree(cached_utrans_id);
			cached_utrans_id = NULL;
			utrans_close(utrans);
			utrans = NULL;
		}
	}

	if (utrans == NULL)
	{
		in_ulen = icu_to_uchar(&trans_id, input_id, strlen(input_id));

		utrans = utrans_openU(trans_id,
						  in_ulen,
						  UTRANS_FORWARD,
						  NULL, /* rules. NULL for system transliterators */
						  -1,
						  NULL, /* pointer to parseError. Not used */
						  &status);

		if (U_FAILURE(status) || !utrans)
		{
			elog(ERROR, "utrans_open failed: %s", u_errorName(status));
		}
		else
		{
			cached_utrans_id = MemoryContextStrdup(TopMemoryContext, input_id);
		}
	}

	ulen = icu_to_uchar(&utext, text_to_cstring(arg1), len1);
	/* utext is terminated by a zero UChar that we include in the copy. */
	original = (UChar*) palloc((ulen+1)*sizeof(UChar));
	original_ulen = ulen;
	memcpy(original, utext, (ulen+1)*sizeof(UChar));

	limit = ulen;
	capacity = ulen + 1;
	start = 0;
	/*
	 * utrans_transUChars() updates the string in-place, stopping if
	 * it would go over `capacity`.
	 * The following loop doubles the capacity and restarts from
	 * scratch with a clean copy of the source if the buffer was
	 * too small.
	 * Although it looks like we could use `start` and `limit`
	 * to reallocate and make the transliteration continue from
	 * where it stopped, in practice this does not appear to work.
	 * The documentation is quite unclear about this function.
	 */
	do {
		status = U_ZERO_ERROR;
		utrans_transUChars(utrans,
						   utext,
						   &ulen,
						   capacity,
						   start,		/* beginning index */
						   &limit,
						   &status);
		if (U_FAILURE(status))
		{
			if (status != U_BUFFER_OVERFLOW_ERROR)
			{
				elog(ERROR, "utrans_transUChars failed: %s", u_errorName(status));
			}
			else
			{
				pfree(utext);
				capacity = capacity * 2;
				utext = (UChar*) palloc(capacity*sizeof(UChar));
				/* restore the original text in the enlarged buffer */
				ulen = original_ulen;
				limit = ulen;
				memcpy(utext, original, (ulen+1)*sizeof(UChar));
			}
		}
		else
			done = true;
	} while (!done);

	result_len = icu_from_uchar(&result, utext, ulen);
	PG_RETURN_TEXT_P(cstring_to_text_with_len(result, result_len));
}
