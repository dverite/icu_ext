/*
 * icu_num.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2020. See LICENSE.md
 */

#include "postgres.h"
#include "access/htup_details.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"
#include "mb/pg_wchar.h"

#include "unicode/ucol.h"
#include "unicode/uloc.h"
#include "unicode/unum.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"

PG_FUNCTION_INFO_V1(icu_number_spellout);

Datum
icu_number_spellout(PG_FUNCTION_ARGS)
{
	float8 number = PG_GETARG_FLOAT8(0);
	const char *locale = text_to_cstring(PG_GETARG_TEXT_PP(1));
	UErrorCode status = U_ZERO_ERROR;
	UChar local_ubuf[256];
	UChar *ubuf = local_ubuf;
	int32_t buf_len = sizeof(local_ubuf)/sizeof(UChar);
	UNumberFormat* nf;
	int32_t real_len;
	char *output;

	nf = unum_open(UNUM_SPELLOUT,
				   NULL, /* pattern */
				   -1,	/* pattern length */
				   locale,
				   NULL, /* parseErr */
				   &status);

	if (U_FAILURE(status))
		elog(ERROR, "unum_open failed: %s", u_errorName(status));

	real_len = unum_formatDouble(nf, number, ubuf, buf_len, NULL, &status);
	if (status == U_BUFFER_OVERFLOW_ERROR) {		/* buffer too small */
		ubuf = palloc((real_len+1)*sizeof(UChar));
		status = U_ZERO_ERROR;
		real_len = unum_formatDouble(nf, number, ubuf, real_len+1, NULL, &status);
	}
	if (U_FAILURE(status))
	{
		unum_close(nf);
		elog(ERROR, "unum_formatDouble failed: %s", u_errorName(status));
	}

	icu_from_uchar(&output, ubuf, real_len);
	unum_close(nf);

	PG_RETURN_TEXT_P(cstring_to_text(output));
}
