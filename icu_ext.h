/*
 * icu_ext.h
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2023. See LICENSE.md
 */

#include "postgres.h"
#include "unicode/ucol.h"

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
extern char *icu_ext_timestamp_format;
