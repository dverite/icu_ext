/*
 * icu_calendar.c
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

PG_FUNCTION_INFO_V1(icu_format_date_locale);
PG_FUNCTION_INFO_V1(icu_format_date_default_locale);
PG_FUNCTION_INFO_V1(icu_parse_date_locale);
PG_FUNCTION_INFO_V1(icu_parse_date_default_locale);
PG_FUNCTION_INFO_V1(icu_date_in);
PG_FUNCTION_INFO_V1(icu_date_out);
PG_FUNCTION_INFO_V1(icu_date_add_days);
PG_FUNCTION_INFO_V1(icu_date_days_add);


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


/*
 * Return a text representation of a PG timestamp given the locale and ICU format.
 * locale==NULL means the default locale.
 */

static Datum
icu_format_date(TimestampTz pg_tstz, text *date_fmt, const char *locale)
{
	const char* icu_date_format = text_to_cstring(date_fmt);
	UErrorCode status = U_ZERO_ERROR;
	char *result;
	int32_t result_len;

	int32_t pattern_length;
	UChar* pattern_buf;

	UDateFormat* df = NULL;
	UDate dat;
	UChar* tzid;
	int32_t tzid_length;
	const char *pg_tz_name = pg_get_timezone_name(session_timezone);
	UDateFormatStyle style;

	if (TIMESTAMP_NOT_FINITE(pg_tstz))
	{
		char buf[MAXDATELEN + 1];

		EncodeSpecialTimestamp(pg_tstz, buf); /* produces [-]infinity */
		result = pstrdup(buf);
		PG_RETURN_TEXT_P(cstring_to_text(result));
	}

	dat = ts_to_udate(pg_tstz);

	style = date_format_style(icu_date_format);
	if (style == UDAT_NONE)
	{
		pattern_length = icu_to_uchar(&pattern_buf, icu_date_format, strlen(icu_date_format));
		style = UDAT_PATTERN;
	}
	else
	{
		pattern_length = -1;
		pattern_buf = NULL;
	}

	tzid_length = icu_to_uchar(&tzid,
							   pg_tz_name, /* or UCAL_UNKNOWN_ZONE_ID, like GMT */
							   strlen(pg_tz_name));

	/* if UDAT_PATTERN is passed, it must for both timeStyle and dateStyle */
	df = udat_open(style,		/* timeStyle */
				   style,		/* dateStyle */
				   locale,		/* NULL for the default locale */
				   tzid,			/* tzID (NULL=default). */
				   tzid_length,			/* tzIDLength */
				   pattern_buf,
				   pattern_length,
				   &status);
	if (U_FAILURE(status))
		elog(ERROR, "udat_open failed with code %d\n", status);

	{
		/* Try first to convert into a buffer on the stack, and
		   palloc() it only if udat_format says it's too small */
		UChar local_buf[MAXDATELEN];

		int32_t u_buffer_size = udat_format(df, dat,
											local_buf, sizeof(local_buf)/sizeof(UChar),
											NULL, &status);

		if (status == U_BUFFER_OVERFLOW_ERROR)
		{
			UChar* u_buffer;
			status = U_ZERO_ERROR;
			u_buffer = (UChar*) palloc(u_buffer_size*sizeof(UChar));
			udat_format(df, dat, u_buffer, u_buffer_size, NULL, &status);
			result_len = icu_from_uchar(&result, u_buffer, u_buffer_size);
		}
		else
		{
			result_len = icu_from_uchar(&result, local_buf, u_buffer_size);
		}
	}
	if (df)
		udat_close(df);

	PG_RETURN_TEXT_P(cstring_to_text_with_len(result, result_len));
}


Datum
icu_format_date_default_locale(PG_FUNCTION_ARGS)
{
	return icu_format_date(PG_GETARG_TIMESTAMPTZ(0),
						   PG_GETARG_TEXT_PP(1),
						   NULL);
}

Datum
icu_format_date_locale(PG_FUNCTION_ARGS)
{
	return icu_format_date(PG_GETARG_TIMESTAMPTZ(0),
						   PG_GETARG_TEXT_PP(1),
						   text_to_cstring(PG_GETARG_TEXT_PP(2)));
}

static Datum
icu_parse_date(const text *input_date,
			   const text *input_format,
			   const char *locale)
{
	const char* date_string = text_to_cstring(input_date);
	const char* date_format = text_to_cstring(input_format);

	int32_t pattern_length;
	UChar* pattern_buf;
	UChar* u_date_string;
	int32_t u_date_length;
	UDateFormat* df = NULL;
	UDate udat;
	UErrorCode status = U_ZERO_ERROR;

	pattern_length = icu_to_uchar(&pattern_buf, date_format, strlen(date_format));
	u_date_length = icu_to_uchar(&u_date_string, date_string, strlen(date_string));

	/* if UDAT_PATTERN is used, we must pass it for both timeStyle and dateStyle */
	df = udat_open(UDAT_PATTERN, /* timeStyle */
				   UDAT_PATTERN, /* dateStyle */
				   locale,
				   0,
				   -1,
				   pattern_buf,
				   pattern_length,
				   &status);
	if (U_FAILURE(status))
	{
		udat_close(df);
		elog(ERROR, "udat_open failed: %s\n", u_errorName(status));
	}

	udat_setLenient(df, false);	/* strict parsing */

	udat = udat_parse(df,
					   u_date_string,
					   u_date_length,
					   NULL,
					   &status);
	udat_close(df);

	if (U_FAILURE(status))
		elog(ERROR, "udat_parse failed: %s\n", u_errorName(status));

	PG_RETURN_TIMESTAMPTZ(udate_to_ts(udat));
}

Datum
icu_parse_date_locale(PG_FUNCTION_ARGS)
{
	return icu_parse_date(PG_GETARG_TEXT_PP(0),
						  PG_GETARG_TEXT_PP(1),
						  text_to_cstring(PG_GETARG_TEXT_PP(2)));
}

Datum
icu_parse_date_default_locale(PG_FUNCTION_ARGS)
{
	/* Let the default ICU locale for now. Probably use a GUC later */
	return icu_parse_date(PG_GETARG_TEXT_PP(0),
						  PG_GETARG_TEXT_PP(1),
						  NULL);
}


/*
 * Input function for text representation of icu_date.
 */
Datum
icu_date_in(PG_FUNCTION_ARGS)
{
	char	   *date_string = PG_GETARG_CSTRING(0);
	int32_t pattern_length = -1;
	UChar *u_date_string;
	int32_t u_date_length;
	UDateFormat* df = NULL;
	UDate udat;
	UDateFormatStyle style = icu_ext_date_style;
	UErrorCode status = U_ZERO_ERROR;
	UChar *input_pattern = NULL;
	Timestamp pg_ts;
	const char *locale = NULL;
	DateADT		result;
	struct pg_tm tm;
	fsec_t		fsec;
	int32_t parse_pos = 0;
	UChar* tzid;
	int32_t tzid_length;

	if (icu_ext_date_format != NULL)
	{
		if (icu_ext_date_format[0] != '\0' && icu_ext_date_style == UDAT_NONE)
		{
			pattern_length = icu_to_uchar(&input_pattern,
										  icu_ext_date_format,
										  strlen(icu_ext_date_format));
		}
	}

	u_date_length = icu_to_uchar(&u_date_string, date_string, strlen(date_string));
	
	if (icu_ext_default_locale != NULL && icu_ext_default_locale[0] != '\0')
	{
		locale = icu_ext_default_locale;
	}

	tzid_length = icu_to_uchar(&tzid,
							   "GMT", /* for dates, we ignore timezones */
							   3);

	/* if UDAT_PATTERN is used, we must pass it for both timeStyle and dateStyle */
	df = udat_open(input_pattern ? UDAT_PATTERN : UDAT_NONE,	 /* timeStyle */
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
					  u_date_string,
					  u_date_length,
					  &parse_pos,
					  &status);
	udat_close(df);

	if (U_FAILURE(status))
		elog(ERROR, "udat_parse failed: %s\n", u_errorName(status));

	/* convert UDate to julian days, with an intermediate Timestamp to use date2j */
	pg_ts = udate_to_ts(udat);

	if (timestamp2tm(pg_ts, NULL, &tm, &fsec, NULL, NULL) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("date out of range: \"%s\"", date_string)));

	result = date2j(tm.tm_year, tm.tm_mon, tm.tm_mday) - POSTGRES_EPOCH_JDATE;

	PG_RETURN_DATEADT(result);
}

/* Convert a postgres date (number of days since 1/1/2000) to a UDate */
static UDate
dateadt_to_udate(DateADT pg_date)
{
	/* simple version */
	return (UDate)(
		(double)(pg_date+(POSTGRES_EPOCH_JDATE-UNIX_EPOCH_JDATE)) /* days since Unix epoch */
		*86400.0*1000 /* multiplied by the number of milliseconds in a day */
		);
}



Datum
icu_date_out(PG_FUNCTION_ARGS)
{
	DateADT date = PG_GETARG_DATEADT(0);
	char	buf[MAXDATELEN + 1];

	UErrorCode status = U_ZERO_ERROR;
	UDateFormat* df = NULL;
	UDate udate;
	const char *locale = NULL;
	char *result;
	UChar* tzid;
	int32_t tzid_length;

	if (DATE_NOT_FINITE(date))
	{
		EncodeSpecialDate(date, buf);
		result = pstrdup(buf);
	}
	else
	{
		UChar *output_pattern = NULL;
		int32_t pattern_length = -1;
		UDateFormatStyle style = icu_ext_date_style;

		udate = dateadt_to_udate(date);

		if (icu_ext_date_format != NULL)
		{
			if (icu_ext_date_format[0] != '\0' && icu_ext_date_style == UDAT_NONE)
			{
				pattern_length = icu_to_uchar(&output_pattern,
											  icu_ext_date_format,
											  strlen(icu_ext_date_format));
			}
		}

		if (icu_ext_default_locale != NULL && icu_ext_default_locale[0] != '\0')
		{
			locale = icu_ext_default_locale;
		}

		/* dates are not time-zone shifted when output */
		tzid_length = icu_to_uchar(&tzid,
								   UCAL_UNKNOWN_ZONE_ID, /*like GMT */
								   strlen(UCAL_UNKNOWN_ZONE_ID));
		/* if UDAT_PATTERN is passed, it must for both timeStyle and dateStyle */
		df = udat_open(output_pattern ? UDAT_PATTERN : UDAT_NONE,	 /* timeStyle */
					   output_pattern ? UDAT_PATTERN : style, /* dateStyle */
					   locale,		 /* NULL for the default locale */
					   tzid,		 /* tzID (NULL=default). */
					   tzid_length,			 /* tzIDLength */
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
	}
	PG_RETURN_CSTRING(result);
}

Datum
icu_date_add_days(PG_FUNCTION_ARGS)
{
	DateADT date = PG_GETARG_DATEADT(0);
	int32 days = PG_GETARG_INT32(1);
	/* same operation as the built-in date type */
	return DirectFunctionCall2(date_pli, date, days);
}

Datum
icu_date_days_add(PG_FUNCTION_ARGS)
{
	int32 days = PG_GETARG_INT32(0);
	DateADT date = PG_GETARG_DATEADT(1);
	/* same operation as the built-in date type */
	return DirectFunctionCall2(date_pli, date, days);
}


/*
 GUC:
 icu_ext.locale
 icu_ext.date_format
 icu_ext.timestamptz_format
*/
