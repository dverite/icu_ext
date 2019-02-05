/*
 * icu_ext.h
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2019. See LICENSE.md
 */

#include "postgres.h"
#include "unicode/ucol.h"

UCollator* ucollator_from_coll_id(Oid collid);

