/* icu_ext.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION icu_ext" to load this file. \quit

CREATE FUNCTION icu_version() RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION icu_version()
 IS 'Version of the ICU library currently in use';

CREATE FUNCTION icu_unicode_version() RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION icu_unicode_version()
 IS 'Version of the Unicode standard used by ICU';

CREATE FUNCTION icu_collation_attributes(
 IN collator text,
 IN exclude_defaults bool default false,
 OUT attribute text,
 OUT value text
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_collation_attributes(text,bool)
 IS 'List the attributes of an ICU collation';


CREATE FUNCTION icu_locales_list (
 OUT name text,
 OUT country text,
 OUT country_code text,
 OUT language text,
 OUT language_code text,
 OUT script text,
 OUT direction text
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION icu_locales_list()
 IS 'List the available ICU locales with their main properties';


CREATE FUNCTION icu_default_locale() RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION icu_default_locale()
 IS 'Return the ICU locale currently used by default';

/* Set the default locale to some name and return
   the canonicalized name. */
CREATE FUNCTION icu_set_default_locale(text) RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_set_default_locale(text)
 IS 'Set the ICU locale used by default';


/* See http://userguide.icu-project.org/boundaryanalysis */
CREATE FUNCTION icu_character_boundaries(
  contents text,
  locale text
) RETURNS SETOF text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;


COMMENT ON FUNCTION icu_character_boundaries(text,text)
 IS 'Split text into characters, using boundary positions according to Unicode rules with the specified locale';

CREATE FUNCTION icu_word_boundaries(
  contents text,
  locale text,
  OUT tag int,
  OUT contents text
) RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_word_boundaries(text,text)
 IS 'Split text into words, using boundary positions according to Unicode rules with the specified locale';

CREATE FUNCTION icu_line_boundaries(
  contents text,
  locale text,
  OUT tag int,
  OUT contents text
) RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_line_boundaries(text,text)
 IS 'Split text into parts between which line breaks may occur, using rules of the specified locale';

CREATE FUNCTION icu_sentence_boundaries(
  contents text,
  locale text,
  OUT tag int,
  OUT contents text
) RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

COMMENT ON FUNCTION icu_sentence_boundaries(text,text)
 IS 'Split text into sentences, according to Unicode rules with the specified locale';

CREATE FUNCTION icu_compare(
 str1 text,
 str2 text,
 collator text
) RETURNS int
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

COMMENT ON FUNCTION icu_compare(text,text,text)
 IS 'Compare two string with the given collation and return an signed int like strcoll';

CREATE FUNCTION icu_sort_key(
 str text,
 collator text
) RETURNS bytea
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

COMMENT ON FUNCTION icu_sort_key(text,text)
 IS 'Compute the binary sort key for the string given the collation';


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
