-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.4'" to load this file. \quit

CREATE OR REPLACE FUNCTION icu_strpos(
 haystack text,
 needle text
) RETURNS int4
AS 'MODULE_PATHNAME', 'icu_strpos'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;

CREATE OR REPLACE FUNCTION icu_strpos(
 haystack text,
 needle text,
 collator text
) RETURNS int4
AS 'MODULE_PATHNAME', 'icu_strpos_coll'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;

CREATE OR REPLACE FUNCTION icu_replace(
 haystack text,
 oldstr text,
 newstr text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_replace'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;
