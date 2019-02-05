/*
 * icu_search.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2019. See LICENSE.md
 */

/* Postgres includes */
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"

/* ICU includes */
#include "unicode/ucol.h"
#include "unicode/usearch.h"


PG_FUNCTION_INFO_V1(icu_strpos);

/*
 * Given @str in the database encoding and @str_utf16 its UTF-16
 * representation, translate the character position @pos (expressed in
 * UTF-16 code units and 0-based) to an offset into the original string.
 */
static
int32_t
translate_char_pos(const char* str,
				   int32_t str_len,
				   const UChar* str_utf16,
				   int32_t u16_len, 		/* in 16-bit code units */
				   int32_t u16_pos)
{
	UChar32 c;
	int32_t u16_offset = 0;
	int32_t offset = 0;

	if (GetDatabaseEncoding() == PG_UTF8)
	{
		/* for UTF-8, fast path with ICU macros */
		while (u16_offset < u16_pos)
		{
			U16_NEXT(str_utf16, u16_offset, u16_len, c);
			U8_NEXT(str, offset, str_len, c);
		}
	}
	else if (pg_encoding_max_length(GetDatabaseEncoding()) == 1)
	{
		/*
		 * for mono-byte encodings, assume a 1:1 mapping
		 * with UTF-16 code units and no surrogate pairs.
		 */
		offset = u16_pos;
	}
	else
	{
		/* for non-UTF-8 multi-byte encodings, iterate */
		const char* str0 = str;
		while (u16_offset < u16_pos)
		{
			U16_NEXT(str_utf16, u16_offset, u16_len, c);
			str += pg_mblen(str);
		}
		offset = str - str0;
	}
	return offset;
}

/*
 * icu_strpos(haystack, needle, collation)
 *
 */
Datum
icu_strpos(PG_FUNCTION_ARGS)
{
	text	*txt1 = PG_GETARG_TEXT_PP(0);
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	text	*txt2 = PG_GETARG_TEXT_PP(1);
	int32_t len2 = VARSIZE_ANY_EXHDR(txt2);
	const char	*collname = text_to_cstring(PG_GETARG_TEXT_PP(2));
	UCollator	*collator = NULL;
	UErrorCode	status = U_ZERO_ERROR;
	UStringSearch *search = NULL;
	UChar *uchar1, *uchar2;
	int32_t ulen1, ulen2;
	int pos;

	collator = ucol_open(collname, &status);
	if (!collator || U_FAILURE(status)) {
		elog(ERROR, "failed to open collation: %s", u_errorName(status));
	}

	ulen1 = icu_to_uchar(&uchar1, VARDATA_ANY(txt1), len1);
	ulen2 = icu_to_uchar(&uchar2, VARDATA_ANY(txt2), len2);

	search = usearch_openFromCollator(uchar2, /* needle */
									  ulen2,
									  uchar1, /* haystack */
									  ulen1,
									  collator, 
									  NULL,
									  &status);
	if (U_FAILURE(status)) {
		ucol_close(collator);
		elog(ERROR, "failed to start search: %s", u_errorName(status));
	}

	pos = usearch_first(search, &status);

	if (pos >= 0)
	{
		pos = translate_char_pos(VARDATA_ANY(txt2), len2, uchar1, ulen1, pos);
	}

	pfree(uchar1);
	pfree(uchar2);
	ucol_close(collator);
	usearch_close(search);

	/* return 0 if not found or the 1-based position of txt2 inside txt1 */
	return pos + 1;
}
