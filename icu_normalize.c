/*
 * icu_normalize.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2020. See LICENSE.md
 */

/* Postgres includes */
#include "postgres.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"
#if PG_VERSION_NUM >= 160000
#include "varatt.h"
#endif

/* ICU includes */
#include "unicode/unorm.h"

#include "icu_ext.h"

PG_FUNCTION_INFO_V1(icu_is_normalized);
PG_FUNCTION_INFO_V1(icu_normalize);

typedef enum {
	UNICODE_NFC,
	UNICODE_NFD,
	UNICODE_NFKC,
	UNICODE_NFKD
} norm_form_t;

static norm_form_t
name_to_norm(const char *formstr)
{
	if (pg_strcasecmp(formstr, "NFC") == 0)
		return UNICODE_NFC;
	else if (pg_strcasecmp(formstr, "NFD") == 0)
		return UNICODE_NFD;
	else if (pg_strcasecmp(formstr, "NFKC") == 0)
		return UNICODE_NFKC;
	else if (pg_strcasecmp(formstr, "NFKD") == 0)
		return UNICODE_NFKD;
	else
		elog(ERROR, "invalid normalization form: %s", formstr);
}

static const
UNormalizer2* norm_instance(norm_form_t form)
{
	UErrorCode	status = U_ZERO_ERROR;
	const UNormalizer2 *instance = NULL;

	switch (form)
	{
	case UNICODE_NFC:
		instance = unorm2_getNFCInstance(&status);
		break;
	case UNICODE_NFD:
		instance = unorm2_getNFDInstance(&status);
		break;
	case UNICODE_NFKC:
		instance = unorm2_getNFKCInstance(&status);
		break;
	case UNICODE_NFKD:
		instance = unorm2_getNFKDInstance(&status);
		break;
	}
	if (U_FAILURE(status))
		elog(ERROR, "norm_instance failure: %s", u_errorName(status));
	return instance;
}

/*
 * Return the string (1st arg) with the given Unicode normalization
 * (2nd arg).
 */
Datum
icu_normalize(PG_FUNCTION_ARGS)
{
	text *src_text = PG_GETARG_TEXT_PP(0);
	const char* arg_form = text_to_cstring(PG_GETARG_TEXT_P(1));
	norm_form_t form = name_to_norm(arg_form);
	const UNormalizer2 *instance = norm_instance(form);
	int32_t u_src_length, u_dest_length, effective_length, result_len;
	char *result;
	UChar *u_src, *u_dest;
	UErrorCode	status = U_ZERO_ERROR;

	if (GetDatabaseEncoding() != PG_UTF8)
		elog(ERROR, "non-Unicode database encoding");

	u_src_length = icu_to_uchar(&u_src,
								VARDATA_ANY(src_text),
								VARSIZE_ANY_EXHDR(src_text));

	/*
	 * The result may be expanded by the maximum factor given at:
	 * https://unicode.org/faq/normalization.html#12
	 * (given that the UChar buffer is in UTF-16)
	 */
	switch(form)
	{
	case UNICODE_NFC:
		u_dest_length = u_src_length * 3;
		break;
	case UNICODE_NFD:
		u_dest_length = u_src_length * 4;
		break;
	case UNICODE_NFKC:
	case UNICODE_NFKD:
	default:
		u_dest_length = u_src_length * 18;
		break;
	}

	u_dest = (UChar*) palloc(u_dest_length*sizeof(UChar));

	effective_length = unorm2_normalize(instance,
										u_src,
										u_src_length,
										u_dest,
										u_dest_length,
										&status);
	if (U_FAILURE(status))
		elog(ERROR, "unorm2_normalize failure: %s", u_errorName(status));

	result_len = icu_from_uchar(&result, u_dest, effective_length);
	PG_RETURN_TEXT_P(cstring_to_text_with_len(result, result_len));
}

/*
 * Check if a string (1st arg) is in the given Unicode normal form
 * (2nd arg).
 */
Datum
icu_is_normalized(PG_FUNCTION_ARGS)
{
    text *src_text = PG_GETARG_TEXT_PP(0);
	const char* arg_form = text_to_cstring(PG_GETARG_TEXT_PP(1));
	norm_form_t form = name_to_norm(arg_form);
	UErrorCode	status = U_ZERO_ERROR;
	UChar *u_src;
	int32_t u_src_length;
	UBool is_norm;
	const UNormalizer2 *instance = norm_instance(form);

	if (GetDatabaseEncoding() != PG_UTF8)
		elog(ERROR, "non-Unicode database encoding");

	u_src_length = icu_to_uchar(&u_src,
								VARDATA_ANY(src_text),
								VARSIZE_ANY_EXHDR(src_text));

	is_norm = unorm2_isNormalized(instance, u_src, u_src_length, &status);

	if (U_FAILURE(status))
		elog(ERROR, "unorm2_isNormalized failure: %s", u_errorName(status));

	PG_RETURN_BOOL(is_norm == 1);
}
