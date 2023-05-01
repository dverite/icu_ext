-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.8'" to load this file. \quit

/* Interface to udat_parse().
   The calendar is typically set in the locale argument. */
CREATE OR REPLACE FUNCTION icu_parse_date(
 date_string text,
 format text,
 locale text
) RETURNS timestamptz
AS 'MODULE_PATHNAME', 'icu_parse_date_locale'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

/* Interface to udat_parse(), without the locale argument. */
CREATE OR REPLACE FUNCTION icu_parse_date(
 date_string text,
 format text
) RETURNS timestamptz
AS 'MODULE_PATHNAME', 'icu_parse_date_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_parse_date(text,text,text)
IS 'Convert a locale-formatted date into a time stamp, using the supplied locale ';

COMMENT ON FUNCTION icu_parse_date(text,text)
IS 'Convert a locale-formatted date into a time stamp';

/* Interface to udat_format().
   The calendar is typically set in the locale argument. */

CREATE OR REPLACE FUNCTION icu_format_date(
 tstamp timestamptz,
 format text,
 locale text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_format_date_locale'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_format_date(timestamptz,text,text)
IS 'Convert a time stamp into a string according to the given locale and format';


CREATE OR REPLACE FUNCTION icu_format_date(
 tstamp timestamptz,
 format text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_format_date_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_format_date(timestamptz,text)
IS 'Convert a time stamp into a string according to the given format and default locale';

CREATE OR REPLACE FUNCTION icu_add_interval(
 tstamp timestamptz,
 delta interval
) RETURNS timestamptz
AS 'MODULE_PATHNAME', 'icu_add_interval_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;


COMMENT ON FUNCTION icu_add_interval(timestamptz,interval)
IS 'Add an interval to a timestamp using the calendar of the default locale';

CREATE OR REPLACE FUNCTION icu_add_interval(
 tstamp timestamptz,
 delta interval,
 locale text
) RETURNS timestamptz
AS 'MODULE_PATHNAME', 'icu_add_interval'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_add_interval(timestamptz,interval)
IS 'Add an interval to a timestamp using the calendar of the given locale';


CREATE OR REPLACE FUNCTION icu_diff_timestamps(
 tstamp1 timestamptz,
 tstamp2 timestamptz
) RETURNS interval
AS 'MODULE_PATHNAME', 'icu_diff_timestamps_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_diff_timestamps(timestamptz,timestamptz)
IS 'Compute the difference between two dates in the calendar of the default locale';

CREATE OR REPLACE FUNCTION icu_diff_timestamps(
 tstamp1 timestamptz,
 tstamp2 timestamptz,
 locale text
) RETURNS interval
AS 'MODULE_PATHNAME', 'icu_diff_timestamps'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_diff_timestamps(timestamptz,timestamptz,text)
IS 'Compute the difference between two dates according to the calendar of the given locale';


CREATE OR REPLACE FUNCTION icu_date_in(cstring)
RETURNS icu_date
AS 'MODULE_PATHNAME', 'icu_date_in'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

CREATE OR REPLACE FUNCTION icu_date_out(icu_date)
RETURNS cstring
AS 'MODULE_PATHNAME', 'icu_date_out'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

CREATE TYPE icu_date (
 INPUT = icu_date_in,
 OUTPUT = icu_date_out,
 LIKE = pg_catalog.date
);

CREATE FUNCTION icu_date_eq (icu_date, icu_date) RETURNS bool
LANGUAGE internal AS 'date_eq' IMMUTABLE STRICT;

CREATE FUNCTION icu_date_ne (icu_date, icu_date) RETURNS bool
LANGUAGE internal AS 'date_ne' IMMUTABLE STRICT;

CREATE FUNCTION icu_date_gt (icu_date, icu_date) RETURNS bool
LANGUAGE internal AS 'date_gt' IMMUTABLE STRICT;

CREATE FUNCTION icu_date_ge (icu_date, icu_date) RETURNS bool
LANGUAGE internal AS 'date_ge' IMMUTABLE STRICT;

CREATE FUNCTION icu_date_lt (icu_date, icu_date) RETURNS bool
LANGUAGE internal AS 'date_lt' IMMUTABLE STRICT;

CREATE FUNCTION icu_date_le (icu_date, icu_date) RETURNS bool
LANGUAGE internal AS 'date_le' IMMUTABLE STRICT;

CREATE FUNCTION icu_date_cmp (icu_date, icu_date) RETURNS int4
LANGUAGE internal AS 'date_cmp' IMMUTABLE STRICT;

CREATE OPERATOR = (
 PROCEDURE = icu_date_eq,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '<>',
 HASHES, MERGES
);

CREATE OPERATOR <> (
 PROCEDURE = icu_date_ne,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '=',
 HASHES, MERGES
);

CREATE OPERATOR > (
 PROCEDURE = icu_date_gt,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '=',
 HASHES, MERGES
);

CREATE OPERATOR >= (
 PROCEDURE = icu_date_ge,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '=',
 HASHES, MERGES
);

CREATE OPERATOR < (
 PROCEDURE = icu_date_lt,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '=',
 HASHES, MERGES
);

CREATE OPERATOR <= (
 PROCEDURE = icu_date_le,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '=',
 HASHES, MERGES
);

CREATE OPERATOR CLASS icu_date_ops
DEFAULT FOR TYPE icu_date
USING btree AS
OPERATOR 1 <,
OPERATOR 2 <=,
OPERATOR 3 =,
OPERATOR 4 >=,
OPERATOR 5 >,
FUNCTION 1 icu_date_cmp(icu_date, icu_date);

CREATE CAST (icu_date AS date) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (date AS icu_date) WITHOUT FUNCTION AS IMPLICIT;

