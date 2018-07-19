-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.2'" to load this file. \quit

CREATE FUNCTION icu_transforms_list(
)
RETURNS SETOF text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_transforms_list()
IS 'List the basic transforms available to icu_transform';

CREATE FUNCTION icu_transform(string text, trans text) RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_transform(text,text)
IS 'Apply a transformation through basic or compound transliterators and filters';
