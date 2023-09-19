/*
 * icu_timestamptz.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2023. See LICENSE.md
 */

/* Postgres includes */
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "pgtime.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/pg_locale.h"
#include "utils/date.h"
#include "utils/datetime.h"

/* ICU includes */
#include "unicode/ucal.h"
#include "unicode/udat.h"
#include "unicode/ustring.h"


#include "icu_ext.h"

PG_FUNCTION_INFO_V1(icu_diff_timestamps);
PG_FUNCTION_INFO_V1(icu_diff_timestamps_default_locale);
PG_FUNCTION_INFO_V1(icu_timestamptz_in);
PG_FUNCTION_INFO_V1(icu_timestamptz_out);


Datum
icu_diff_timestamps(PG_FUNCTION_ARGS)
{
	TimestampTz pg_tstz1 = PG_GETARG_TIMESTAMPTZ(0);
	TimestampTz pg_tstz2 = PG_GETARG_TIMESTAMPTZ(1);
	/*
	  delta interval,
	  locale text
	*/
	long diff = TimestampDifferenceMilliseconds(pg_tstz1, pg_tstz2);
	Interval *interv = (Interval*)palloc0(sizeof(Interval));
	interv->time = diff*1000;	/* TimeOffset has units of microseconds */
	/*
	  fsec_t fsec = 0;
	  tm2interval(&tm, fsec, &interv);
	*/
	PG_RETURN_INTERVAL_P(interv);
}

Datum
icu_diff_timestamps_default_locale(PG_FUNCTION_ARGS)
{
	TimestampTz pg_tstz1 = PG_GETARG_TIMESTAMPTZ(0);
	TimestampTz pg_tstz2 = PG_GETARG_TIMESTAMPTZ(1);

	Interval *interv = (Interval*)palloc0(sizeof(Interval));
	interv->time = pg_tstz2 - pg_tstz1;	/* TimeOffset has units of microseconds */
	PG_RETURN_INTERVAL_P(interv);
}


/* Convert a Postgres timestamp into an ICU timestamp */
static UDate
ts_to_udate(TimestampTz pg_tstz)
{
	/*
	 *  ICU's UDate is a number of milliseconds since the Unix Epoch,
	 *  (1970-01-01, 00:00 UTC), stored as a double.
	 *  Postgres' TimestampTz is a number of microseconds since 2000-01-01 00:00 UTC,
	 *  stored as an int64.
	 * The code below translates directly between the epochs
	 * Ideally these implementation details should not be relied upon here
	 * but there doesn't seem to be a function udat_xxx() to set the date
	 * from an epoch.
	 * Alternatively we could extract the year/month/..etc.. fields from pg_tstz
	 * and set them one by one in a gregorian calendar with ucal_set(cal, field, value),
	 * and then obtain the UDate with ucal_getMillis(cal), but it would be slower.
	 */

	return (UDate)(10957.0*86400*1000 + pg_tstz/1000);
}

/* Convert an ICU timestamp into a Postgres timestamp */
static TimestampTz
udate_to_ts(const UDate ud)
{
	/*
	 * Input: number of milliseconds since 1970-01-01 UTC
	 * Output: number of microseconds since 2000-01-01 UTC
	 * See the comment above in ts_to_udate about the translation
	 */
	return (TimestampTz)(ud*1000 - 10957LL*86400*1000*1000);
}

/* icu_timestamptz_out()
 * Convert a timestamp to external form.
 */
Datum
icu_timestamptz_out(PG_FUNCTION_ARGS)
{
	TimestampTz dt = PG_GETARG_TIMESTAMPTZ(0);
	char	   *result;
	int			tz;
	struct pg_tm tt,
			   *tm = &tt;
	fsec_t		fsec;
	const char *tzn;
	char		buf[MAXDATELEN + 1];

	if (TIMESTAMP_NOT_FINITE(dt)) {
		EncodeSpecialTimestamp(dt, buf);
		result = pstrdup(buf);
		PG_RETURN_CSTRING(result);
	}
	else if (timestamp2tm(dt, &tz, tm, &fsec, &tzn, NULL) == 0)
	{
		UErrorCode status = U_ZERO_ERROR;
		UDateFormat* df = NULL;
		UDate udate = ts_to_udate(dt);
		const char *locale = NULL;
		UChar *output_pattern = NULL;
		int32_t pattern_length = -1;
		UChar* tzid;
		int32_t tzid_length;
		UDateFormatStyle style = icu_ext_timestamptz_style;
		const char *pg_tz_name = pg_get_timezone_name(session_timezone);


		if (icu_ext_timestamptz_format != NULL)
		{
			if (icu_ext_timestamptz_format[0] != '\0' && icu_ext_timestamptz_style == UDAT_NONE)
			{
				pattern_length = icu_to_uchar(&output_pattern,
											  icu_ext_timestamptz_format,
											  strlen(icu_ext_timestamptz_format));
			}
		}

		if (icu_ext_default_locale != NULL && icu_ext_default_locale[0] != '\0')
		{
			locale = icu_ext_default_locale;
		}

		/* use PG current timezone, hopefully compatible with ICU */
		tzid_length = icu_to_uchar(&tzid,
								   pg_tz_name,
								   strlen(pg_tz_name));

		/* if UDAT_PATTERN is passed, it must for both timeStyle and dateStyle */
		df = udat_open(output_pattern ? UDAT_PATTERN : style, /* timeStyle */
					   output_pattern ? UDAT_PATTERN : style, /* dateStyle */
					   locale,		 /* NULL for the default locale */
					   tzid,			/* tzID (NULL=default). */
					   tzid_length,		/* tzIDLength */
					   output_pattern,		/* pattern */
					   pattern_length,			/* patternLength */
					   &status);
		if (U_FAILURE(status))
			elog(ERROR, "udat_open failed with code %d\n", status);
		{
			/* Try first to convert into a buffer on the stack, and
			   palloc() it only if udat_format says it's too small */
			UChar local_buf[MAXDATELEN];

			int32_t u_buffer_size = udat_format(df, udate,
												local_buf, sizeof(local_buf)/sizeof(UChar),
												NULL, &status);

			if(status == U_BUFFER_OVERFLOW_ERROR)
			{
				UChar* u_buffer;
				status = U_ZERO_ERROR;
				u_buffer = (UChar*) palloc(u_buffer_size*sizeof(UChar));
				udat_format(df, udate, u_buffer, u_buffer_size, NULL, &status);
				icu_from_uchar(&result, u_buffer, u_buffer_size);
			}
			else
			{
				icu_from_uchar(&result, local_buf, u_buffer_size);
			}
		}
		if (df)
			udat_close(df);
		PG_RETURN_CSTRING(result);
	}
	else
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("timestamp out of range")));

}

/* icu_timestamptz_in()
 * Convert a string to internal form.
 */
Datum
icu_timestamptz_in(PG_FUNCTION_ARGS)
{
	char *input_string = PG_GETARG_CSTRING(0);
	int32_t pattern_length = -1;
	UChar *u_ts_string;
	int32_t u_ts_length;
	UDateFormat* df = NULL;
	UDate udat;
	UDateFormatStyle style = icu_ext_timestamptz_style;
	UErrorCode status = U_ZERO_ERROR;
	UChar *input_pattern = NULL;
	const char *locale = NULL;
	int32_t parse_pos = 0;
	UChar* tzid;
	int32_t tzid_length;
	const char *pg_tz_name = pg_get_timezone_name(session_timezone);

	if (icu_ext_timestamptz_format != NULL)
	{
		if (icu_ext_timestamptz_format[0] != '\0' && style == UDAT_NONE)
		{
			pattern_length = icu_to_uchar(&input_pattern,
										  icu_ext_timestamptz_format,
										  strlen(icu_ext_timestamptz_format));
		}
	}

	u_ts_length = icu_to_uchar(&u_ts_string, input_string, strlen(input_string));

	if (icu_ext_default_locale != NULL && icu_ext_default_locale[0] != '\0')
	{
		locale = icu_ext_default_locale;
	}

	/* use PG current timezone, hopefully compatible with ICU */
	tzid_length = icu_to_uchar(&tzid,
							   pg_tz_name,
							   strlen(pg_tz_name));

	/* if UDAT_PATTERN is used, we must pass it for both timeStyle and dateStyle */
	df = udat_open(input_pattern ? UDAT_PATTERN : style,	 /* timeStyle */
				   input_pattern ? UDAT_PATTERN : style, /* dateStyle */
				   locale,
				   tzid,		/* tzID */
				   tzid_length,			/* tzIDLength */
				   input_pattern,
				   pattern_length,
				   &status);
	if (U_FAILURE(status))
	{
		udat_close(df);
		elog(ERROR, "udat_open failed: %s\n", u_errorName(status));
	}

	udat_setLenient(df, false);	/* strict parsing */

	udat = udat_parse(df,
					  u_ts_string,
					  u_ts_length,
					  &parse_pos,
					  &status);
	udat_close(df);

	if (U_FAILURE(status))
		elog(ERROR, "udat_parse failed: %s\n", u_errorName(status));

	PG_RETURN_TIMESTAMPTZ(udate_to_ts(udat));
}

