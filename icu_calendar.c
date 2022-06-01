/*
 * icu_calendar.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2022. See LICENSE.md
 */

/* Postgres includes */
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/pg_locale.h"
#include "utils/date.h"
#include "utils/datetime.h"

/* ICU includes */
#include "unicode/ucal.h"
#include "unicode/ucnv.h"  /* needed? */
#include "unicode/udat.h"
#include "unicode/ustring.h"


#include "icu_ext.h"

PG_FUNCTION_INFO_V1(icu_format_date_locale);
PG_FUNCTION_INFO_V1(icu_format_date_default_locale);
PG_FUNCTION_INFO_V1(icu_parse_date_locale);
PG_FUNCTION_INFO_V1(icu_parse_date_default_locale);
PG_FUNCTION_INFO_V1(icu_add_interval);
PG_FUNCTION_INFO_V1(icu_add_interval_default_locale);
PG_FUNCTION_INFO_V1(icu_diff_timestamps);
PG_FUNCTION_INFO_V1(icu_diff_timestamps_default_locale);
PG_FUNCTION_INFO_V1(icu_date_in);
PG_FUNCTION_INFO_V1(icu_date_out);


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
	UDate dat = ts_to_udate(pg_tstz);

	pattern_length = icu_to_uchar(&pattern_buf, icu_date_format, strlen(icu_date_format));

	/* if UDAT_PATTERN is passed, it must for both timeStyle and dateStyle */
	df = udat_open(UDAT_PATTERN, /* timeStyle */
				   UDAT_PATTERN, /* dateStyle */
				   locale,		 /* NULL for the default locale */
				   NULL,			/* tzID (NULL=default). FIXME  */
				   -1,			/* tzIDLength */
				   pattern_buf,
				   pattern_length,
				   &status);
	if (U_FAILURE(status))
		elog(ERROR, "udat_open failed with code %d\n", status);

	{
		int32_t u_buffer_size = udat_format(df, dat, NULL, 0, NULL, &status);

		if(status == U_BUFFER_OVERFLOW_ERROR)
		{
			UChar* u_buffer;
			status = U_ZERO_ERROR;
			u_buffer = (UChar*) palloc(u_buffer_size*sizeof(UChar));
			udat_format(df, dat, u_buffer, u_buffer_size, NULL, &status);
			result_len = icu_from_uchar(&result, u_buffer, u_buffer_size);
		}
		else
		{
			result = "";
			result_len= 0;
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
 * Add an interval to a timestamp with timezone, given a localized calendar.
 * if locale==NULL, use the current ICU locale.
 */
static
Datum
add_interval(TimestampTz ts, Interval *ival, const char *locale)
{
	UErrorCode status = U_ZERO_ERROR;
	UDate date_time = ts_to_udate(ts);
	UCalendar *ucal;

	ucal = ucal_open(NULL, /* default zoneID */
					 0,
					 locale,
					 UCAL_DEFAULT,
					 &status);
	if (U_FAILURE(status))
	{
		elog(ERROR, "ucal_open failed: %s\n", u_errorName(status));
	}

	ucal_setMillis(ucal, date_time, &status);

	/* Add months and days, with the rules of the given calendar */
	if (ival->month != 0)
		ucal_add(ucal, UCAL_MONTH, ival->month, &status);

	if (ival->day != 0)
		ucal_add(ucal, UCAL_DAY_OF_MONTH, ival->day, &status);

	/* Translate back to a UDate, and then to a postgres timestamptz */
	date_time = ucal_getMillis(ucal, &status);
	ucal_close(ucal);

	if (U_FAILURE(status))
	{
		elog(ERROR, "calendar translation failed: %s\n", u_errorName(status));
	}

	PG_RETURN_TIMESTAMPTZ(udate_to_ts(date_time));
}

Datum
icu_add_interval(PG_FUNCTION_ARGS)
{
	TimestampTz pg_tstz = PG_GETARG_TIMESTAMPTZ(0);
	Interval *pg_interval = PG_GETARG_INTERVAL_P(1);
	const char *locale = text_to_cstring(PG_GETARG_TEXT_PP(2));

	return add_interval(pg_tstz, pg_interval, locale);
}

Datum
icu_add_interval_default_locale(PG_FUNCTION_ARGS)
{
	TimestampTz pg_tstz = PG_GETARG_TIMESTAMPTZ(0);
	Interval *pg_interval = PG_GETARG_INTERVAL_P(1);

	return add_interval(pg_tstz, pg_interval, NULL);
}

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
	UErrorCode status = U_ZERO_ERROR;
	UChar *input_pattern = NULL;
	Timestamp pg_ts;
	const char *locale = NULL;
	DateADT		result;
	struct pg_tm tt,
			   *tm = &tt;
	fsec_t		fsec;
	int32_t parse_pos = 0;
	UChar* tzid;
	int32_t tzid_length;
	
	if (icu_ext_date_format != NULL && icu_ext_date_format[0] != '\0')
	{
		pattern_length = icu_to_uchar(&input_pattern,
									  icu_ext_date_format,
									  strlen(icu_ext_date_format));
	}

	u_date_length = icu_to_uchar(&u_date_string, date_string, strlen(date_string));
	
	if (icu_ext_default_locale != NULL && icu_ext_default_locale[0] != '\0')
	{
		locale = icu_ext_default_locale;
	}

	tzid_length = icu_to_uchar(&tzid,
							   UCAL_UNKNOWN_ZONE_ID, /* like GMT */
							   strlen(UCAL_UNKNOWN_ZONE_ID));

	/* if UDAT_PATTERN is used, we must pass it for both timeStyle and dateStyle */
	df = udat_open(input_pattern ? UDAT_PATTERN : UDAT_NONE,	 /* timeStyle */
				   input_pattern ? UDAT_PATTERN : UDAT_DEFAULT, /* dateStyle */
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

	udat = udat_parse(df,
					   u_date_string,
					   u_date_length,
					   &parse_pos,
					   &status);
	udat_close(df);

	elog(DEBUG1, "udat_parse('%s'[%d], '%s' ) => %f, parse_pos=%d",
		 date_string, u_strlen(u_date_string), icu_ext_date_format, udat, parse_pos);

	if (U_FAILURE(status))
		elog(ERROR, "udat_parse failed: %s\n", u_errorName(status));


	/* convert UDate to julian days, with an intermediate Timestamp to use date2j */
	pg_ts = udate_to_ts(udat);

	if (timestamp2tm(pg_ts, NULL, tm, &fsec, NULL, NULL) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("date out of range: \"%s\"", date_string)));

	result = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - POSTGRES_EPOCH_JDATE;

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
	
	if (DATE_NOT_FINITE(date))
	{
		EncodeSpecialDate(date, buf);
		result = pstrdup(buf);
	}
	else
	{
		UChar *output_pattern = NULL;
		int32_t pattern_length = -1;

		udate = dateadt_to_udate(date);

		if (icu_ext_date_format != NULL && icu_ext_date_format[0] != '\0')
		{
			pattern_length = icu_to_uchar(&output_pattern,
										  icu_ext_date_format,
										  strlen(icu_ext_date_format));
		}

		if (icu_ext_default_locale != NULL && icu_ext_default_locale[0] != '\0')
		{
			locale = icu_ext_default_locale;
		}

		/* if UDAT_PATTERN is passed, it must for both timeStyle and dateStyle */
		df = udat_open(output_pattern ? UDAT_PATTERN : UDAT_NONE,	 /* timeStyle */
					   output_pattern ? UDAT_PATTERN : UDAT_DEFAULT, /* dateStyle */
					   locale,		 /* NULL for the default locale */
					   NULL,			/* tzID (NULL=default). */
					   -1,			/* tzIDLength */
					   output_pattern,		/* pattern */
					   pattern_length,			/* patternLength */
					   &status);
		if (U_FAILURE(status))
			elog(ERROR, "udat_open failed with code %d\n", status);
		{
			/* TODO: try first with a buffer of MAXDATELEN*sizeof(UChar) size */
			int32_t u_buffer_size = udat_format(df, udate, NULL, 0, NULL, &status);

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
				result = pstrdup("");
			}
		}
		if (df)
			udat_close(df);
	}
	PG_RETURN_CSTRING(result);
}

/*
 GUC:
 icu_ext.locale
 icu_ext.date_format
 icu_ext.timestamptz_format
*/
