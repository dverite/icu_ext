-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.7'" to load this file. \quit

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
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

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
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_format_date(timestamptz,text)
IS 'Convert a time stamp into a string according to the given format and default locale';
