/*
 * icu_search.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2023. See LICENSE.md
 */

#include "icu_ext.h"

/* Postgres includes */
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"

/* ICU includes */
#include "unicode/ucol.h"
#include "unicode/usearch.h"

PG_FUNCTION_INFO_V1(icu_strpos);
PG_FUNCTION_INFO_V1(icu_strpos_coll);
PG_FUNCTION_INFO_V1(icu_replace);
PG_FUNCTION_INFO_V1(icu_replace_coll);

/*
 * Given @str in the database encoding and @str_utf16 its UTF-16
 * representation, translate the character position @u16_pos (expressed in
 * UTF-16 code units and 0-based) to a character position in @str.
 * It differs from @u16_pos if @str_utf16 contains surrogate pairs.
 *
 * if @p_str null, make it point to the first byte
 * corresponding to @pos in @str
 */
static int32_t
translate_char_pos(const char* str,
				   int32_t str_len,
				   const UChar* str_utf16,
				   int32_t u16_len, 		/* in 16-bit code units */
				   int32_t u16_pos,
				   const char **p_str)
{
	UChar32 c;
	int32_t u16_idx = 0;
	int32_t out_pos = 0;

	if (GetDatabaseEncoding() == PG_UTF8)
	{
		int32_t u8_offset = 0;
		/* for UTF-8, use ICU macros instead of calling pg_mblen() */
		while (u16_idx < u16_pos)
		{
			U16_NEXT(str_utf16, u16_idx, u16_len, c);
			U8_NEXT(str, u8_offset, str_len, c);
			out_pos++;
		}
		if (p_str != NULL)
			*p_str = str + u8_offset;
	}
	else if (pg_encoding_max_length(GetDatabaseEncoding()) == 1)
	{
		/*
		 * for mono-byte encodings, assume a 1:1 mapping with UTF-16
		 * code units, since they should not contain characters
		 * outside of the BMP.
		 */
		out_pos = u16_pos;
		if (p_str != NULL)
			*p_str = str + out_pos;
	}
	else
	{
		/* for non-UTF-8 multi-byte encodings, use pg_mblen() */
		while (u16_idx < u16_pos)
		{
			U16_NEXT(str_utf16, u16_idx, u16_len, c);
			str += pg_mblen(str);
			out_pos++;
		}
		if (p_str != NULL)
			*p_str = str;
	}
	return out_pos;
}

/*
 * Do the bulk of the work for icu_strpos and icu_strpos_coll.
 * Return values:
 *   0: not found
 *  >0: the 1-based position of txt2 into txt1
 */
static int32_t
internal_strpos(text *txt1, text *txt2, UCollator *collator)
{
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	int32_t len2 = VARSIZE_ANY_EXHDR(txt2);
	UErrorCode	status = U_ZERO_ERROR;
	UStringSearch *usearch;
	UChar *uchar1, *uchar2;
	int32_t ulen1, ulen2;
	int32_t pos;

	/*
	 * A non-empty substring is never contained by an empty string.
	 */
	if (len1 == 0 && len2 != 0)
		return 0;

	/*
	 * An empty substring is always found at the first character (even
	 * inside an empty string), to be consistent with strpos() in
	 * core.
	 */
	if (len2 == 0)
	  return 1;

	ulen1 = icu_to_uchar(&uchar1, VARDATA_ANY(txt1), len1);
	ulen2 = icu_to_uchar(&uchar2, VARDATA_ANY(txt2), len2);

	usearch = usearch_openFromCollator(uchar2, /* needle */
									  ulen2,
									  uchar1, /* haystack */
									  ulen1,
									  collator,
									  NULL,
									  &status);
	if (U_FAILURE(status))
		elog(ERROR, "failed to start search: %s", u_errorName(status));
	else
	{
		pos = usearch_first(usearch, &status);
		if (!U_FAILURE(status) && pos != USEARCH_DONE)
		{
			/*
			 * pos is in UTF-16 code units, with surrogate pairs counting
			 * as two, so we need a non-trivial translation to the corresponding
			 * position in the original string.
			 */
			pos = translate_char_pos(VARDATA_ANY(txt1), len1, uchar1, ulen1, pos, NULL);
		}
		else
			pos = -1;

	}

	pfree(uchar1);
	pfree(uchar2);
	usearch_close(usearch);

	if (U_FAILURE(status))
		elog(ERROR, "failed to perform ICU search: %s", u_errorName(status));

	/* return 0 if not found or the 1-based position of txt2 inside txt1 */
	return pos + 1;
}

/*
 * Equivalent of strpos(haystack, needle) using ICU search
 */
Datum
icu_strpos(PG_FUNCTION_ARGS)
{
    UCollator *collator = ucollator_from_coll_id(PG_GET_COLLATION());

	PG_RETURN_INT32(internal_strpos(PG_GETARG_TEXT_PP(0), /* haystack */
									PG_GETARG_TEXT_PP(1), /* needle */
									collator));
}

/*
 * Equivalent of strpos(haystack, needle) using ICU search
 */
Datum
icu_strpos_coll(PG_FUNCTION_ARGS)
{
	const char	*collname = text_to_cstring(PG_GETARG_TEXT_PP(2));
	UCollator	*collator = NULL;
	UErrorCode	status = U_ZERO_ERROR;
	int32_t pos;

	collator = ucol_open(collname, &status);
	if (!collator || U_FAILURE(status)) {
		elog(ERROR, "failed to open collation: %s", u_errorName(status));
	}

	pos = internal_strpos(PG_GETARG_TEXT_PP(0), /* haystack */
						  PG_GETARG_TEXT_PP(1), /* needle */
						  collator);

	ucol_close(collator);

	PG_RETURN_INT32(pos);
}



/*
 * Search for @txt2 in @txt1 with the ICU @collator and replace the
 * matched substrings with @txt3.
 *
 * The replacement text is always txt3, but the replaced text may not
 * be exactly txt2, and its length in bytes may differ too, depending on
 * the collation rules. For example in utf-8 with an accent-insensitive
 * collation, {LATIN SMALL LETTER E WITH ACUTE} (2 bytes) will match
 * {LATIN SMALL LETTER E} (1 byte).
 */

static text *
internal_str_replace(text *txt1, /* not const because it may be returned */
					 const text *txt2, /* search for txt2 with collator */
					 const text *txt3, /* replace the matched substrings by txt3 */
					 UCollator *collator)
{
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	int32_t len2 = VARSIZE_ANY_EXHDR(txt2);
	int32_t len3 = VARSIZE_ANY_EXHDR(txt3);
	UErrorCode	status = U_ZERO_ERROR;
	UStringSearch *usearch;
	UChar *uchar1, *uchar2;
	int32_t ulen1, ulen2;		/* in utf-16 units */
	text *result;
	int32_t pos;
	StringInfoData resbuf;

	if (len1 == 0 || len2 == 0)
		return txt1;

	ulen1 = icu_to_uchar(&uchar1, VARDATA_ANY(txt1), len1);
	ulen2 = icu_to_uchar(&uchar2, VARDATA_ANY(txt2), len2);

	usearch = usearch_openFromCollator(uchar2, /* needle */
									  ulen2,
									  uchar1, /* haystack */
									  ulen1,
									  collator, 
									  NULL,
									  &status);

	/* "nana" in "nananana" must be found 2 times, not 3 times. */
	usearch_setAttribute(usearch, USEARCH_OVERLAP, USEARCH_OFF, &status);

	pos = usearch_first(usearch, &status);

	if (U_FAILURE(status))
		elog(ERROR, "failed to perform ICU search: %s", u_errorName(status));

	if (pos != USEARCH_DONE)
	{
		const char *txt1_currptr;
		const char* txt1_startptr = VARDATA_ANY(txt1);
		initStringInfo(&resbuf);

		/* initialize the output string with the segment before the first match */
		translate_char_pos(txt1_startptr,
						   len1,
						   uchar1,
						   ulen1,
						   pos,
						   &txt1_currptr);

		appendBinaryStringInfo(&resbuf,
							   txt1_startptr,
							   txt1_currptr - txt1_startptr);

		/* append the replacement text */
		appendBinaryStringInfo(&resbuf, VARDATA_ANY(txt3), len3);

		/* skip the replaced text in txt1 */
		translate_char_pos(
			txt1_currptr,
			len1 - (txt1_currptr - txt1_startptr),
			uchar1 + pos,
			usearch_getMatchedLength(usearch),
			usearch_getMatchedLength(usearch),
			&txt1_currptr);

		do {
			int32_t previous_pos = pos + usearch_getMatchedLength(usearch);

			CHECK_FOR_INTERRUPTS();
			pos = usearch_next(usearch, &status);
			if (U_FAILURE(status))
				break;

			if (pos != USEARCH_DONE)
			{
				const char *txt1_nextptr;

				/* copy the segment before the match */
				translate_char_pos(
					txt1_currptr,
					len1 - (txt1_currptr - txt1_startptr),
					uchar1 + previous_pos,
					len1 - previous_pos,
					pos - previous_pos,
					&txt1_nextptr);

				appendBinaryStringInfo(&resbuf,
									   txt1_currptr,
									   txt1_nextptr - txt1_currptr);

				/* compute the length of the replaced text in txt1 */
				translate_char_pos(
					txt1_nextptr,
					len1 - (txt1_nextptr - txt1_startptr),
					uchar1 + pos,
					usearch_getMatchedLength(usearch),
					usearch_getMatchedLength(usearch),
					&txt1_currptr);

				/* append the replacement text */
				appendBinaryStringInfo(&resbuf, VARDATA_ANY(txt3), len3);
			}

		} while (pos != USEARCH_DONE);

		/* copy the segment after the last match */
		if (len1 - (txt1_currptr - txt1_startptr) > 0)
		{
			appendBinaryStringInfo(&resbuf,
								   txt1_currptr,
								   len1 - (txt1_currptr - txt1_startptr));
		}

		result = cstring_to_text_with_len(resbuf.data, resbuf.len);
		pfree(resbuf.data);
	}
	else
	{
		/*
		 * The substring is not found: return the original string
		 */
		result = txt1;
	}

	pfree(uchar1);
	pfree(uchar2);

	if (usearch != NULL)
		usearch_close(usearch);

	if (U_FAILURE(status))
		elog(ERROR, "failed to perform ICU search: %s", u_errorName(status));

	return result;
}

Datum
icu_replace(PG_FUNCTION_ARGS)
{
    UCollator *collator = ucollator_from_coll_id(PG_GET_COLLATION());
	text *string;

	string = internal_str_replace(
		PG_GETARG_TEXT_PP(0), /* haystack */
		PG_GETARG_TEXT_PP(1), /* needle */
		PG_GETARG_TEXT_PP(2), /* replacement */
		collator);

	PG_RETURN_TEXT_P(string);
}

Datum
icu_replace_coll(PG_FUNCTION_ARGS)
{
	const char	*collname = text_to_cstring(PG_GETARG_TEXT_PP(3));
	UCollator	*collator = NULL;
	UErrorCode	status = U_ZERO_ERROR;

	collator = ucol_open(collname, &status);
	if (!collator || U_FAILURE(status)) {
		elog(ERROR, "failed to open collation: %s", u_errorName(status));
	}

	PG_RETURN_TEXT_P(
		internal_str_replace(
			PG_GETARG_TEXT_PP(0), /* haystack */
			PG_GETARG_TEXT_PP(1), /* needle */
			PG_GETARG_TEXT_PP(2), /* replacement */
			collator)
		);
}
