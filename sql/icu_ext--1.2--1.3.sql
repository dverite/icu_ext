-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.3'" to load this file. \quit

CREATE OR REPLACE FUNCTION icu_sort_key(
 str text,
 collator text
) RETURNS bytea
AS 'MODULE_PATHNAME', 'icu_sort_key_coll'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;

COMMENT ON FUNCTION icu_sort_key(text,text)
 IS 'Compute the binary sort key for the string given the collation';

CREATE OR REPLACE FUNCTION icu_sort_key(
 str text
) RETURNS bytea
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;

COMMENT ON FUNCTION icu_sort_key(text)
 IS 'Compute the binary sort key with the collate of the string';


CREATE OR REPLACE FUNCTION icu_compare(
 str1 text,
 str2 text
) RETURNS int
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_compare(text,text)
 IS 'Compare two strings with their ICU collation and return a signed int like strcoll';

CREATE OR REPLACE FUNCTION icu_compare(
 str1 text,
 str2 text,
 collator text
) RETURNS int
AS 'MODULE_PATHNAME', 'icu_compare_coll'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_compare(text,text,text)
 IS 'Compare two strings with the given collation and return a signed int like strcoll';
