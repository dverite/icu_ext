-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.9'" to load this file. \quit

CREATE FUNCTION icu_char_type(character) RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

COMMENT ON FUNCTION icu_char_type(character)
 IS 'Return the general category value for the code point';

CREATE FUNCTION icu_char_ublock_id(character) RETURNS smallint
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

COMMENT ON FUNCTION icu_char_ublock_id(character)
 IS 'Return a numerical ID corresponding to the Unicode block of the given code point';

CREATE FUNCTION icu_unicode_blocks(
 OUT block_id int2,
 OUT block_name text
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STABLE;

COMMENT ON FUNCTION icu_unicode_blocks()
 IS 'Return the list of Unicode blocks with their internal ID';
