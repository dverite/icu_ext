/*
 * icu_interval.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2023. See LICENSE.md
 */

#include "icu_ext.h"

/* Postgres includes */
#include "funcapi.h"
#include "miscadmin.h"
#include "common/int.h"
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

PG_FUNCTION_INFO_V1(icu_interval_in);
PG_FUNCTION_INFO_V1(icu_interval_out);
PG_FUNCTION_INFO_V1(icu_from_interval);
PG_FUNCTION_INFO_V1(icu_timestamptz_add_interval);
PG_FUNCTION_INFO_V1(icu_interval_add_timestamptz);
PG_FUNCTION_INFO_V1(icu_timestamptz_sub_interval);
PG_FUNCTION_INFO_V1(icu_interval_mul);
PG_FUNCTION_INFO_V1(icu_mul_i_interval);
PG_FUNCTION_INFO_V1(icu_interv_plus_interv);
PG_FUNCTION_INFO_V1(icu_interv_minus_interv);


/*
 * Add an interval to a timestamp with timezone, given a localized calendar.
 * if locale==NULL, use the current ICU locale.
 */
static
Datum
add_interval(TimestampTz ts, const icu_interval_t *ival, const char *locale)
{
	UErrorCode status = U_ZERO_ERROR;
	UDate date_time = TS_TO_UDATE(ts);
	UCalendar *ucal;
	UChar* tzid;
	int32_t tzid_length;
	const char *pg_tz_name = pg_get_timezone_name(session_timezone);

	tzid_length = icu_to_uchar(&tzid,
							   pg_tz_name, /* or UCAL_UNKNOWN_ZONE_ID, like GMT */
							   strlen(pg_tz_name));

	ucal = ucal_open(tzid,
					 tzid_length,
					 locale,
					 UCAL_DEFAULT,
					 &status);
	if (U_FAILURE(status))
	{
		elog(ERROR, "ucal_open failed: %s\n", u_errorName(status));
	}

	ucal_setMillis(ucal, date_time, &status);

	/* Add years, months, days, with the rules of the given calendar */
	if (ival->year != 0)
		ucal_add(ucal, UCAL_YEAR, ival->year, &status);

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

	PG_RETURN_TIMESTAMPTZ(UDATE_TO_TS(date_time));
}

/*
 * This is similar to core's interval_in() except
 * there's no typmod support and no adjustment.
 */
Datum
icu_interval_in(PG_FUNCTION_ARGS)
{
	icu_interval_t *result;
	char	   *str = PG_GETARG_CSTRING(0);
/*	int32		typmod = PG_GETARG_INT32(2);  */
	int			dtype;
	int			nf;
	int			dterr;
	char	   *field[MAXDATEFIELDS];
	int			ftype[MAXDATEFIELDS];
	char		workbuf[256];
#if PG_VERSION_NUM >= 160000
	Node	   *escontext = fcinfo->context;
	DateTimeErrorExtra extra;
#endif
#if PG_VERSION_NUM >= 150000
	struct pg_itm_in tt,
			   *itm_in = &tt;
#else
	fsec_t		fsec;
	struct pg_tm tt,
			   *tm = &tt;
#endif


#if PG_VERSION_NUM >= 150000
	itm_in->tm_year = 0;
	itm_in->tm_mon = 0;
	itm_in->tm_mday = 0;
	itm_in->tm_usec = 0;
#else
	tm->tm_year = 0;
	tm->tm_mon = 0;
	tm->tm_mday = 0;
	tm->tm_hour = 0;
	tm->tm_min = 0;
	tm->tm_sec = 0;
	fsec = 0;
#endif

	dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field,
						  ftype, MAXDATEFIELDS, &nf);
	if (dterr == 0)
	{
#if PG_VERSION_NUM >= 150000
		dterr = DecodeInterval(field, ftype, nf, INTERVAL_FULL_RANGE,
							   &dtype, itm_in);
		/* if those functions think it's a bad format, try ISO8601 style */
		if (dterr == DTERR_BAD_FORMAT)
			dterr = DecodeISO8601Interval(str,
										  &dtype, itm_in);
#else
		dterr = DecodeInterval(field, ftype, nf, INTERVAL_FULL_RANGE,
							   &dtype, tm, &fsec);
		if (dterr == DTERR_BAD_FORMAT)
			dterr = DecodeISO8601Interval(str,
										  &dtype, tm, &fsec);
#endif
	}

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
			/* do not call itm2interval() to not merge years into months */
#if PG_VERSION_NUM >= 150000
			result->month = itm_in->tm_mon;
			result->day = itm_in->tm_mday;
			result->year = itm_in->tm_year;
			result->time = itm_in->tm_usec;
#else
			result->month = tm->tm_mon;
			result->day = tm->tm_mday;
			result->year = tm->tm_year;
			result->time = (((((tm->tm_hour * INT64CONST(60)) +
							   tm->tm_min) * INT64CONST(60)) +
							 tm->tm_sec) * USECS_PER_SEC) + fsec;

#endif
			break;

		default:
			elog(ERROR, "unexpected dtype %d while parsing interval \"%s\"",
				 dtype, str);
	}

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
	TimeOffset time, tfrac;
#if PG_VERSION_NUM >= 150000
	struct pg_itm itm;

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
#else
	/* see interval2tm() in backend/utils/adt/timestamp.c */
	fsec_t		fsec;
	struct pg_tm ptm,
		*tm = &ptm;

	tm->tm_year = itv->month / MONTHS_PER_YEAR;
	tm->tm_mon = itv->month % MONTHS_PER_YEAR;
	tm->tm_mday = itv->day;
	time = itv->time;

	tfrac = time / USECS_PER_HOUR;
	time -= tfrac * USECS_PER_HOUR;
	tm->tm_hour = tfrac;
	if ((tm->tm_hour < 0) != (tfrac < 0))  /* !SAMESIGN */
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("interval out of range")));
	tfrac = time / USECS_PER_MINUTE;
	time -= tfrac * USECS_PER_MINUTE;
	tm->tm_min = tfrac;
	tfrac = time / USECS_PER_SEC;
	fsec = time - (tfrac * USECS_PER_SEC);
	tm->tm_sec = tfrac;

	EncodeInterval(tm, fsec, IntervalStyle, buf);
#endif

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
 * icu_timestamptz + icu_interval
 */
Datum
icu_timestamptz_add_interval(PG_FUNCTION_ARGS)
{
	TimestampTz pg_ts = PG_GETARG_TIMESTAMPTZ(0);
	icu_interval_t *itv = (icu_interval_t*) PG_GETARG_DATUM(1);

	return add_interval(pg_ts, itv, icu_ext_default_locale);
}

/*
 * icu_interval + icu_timestamptz
 */
Datum
icu_interval_add_timestamptz(PG_FUNCTION_ARGS)
{
	icu_interval_t *itv = (icu_interval_t*) PG_GETARG_DATUM(0);
	TimestampTz pg_ts = PG_GETARG_TIMESTAMPTZ(1);

	return add_interval(pg_ts, itv, icu_ext_default_locale);
}

Datum
icu_timestamptz_sub_interval(PG_FUNCTION_ARGS)
{
	TimestampTz pg_ts = PG_GETARG_TIMESTAMPTZ(0);
	icu_interval_t *itv = (icu_interval_t*) PG_GETARG_DATUM(1);

	itv->year = -itv->year;
	itv->month = -itv->month;
	itv->day = -itv->day;
	itv->time = -itv->time;

	return add_interval(pg_ts, itv, icu_ext_default_locale);
}

Datum
icu_interval_mul(PG_FUNCTION_ARGS)
{
	icu_interval_t *itv = (icu_interval_t*) PG_GETARG_DATUM(0);
	int32 factor = PG_GETARG_INT32(1);
	icu_interval_t *result;

	result = (icu_interval_t *) palloc(sizeof(icu_interval_t));

	if (pg_mul_s32_overflow(itv->day, factor, &result->day) ||
		pg_mul_s32_overflow(itv->month, factor, &result->month) ||
		pg_mul_s32_overflow(itv->year, factor, &result->year) ||
		pg_mul_s64_overflow(itv->time, factor, &result->time))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("interval out of range")));
	}
	return PointerGetDatum(result);
}

/* integer multiplied by icu_interval */
Datum
icu_mul_i_interval(PG_FUNCTION_ARGS)
{
	Datum factor = PG_GETARG_DATUM(0);
	Datum itv = PG_GETARG_DATUM(1);
	return DirectFunctionCall2(icu_interval_mul, itv, factor);
}


/* icu_interval + icu_interval */
Datum
icu_interv_plus_interv(PG_FUNCTION_ARGS)
{
	icu_interval_t *i1 = (icu_interval_t*) PG_GETARG_DATUM(0);
	icu_interval_t *i2 = (icu_interval_t*) PG_GETARG_DATUM(1);
	icu_interval_t *result;

	result = (icu_interval_t *) palloc(sizeof(icu_interval_t));
	if (pg_add_s32_overflow(i1->day, i2->day, &result->day) ||
		pg_add_s32_overflow(i1->month, i2->month, &result->month) ||
		pg_add_s32_overflow(i1->year, i2->year, &result->year) ||
		pg_add_s64_overflow(i1->time, i2->time, &result->time))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("interval out of range")));
	}
	return PointerGetDatum(result);
}

/* icu_interval - icu_interval */
Datum
icu_interv_minus_interv(PG_FUNCTION_ARGS)
{
	icu_interval_t *i1 = (icu_interval_t*) PG_GETARG_DATUM(0);
	icu_interval_t *i2 = (icu_interval_t*) PG_GETARG_DATUM(1);
	icu_interval_t *result;

	result = (icu_interval_t *) palloc(sizeof(icu_interval_t));
	if (pg_add_s32_overflow(i1->day, -i2->day, &result->day) ||
		pg_add_s32_overflow(i1->month, -i2->month, &result->month) ||
		pg_add_s32_overflow(i1->year, -i2->year, &result->year) ||
		pg_add_s64_overflow(i1->time, -i2->time, &result->time))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
				 errmsg("interval out of range")));
	}
	return PointerGetDatum(result);
}


/*
TODO:
- binary
- cast from icu_interval to interval?
- justify_interval?
*/
