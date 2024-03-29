-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.8'" to load this file. \quit

/* Interface to udat_parse().
   The calendar is typically set in the locale argument. */
CREATE FUNCTION icu_parse_date(
 date_string text,
 format text,
 locale text
) RETURNS date
AS 'MODULE_PATHNAME', 'icu_parse_date_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

/* Interface to udat_parse(), without the locale argument. */
CREATE FUNCTION icu_parse_date(
 date_string text,
 format text
) RETURNS date
AS 'MODULE_PATHNAME', 'icu_parse_date_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_parse_date(text,text,text)
IS 'Convert a locale-formatted string into a date, using the supplied locale';

COMMENT ON FUNCTION icu_parse_date(text,text)
IS 'Convert a locale-formatted string into a date, using the default locale';

CREATE FUNCTION icu_parse_datetime(
 date_string text,
 format text,
 locale text
) RETURNS timestamptz
AS 'MODULE_PATHNAME', 'icu_parse_datetime_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

/* Interface to udat_parse(), without the locale argument. */
CREATE FUNCTION icu_parse_datetime(
 date_string text,
 format text
) RETURNS timestamptz
AS 'MODULE_PATHNAME', 'icu_parse_datetime_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_parse_datetime(text,text,text)
IS 'Convert a locale-formatted string into a timestamptz, using the supplied locale';

COMMENT ON FUNCTION icu_parse_datetime(text,text)
IS 'Convert a locale-formatted string into a timestamptz, using the default locale';

/* Interface to udat_format().
   The calendar is typically set in the locale argument. */

CREATE FUNCTION icu_format_datetime(
 tstamp timestamptz,
 format text,
 locale text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_format_datetime_locale'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_format_datetime(timestamptz,text,text)
IS 'Convert a time stamp into a string according to the given locale and format';


CREATE FUNCTION icu_format_datetime(
 tstamp timestamptz,
 format text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_format_datetime_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_format_datetime(timestamptz,text)
IS 'Convert a time stamp into a string according to the given format and default locale';

CREATE FUNCTION icu_format_date(
 input date,
 format text,
 locale text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_format_date_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE FUNCTION icu_format_date(
 input date,
 format text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_format_date_default_locale'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

---
--- icu_date datatype
---

CREATE FUNCTION icu_date_in(cstring)
RETURNS icu_date
AS 'MODULE_PATHNAME', 'icu_date_in'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE FUNCTION icu_date_out(icu_date)
RETURNS cstring
AS 'MODULE_PATHNAME', 'icu_date_out'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE TYPE icu_date (
 INPUT = icu_date_in,
 OUTPUT = icu_date_out,
 LIKE = pg_catalog.date
);

CREATE FUNCTION icu_date_add_days(icu_date, int4)
RETURNS icu_date
AS 'MODULE_PATHNAME', 'icu_date_add_days'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

CREATE FUNCTION icu_date_days_add(int4, icu_date)
RETURNS icu_date
AS 'MODULE_PATHNAME', 'icu_date_days_add'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

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
 NEGATOR = '<=',
 HASHES, MERGES
);

CREATE OPERATOR >= (
 PROCEDURE = icu_date_ge,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '<',
 HASHES, MERGES
);

CREATE OPERATOR < (
 PROCEDURE = icu_date_lt,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '>=',
 HASHES, MERGES
);

CREATE OPERATOR <= (
 PROCEDURE = icu_date_le,
 LEFTARG = icu_date,
 RIGHTARG = icu_date,
 NEGATOR = '>',
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

CREATE OPERATOR + (
 PROCEDURE = icu_date_add_days,
 LEFTARG = icu_date,
 RIGHTARG = int4
);

CREATE OPERATOR + (
 PROCEDURE = icu_date_days_add,
 LEFTARG = int4,
 RIGHTARG = icu_date
);

CREATE CAST (icu_date AS date) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (date AS icu_date) WITHOUT FUNCTION AS IMPLICIT;


---
--- icu_timestamptz datatype
---
CREATE FUNCTION icu_timestamptz_in(cstring)
RETURNS icu_timestamptz
AS 'MODULE_PATHNAME', 'icu_timestamptz_in'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE FUNCTION icu_timestamptz_out(icu_timestamptz)
RETURNS cstring
AS 'MODULE_PATHNAME', 'icu_timestamptz_out'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE TYPE icu_timestamptz (
 INPUT = icu_timestamptz_in,
 OUTPUT = icu_timestamptz_out,
 LIKE = pg_catalog.timestamptz
);

CREATE FUNCTION icu_timestamptz_eq (icu_timestamptz, icu_timestamptz) RETURNS bool
LANGUAGE internal AS 'timestamp_eq' IMMUTABLE STRICT;

CREATE FUNCTION icu_timestamptz_ne (icu_timestamptz, icu_timestamptz) RETURNS bool
LANGUAGE internal AS 'timestamp_ne' IMMUTABLE STRICT;

CREATE FUNCTION icu_timestamptz_gt (icu_timestamptz, icu_timestamptz) RETURNS bool
LANGUAGE internal AS 'timestamp_gt' IMMUTABLE STRICT;

CREATE FUNCTION icu_timestamptz_ge (icu_timestamptz, icu_timestamptz) RETURNS bool
LANGUAGE internal AS 'timestamp_ge' IMMUTABLE STRICT;

CREATE FUNCTION icu_timestamptz_lt (icu_timestamptz, icu_timestamptz) RETURNS bool
LANGUAGE internal AS 'timestamp_lt' IMMUTABLE STRICT;

CREATE FUNCTION icu_timestamptz_le (icu_timestamptz, icu_timestamptz) RETURNS bool
LANGUAGE internal AS 'timestamp_le' IMMUTABLE STRICT;

CREATE FUNCTION icu_timestamptz_cmp (icu_timestamptz, icu_timestamptz) RETURNS int4
LANGUAGE internal AS 'timestamp_cmp' IMMUTABLE STRICT;

CREATE OPERATOR = (
 PROCEDURE = icu_timestamptz_eq,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_timestamptz,
 NEGATOR = '<>',
 HASHES, MERGES
);

CREATE OPERATOR <> (
 PROCEDURE = icu_timestamptz_ne,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_timestamptz,
 NEGATOR = '=',
 HASHES, MERGES
);

CREATE OPERATOR > (
 PROCEDURE = icu_timestamptz_gt,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_timestamptz,
 NEGATOR = '<=',
 HASHES, MERGES
);

CREATE OPERATOR >= (
 PROCEDURE = icu_timestamptz_ge,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_timestamptz,
 NEGATOR = '<',
 HASHES, MERGES
);

CREATE OPERATOR < (
 PROCEDURE = icu_timestamptz_lt,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_timestamptz,
 NEGATOR = '>=',
 HASHES, MERGES
);

CREATE OPERATOR <= (
 PROCEDURE = icu_timestamptz_le,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_timestamptz,
 NEGATOR = '>',
 HASHES, MERGES
);

CREATE OPERATOR CLASS icu_timestamptz_ops
DEFAULT FOR TYPE icu_timestamptz
USING btree AS
OPERATOR 1 <,
OPERATOR 2 <=,
OPERATOR 3 =,
OPERATOR 4 >=,
OPERATOR 5 >,
FUNCTION 1 icu_timestamptz_cmp(icu_timestamptz, icu_timestamptz);

CREATE CAST (icu_timestamptz AS timestamptz) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (timestamptz AS icu_timestamptz) WITHOUT FUNCTION AS IMPLICIT;


--
-- icu_interval datatype
---
CREATE FUNCTION icu_interval_in(cstring)
RETURNS icu_interval
AS 'MODULE_PATHNAME', 'icu_interval_in'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE FUNCTION icu_interval_out(icu_interval)
RETURNS cstring
AS 'MODULE_PATHNAME', 'icu_interval_out'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE TYPE icu_interval (
 INPUT = icu_interval_in,
 OUTPUT = icu_interval_out,
 INTERNALLENGTH = 20,
 ALIGNMENT = 'double'
-- SEND = icu_interval_send,
-- RECEIVE = interval_recv
);

CREATE FUNCTION icu_from_interval (interval) RETURNS icu_interval
AS 'MODULE_PATHNAME', 'icu_from_interval'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE CAST (interval AS icu_interval)
 WITH FUNCTION icu_from_interval(interval)
 AS IMPLICIT;


--
-- Non-implicit casts
--
CREATE FUNCTION icu_date_to_ts(icu_date) RETURNS icu_timestamptz
AS 'MODULE_PATHNAME', 'icu_date_to_ts'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE CAST (icu_date AS icu_timestamptz)
 WITH FUNCTION icu_date_to_ts(icu_date)
 AS ASSIGNMENT;

CREATE FUNCTION icu_ts_to_date(icu_timestamptz) RETURNS icu_date
AS 'MODULE_PATHNAME', 'icu_ts_to_date'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE CAST (icu_timestamptz AS icu_date)
 WITH FUNCTION icu_ts_to_date(icu_timestamptz)
 AS ASSIGNMENT;

CREATE CAST (timestamptz AS icu_date)
 WITH FUNCTION icu_ts_to_date(icu_timestamptz)
 AS ASSIGNMENT;


--
-- Functions and operators combining types
--

/* icu_timestamptz plus icu_interval */
CREATE FUNCTION icu_timestamptz_add_interval(icu_timestamptz, icu_interval)
RETURNS icu_timestamptz
AS 'MODULE_PATHNAME', 'icu_timestamptz_add_interval'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

/* icu_interval plus icu_timestamptz */
CREATE FUNCTION icu_interval_add_timestamptz(icu_interval, icu_timestamptz)
RETURNS icu_timestamptz
AS 'MODULE_PATHNAME', 'icu_interval_add_timestamptz'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

/* icu_timestamptz minus icu_interval */
CREATE FUNCTION icu_timestamptz_sub_interval(icu_timestamptz, icu_interval)
RETURNS icu_timestamptz
AS 'MODULE_PATHNAME', 'icu_timestamptz_sub_interval'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

/* icu_interval multiplied by integer */
CREATE FUNCTION icu_interval_mul(icu_interval, int)
RETURNS icu_interval
AS 'MODULE_PATHNAME', 'icu_interval_mul'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

/* integer multiplied by icu_interval */
CREATE FUNCTION icu_mul_i_interval(int, icu_interval)
RETURNS icu_interval
AS 'MODULE_PATHNAME', 'icu_mul_i_interval'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;


CREATE OPERATOR + (
 PROCEDURE = icu_timestamptz_add_interval,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_interval,
 COMMUTATOR = +
);

CREATE OPERATOR + (
 PROCEDURE = icu_interval_add_timestamptz,
 LEFTARG = icu_interval,
 RIGHTARG = icu_timestamptz,
 COMMUTATOR = +
);

CREATE OPERATOR - (
 PROCEDURE = icu_timestamptz_sub_interval,
 LEFTARG = icu_timestamptz,
 RIGHTARG = icu_interval
);

CREATE OPERATOR * (
 PROCEDURE = icu_interval_mul,
 LEFTARG = icu_interval,
 RIGHTARG = int,
 COMMUTATOR = *
);

CREATE OPERATOR * (
 PROCEDURE = icu_mul_i_interval,
 LEFTARG = int,
 RIGHTARG = icu_interval,
 COMMUTATOR = *
);

/* icu_interval plus icu_interval */
CREATE FUNCTION icu_interv_plus_interv(icu_interval, icu_interval)
RETURNS icu_interval
AS 'MODULE_PATHNAME', 'icu_interv_plus_interv'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

/* icu_interval plus icu_interval */
CREATE FUNCTION icu_interv_minus_interv(icu_interval, icu_interval)
RETURNS icu_interval
AS 'MODULE_PATHNAME', 'icu_interv_minus_interv'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

CREATE OPERATOR + (
 PROCEDURE = icu_interv_plus_interv,
 LEFTARG = icu_interval,
 RIGHTARG = icu_interval,
 COMMUTATOR = +
);

CREATE OPERATOR - (
 PROCEDURE = icu_interv_minus_interv,
 LEFTARG = icu_interval,
 RIGHTARG = icu_interval
);

CREATE FUNCTION icu_date_plus_interval(icu_date, icu_interval)
RETURNS icu_timestamptz
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE FUNCTION icu_date_minus_interval(icu_date, icu_interval)
RETURNS icu_timestamptz
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

CREATE OPERATOR + (
 PROCEDURE = icu_date_plus_interval,
 LEFTARG = icu_date,
 RIGHTARG = icu_interval,
 COMMUTATOR = +
);

CREATE OPERATOR - (
 PROCEDURE = icu_date_minus_interval,
 LEFTARG = icu_date,
 RIGHTARG = icu_interval
);
