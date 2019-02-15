-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.4'" to load this file. \quit

CREATE OR REPLACE FUNCTION icu_strpos(
 string text,
 "substring" text
) RETURNS int4
AS 'MODULE_PATHNAME', 'icu_strpos'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;

CREATE OR REPLACE FUNCTION icu_strpos(
 string text,
 "substring" text,
 collator text
) RETURNS int4
AS 'MODULE_PATHNAME', 'icu_strpos_coll'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 10;

CREATE OR REPLACE FUNCTION icu_replace(
 string text,
 "from" text,
 "to" text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_replace'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 100;

CREATE OR REPLACE FUNCTION icu_replace(
 string text,
 "from" text,
 "to" text,
 collator text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_replace_coll'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE COST 100;
