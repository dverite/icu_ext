-- complain if script is sourced in psql, rather than via CREATE/ALTER EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.6'" to load this file. \quit

CREATE OR REPLACE FUNCTION icu_normalize(
 string text,
 form text
) RETURNS text
AS 'MODULE_PATHNAME', 'icu_normalize'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_normalize(text,text)
IS 'Normalize the string into one of NFC, NFD, NFKC or NFKD Unicode forms';


CREATE OR REPLACE FUNCTION icu_is_normalized(
 string text,
 form text
) RETURNS bool
AS 'MODULE_PATHNAME', 'icu_is_normalized'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_is_normalized(text,text)
IS 'Test if the string is normalized in one of NFC, NFD, NFKC or NFKD Unicode forms';
