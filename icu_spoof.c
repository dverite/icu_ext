/*
 * icu_spoof.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2023. See LICENSE.md
 */

#include "icu_ext.h"

#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"

#include "unicode/uspoof.h"

PG_FUNCTION_INFO_V1(icu_confusable_string_skeleton);
PG_FUNCTION_INFO_V1(icu_spoof_check);
PG_FUNCTION_INFO_V1(icu_confusable_strings_check);

/*
 * Get the "skeleton" for an input string.
 * Two strings are confusable if their skeletons are identical.
 */
Datum
icu_confusable_string_skeleton(PG_FUNCTION_ARGS)
{
	text *txt1 = PG_GETARG_TEXT_PP(0);
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	UErrorCode status = U_ZERO_ERROR;
	USpoofChecker *sc;
	int32_t ulen1, ulen_skel, result_len;
	UChar *uchar1, *uchar_skel;
	char *result;

	sc = uspoof_open(&status);
	if (!sc)
		elog(ERROR, "ICU uspoof_open failed");
	ulen1 = icu_to_uchar(&uchar1, text_to_cstring(txt1), len1);

	// maximum of equal length sounds like a sane guess for the first try
	ulen_skel = ulen1;
	uchar_skel = (UChar*) palloc((ulen_skel)*sizeof(UChar));
	ulen_skel = uspoof_getSkeleton(sc, 0, uchar1, ulen1, uchar_skel, ulen_skel, &status);

	if (U_FAILURE(status) && status == U_BUFFER_OVERFLOW_ERROR) {
		// try again with a properly sized buffer
		status = U_ZERO_ERROR;

		pfree(uchar_skel);
		uchar_skel = (UChar*) palloc((ulen_skel)*sizeof(UChar));
		ulen_skel = uspoof_getSkeleton(sc, 0, uchar1, ulen1, uchar_skel, ulen_skel, &status);
	}

	uspoof_close(sc);

	if (U_FAILURE(status))
		elog(ERROR, "ICU uspoof_getSkeleton failed: %s", u_errorName(status));

	result_len = icu_from_uchar(&result, uchar_skel, ulen_skel);
	PG_RETURN_TEXT_P(cstring_to_text_with_len(result, result_len));
}

/*
 * Check whether the input string is likely to be an attempt at
 * confusing a reader.
 */
Datum
icu_spoof_check(PG_FUNCTION_ARGS)
{
	text *txt1 = PG_GETARG_TEXT_PP(0);
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	UErrorCode status = U_ZERO_ERROR;
	USpoofChecker *sc;
	int32_t bitmask;
	int32_t ulen1;
	UChar *uchar1;

	sc = uspoof_open(&status);
	if (!sc)
		elog(ERROR, "ICU uspoof_open failed");
	ulen1 = icu_to_uchar(&uchar1, text_to_cstring(txt1), len1);
	bitmask = uspoof_check(sc, uchar1, ulen1, NULL, &status);
	uspoof_close(sc);

	if (U_FAILURE(status))
		elog(ERROR, "ICU uspoof_areConfusable failed: %s", u_errorName(status));

	PG_RETURN_BOOL(bitmask != 0);
}

/*
 * Check whether the two input strings are visually confusable with
 * each other.
 */
Datum
icu_confusable_strings_check(PG_FUNCTION_ARGS)
{
	text *txt1 = PG_GETARG_TEXT_PP(0);
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	text *txt2 = PG_GETARG_TEXT_PP(1);
	int32_t len2 = VARSIZE_ANY_EXHDR(txt2);
	int32_t ulen1, ulen2;
	UChar *uchar1, *uchar2;
	USpoofChecker *sc;
	UErrorCode	status = U_ZERO_ERROR;
	int32_t bitmask;

	sc = uspoof_open(&status);
	if (!sc)
		elog(ERROR, "ICU uspoof_open failed");
	ulen1 = icu_to_uchar(&uchar1, text_to_cstring(txt1), len1);
	ulen2 = icu_to_uchar(&uchar2, text_to_cstring(txt2), len2);
	bitmask = uspoof_areConfusable(sc, uchar1, ulen1, uchar2, ulen2, &status);
	uspoof_close(sc);

	if (U_FAILURE(status))
		elog(ERROR, "ICU uspoof_areConfusable failed: %s", u_errorName(status));

	PG_RETURN_BOOL(bitmask != 0);
}
