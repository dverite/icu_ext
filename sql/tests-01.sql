-- regression tests for icu_ext

CREATE EXTENSION icu_ext;

-- Check that the database has the built-in ICU collations
-- required by the tests
SELECT collname FROM pg_collation WHERE collname IN
 ('und-x-icu', 'en-x-icu')
ORDER BY collname;

-- icu_char_name
SELECT c, to_hex(ascii(c)), icu_char_name(c)
   FROM regexp_split_to_table('El Niño', '') as c;

-- icu_character_boundaries
SELECT * FROM icu_character_boundaries('Ete'||E'\u0301', 'fr') as chars;

-- icu_collation_attributes
SELECT * FROM icu_collation_attributes('en') WHERE attribute <> 'version';

-- icu_compare
SELECT icu_compare('abcé', 'abce', 'en@colStrength=primary;colCaseLevel=yes');
SELECT icu_compare('Abcé', 'abce' COLLATE "en-x-icu");

-- icu_confusable_strings_check
SELECT txt, icu_confusable_strings_check('phil', txt) AS confusable
    FROM (VALUES ('phiL'), ('phiI'), ('phi1'), (E'ph\u0131l')) AS s(txt);

-- icu_line_boundaries
SELECT *,convert_to( contents, 'utf-8')
FROM icu_line_boundaries(
$$Thus much let me avow
You are not wrong, who deem
That my days have been a dream;
Yet if hope has flown away
In a night, or in a day,$$
, 'en');

-- icu_number_spellout
SELECT loc, icu_number_spellout(1234, loc)
    FROM (values ('en'),('fr'),('de'),('ru'),('ja')) AS s(loc);

-- icu_replace
SELECT n,
   icu_replace(
      n,
     'jeanrene',
     '{firstname}',
     'und@colStrength=primary;colAlternate=shifted')
FROM (values('jeanrenédupont'),('Jean-René  Dupont')) as s(n)
ORDER BY n COLLATE "C";

-- icu_sentence_boundaries
SELECT * FROM icu_sentence_boundaries('Call me Mr. Brown. It''s a movie.',
 'en@ss=standard');

-- icu_strpos
SELECT v,icu_strpos('hey rene', v, 'und@colStrength=primary;colAlternate=shifted')
FROM (VALUES ('René'), ('rené'), ('Rene'), ('n'), ('në'), ('no'), (''), (null))
 AS s(v)
ORDER BY v COLLATE "C";

-- icu_transform
SELECT icu_transform('10\N{SUPERSCRIPT MINUS}\N{SUPERSCRIPT FOUR}'
		   '\N{MICRO SIGN}m = 1 \N{ANGSTROM SIGN}',
		 'Name-Any');

SELECT icu_transform('Ich muß essen.', '[:^ascii:]; Hex');

-- icu_word_boundaries
SELECT * FROM icu_word_boundaries($$Do you like O'Reilly books?$$, 'en');
