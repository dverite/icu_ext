-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION icu_ext UPDATE TO '1.1'" to load this file. \quit

CREATE FUNCTION icu_char_name(
 c character
) RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

COMMENT ON FUNCTION icu_char_name(character)
 IS 'Return the Unicode character name corresponding to the first codepoint of the input';

CREATE FUNCTION icu_number_spellout(
 num float8,
 locale text
) RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

COMMENT ON FUNCTION icu_number_spellout(float8,text)
 IS 'Spell out the number according to the given locale';

CREATE FUNCTION icu_spoof_check(
  str text
) RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_spoof_check(text)
 IS 'Check whether the argument is likely to be an attempt at confusing a reader';

CREATE FUNCTION icu_confusable_strings_check(
  str1 text,
  str2 text
) RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION icu_confusable_strings_check(text,text)
IS 'Check whether the arguments are visually confusable with each other';
