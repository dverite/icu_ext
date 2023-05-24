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
