/*
 * icu_ext.h
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2025. See LICENSE.md
 */

#include "postgres.h"
#include "fmgr.h"
#include "datatype/timestamp.h"

#include "unicode/ucol.h"
#include "unicode/udat.h"

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

UCollator* ucollator_from_coll_id(Oid collid);

extern char *icu_ext_default_locale;
extern char *icu_ext_date_format;
extern char *icu_ext_timestamptz_format;
extern UDateFormatStyle icu_ext_date_style;
extern UDateFormatStyle icu_ext_timestamptz_style;

extern UDateFormatStyle date_format_style(const char *fmt);

extern Datum icu_timestamptz_add_interval(PG_FUNCTION_ARGS);
extern Datum icu_timestamptz_sub_interval(PG_FUNCTION_ARGS);


/*
 * Convert a Postgres timestamp into an ICU timestamp
 * ICU's UDate is a number of milliseconds since the Unix Epoch,
 * (1970-01-01, 00:00 UTC), stored as a double.
 * Postgres' TimestampTz is a number of microseconds since 2000-01-01 00:00 UTC,
 * stored as an int64.
 * The code below translates directly between the epochs
 */
#define TS_TO_UDATE(pg_tstz) \
  (UDate)(10957.0*86400*1000 + (pg_tstz)/1000)

/*
 * Convert an ICU timestamp into a Postgres timestamp
 * Input: number of milliseconds since 1970-01-01 UTC
 * Output: number of microseconds since 2000-01-01 UTC
 */
#define UDATE_TO_TS(ud) \
  (TimestampTz)((ud)*1000 - 10957LL*86400*1000*1000)
