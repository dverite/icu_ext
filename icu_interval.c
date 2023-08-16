/*
 * icu_interval.c
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
#include "miscadmin.h"
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

PG_FUNCTION_INFO_V1(icu_add_interval);
PG_FUNCTION_INFO_V1(icu_add_interval_default_locale);

PG_FUNCTION_INFO_V1(icu_interval_in);
PG_FUNCTION_INFO_V1(icu_interval_out);
PG_FUNCTION_INFO_V1(icu_from_interval);

/*
 * icu_interval_t is like Interval except for the additional year
 * field. Interval considers that 1 year = 12 months, whereas
 * icu_interval_t does not.
 */
typedef struct {
	TimeOffset	time;			/* all time units other than days, months and
								 * years */
	int32		day;			/* days, after time for alignment */
	int32		month;			/* months, after time for alignment */
	int32		year;			/* years */
} icu_interval_t;

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

	if (ival->time != 0)
		ucal_add(ucal, UCAL_MILLISECOND, ival->time/1000, &status);

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
icu_interval_in(PG_FUNCTION_ARGS)
{
	icu_interval_t *result;
	char	   *str = PG_GETARG_CSTRING(0);
	int32		typmod = PG_GETARG_INT32(2);
	struct pg_itm_in tt,
			   *itm_in = &tt;
	int			dtype;
	int			nf;
	int			range;
	int			dterr;
	char	   *field[MAXDATEFIELDS];
	int			ftype[MAXDATEFIELDS];
	char		workbuf[256];
#if PG_VERSION_NUM >= 160000
	Node	   *escontext = fcinfo->context;
	DateTimeErrorExtra extra;
#endif

	itm_in->tm_year = 0;
	itm_in->tm_mon = 0;
	itm_in->tm_mday = 0;
	itm_in->tm_usec = 0;

	if (typmod >= 0)
		range = INTERVAL_RANGE(typmod);
	else
		range = INTERVAL_FULL_RANGE;

	dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field,
						  ftype, MAXDATEFIELDS, &nf);
	if (dterr == 0)
		dterr = DecodeInterval(field, ftype, nf, range,
							   &dtype, itm_in);

	/* if those functions think it's a bad format, try ISO8601 style */
	if (dterr == DTERR_BAD_FORMAT)
		dterr = DecodeISO8601Interval(str,
									  &dtype, itm_in);

	if (dterr != 0)
	{
		if (dterr == DTERR_FIELD_OVERFLOW)
			dterr = DTERR_INTERVAL_OVERFLOW;
#if PG_VERSION_NUM >= 160000
		DateTimeParseError(dterr, &extra, str, "interval", escontext);
#else
		DateTimeParseError(dterr, str, "interval");
#endif
		PG_RETURN_NULL();
	}

	result	= (icu_interval_t*) palloc(sizeof(icu_interval_t));

	switch (dtype)
	{
		case DTK_DELTA:
			/* do not call itmin2interval() to not merge years into months */
			result->month = itm_in->tm_mon;
			result->day = itm_in->tm_mday;
			result->year = itm_in->tm_year;
			result->time = itm_in->tm_usec;
			break;

		default:
			elog(ERROR, "unexpected dtype %d while parsing interval \"%s\"",
				 dtype, str);
	}
#if 0
	// FIXME
	AdjustIntervalForTypmod(result, typmod, escontext);
#endif

	return PointerGetDatum(result);
}

/*
 * Text representation for icu_interval.
 * It is essentially identical to "interval" except that 
 * the year field is not months%12
 */
Datum
icu_interval_out(PG_FUNCTION_ARGS)
{
	icu_interval_t *itv = (icu_interval_t*)PG_GETARG_DATUM(0);
	char buf[MAXDATELEN + 1];
	struct pg_itm itm;
	TimeOffset time, tfrac;

	itm.tm_year = itv->year;
	itm.tm_mon = itv->month;
	itm.tm_mday = itv->day;

	/* The following code is copied from interval2itm()
	   in backend/utils/adt/timestamp.c */
	time = itv->time;

	tfrac = time / USECS_PER_HOUR;
	time -= tfrac * USECS_PER_HOUR;
	itm.tm_hour = tfrac;
	tfrac = time / USECS_PER_MINUTE;
	time -= tfrac * USECS_PER_MINUTE;
	itm.tm_min = (int) tfrac;
	tfrac = time / USECS_PER_SEC;
	time -= tfrac * USECS_PER_SEC;
	itm.tm_sec = (int) tfrac;
	itm.tm_usec = (int) time;

	EncodeInterval(&itm, IntervalStyle, buf);

	PG_RETURN_CSTRING(pstrdup(buf));
}

Datum
icu_from_interval(PG_FUNCTION_ARGS)
{
	Interval *pg_interval = PG_GETARG_INTERVAL_P(0);
	icu_interval_t *interval = (icu_interval_t*) palloc(sizeof(icu_interval_t));
	interval->time = pg_interval->time;
	interval->day = pg_interval->day;
	interval->month = pg_interval->month;
	interval->year = 0;
	return PointerGetDatum(interval);
}

/*
TODO:
- in, out with '<number>Y <number>M <number>D <number>h <number>m <number>s'
- multiplication, as in '1M'::icu_interval * 4
- binary
- add to icu_timestamptz and icu_date
- justify_interval?
*/
