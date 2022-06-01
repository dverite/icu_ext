/*
 * icu_ext.h
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2020. See LICENSE.md
 */

#include "postgres.h"
#include "unicode/ucol.h"

UCollator* ucollator_from_coll_id(Oid collid);

extern char *icu_ext_default_locale;
extern char *icu_ext_date_format;
extern char *icu_ext_timestamptz_format;
