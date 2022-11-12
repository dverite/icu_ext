-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.7'" to load this file. \quit

CREATE OR REPLACE FUNCTION icu_confusable_string_skeleton(
 string text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_confusable_string_skeleton'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_confusable_string_skeleton(text)
IS 'Get the skeleton for an input string';
