# icu_ext

An extension to expose functionality from [ICU](http://icu-project.org) to PostgreSQL applications.

It requires PostgreSQL version 10 or newer, configured with ICU
(--with-icu).

Note: this text is in GitHub Flavored Markdown format. Please see the version
[on github](https://github.com/dverite/icu_ext/blob/master/README.md)
if it's rendered weirdly elsewhere.

## Installation
The Makefile uses the [PGXS infrastructure](https://www.postgresql.org/docs/current/static/extend-pgxs.html) to find include and library files and determine the install location.  
Build and install with:

	$ make
	$ (sudo) make install

## Functions

### Quick links (in alphabetical order)
[icu_char_name](#icu_char_name)  
[icu_character_boundaries](#icu_character_boundaries)  
[icu_collation_attributes](#icu_collation_attributes)  
[icu_compare](#icu_compare)  
[icu_confusable_strings_check](#icu_confusable_strings_check)  
[icu_default_locale](#icu_default_locale)  
[icu_is_normalized](#icu_is_normalized)  
[icu_line_boundaries](#icu_line_boundaries)  
[icu_locales_list](#icu_locales_list)  
[icu_normalize](#icu_normalize)  
[icu_number_spellout](#icu_number_spellout)  
[icu_replace](#icu_replace)  
[icu_sentence_boundaries](#icu_sentence_boundaries)  
[icu_set_default_locale](#icu_set_default_locale)  
[icu_sort_key](#icu_sort_key)  
[icu_spoof_check](#icu_spoof_check)  
[icu_strpos](#icu_strpos)  
[icu_transform](#icu_transform)  
[icu_transforms_list](#icu_transforms_list)  
[icu_unicode_version](#icu_unicode_version)  
[icu_version](#icu_version)  
[icu_word_boundaries](#icu_word_boundaries)  

These functions work in both Unicode and non-Unicode databases.

<a id="icu_version"></a>
### icu_version()

  Returns the version of the ICU library linked with the server.


<a id="icu_unicode_version"></a>
### icu_unicode_version()

  Returns the version of the Unicode standard used by the ICU library
  linked with the server.


<a id="icu_locales_list"></a>
### icu_locales_list()

  Returns a table-type list of available ICU locales with their main properties
  (country code and name, language code and name, script, direction).
  When translations are available, the country and language names are
  localized with the default ICU locale, configurable with
  `icu_set_default_locale()`. Set it to `en` to force english names.

Examples:

      =# SELECT * FROM icu_locales_list() where name like 'es%' limit 5;
        name  |    country    | country_code | language | language_code | script | direction 
      --------+---------------+--------------+----------+---------------+--------+-----------
       es     |               |              | Spanish  | spa           |        | LTR
       es_419 | Latin America |              | Spanish  | spa           |        | LTR
       es_AR  | Argentina     | ARG          | Spanish  | spa           |        | LTR
       es_BO  | Bolivia       | BOL          | Spanish  | spa           |        | LTR
       es_CL  | Chile         | CHL          | Spanish  | spa           |        | LTR

     =# SELECT name,country FROM icu_locales_list() where script='Simplified Han';
         name    |       country       
     ------------+---------------------
      zh_Hans    | 
      zh_Hans_CN | China
      zh_Hans_HK | Hong Kong SAR China
      zh_Hans_MO | Macau SAR China
      zh_Hans_SG | Singapore

This list is obtained independently from the collations declared to
PostgreSQL (found in `pg_collation`).

<a id="icu_collation_attributes"></a>
### icu_collation_attributes(`collator` text [, `exclude_defaults` bool])

Lists the attributes, version and display name of an ICU collation,
returned as a set of `(attribute,value)` tuples.  The `collator`
argument must designate an
[ICU collator](http://userguide.icu-project.org/collation/api) and accepts
several different syntaxes. In particular, 
a [locale ID](http://userguide.icu-project.org/locale) or (if ICU>=54)
[language tags](http://www.unicode.org/reports/tr35/tr35-collation.html#Collation_Settings)
may be used.
Note that this argument is **not** a reference to a PostgreSQL collation, and
that this function does not depend on whether a corresponding
collation has been instantiated in the database with [`CREATE
COLLATION`](https://www.postgresql.org/docs/current/static/sql-createcollation.html).
To query the properties of an already created PostgreSQL
ICU collation, refer to `pg_collation.collcollate` (which corresponds
to the `lc_collate` argument of CREATE COLLATION).

     =# SELECT a.attribute,a.value FROM pg_collation
          JOIN LATERAL icu_collation_attributes(collcollate) a
          ON (collname='fr-CA-x-icu');

       attribute  |       value
     -------------+-------------------
      displayname | français (Canada)
      kn          | false
      kb          | true
      kk          | false
      ka          | noignore
      ks          | level3
      kf          | false
      kc          | false
      version     | 153.80.33


`icu_collation_attributes()` is useful to check that the settings embedded into a
collation name activate the intended options, because ICU parses them
in a way that non-conformant parts tend to be silently ignored,
and because the interpretation
somewhat depends on the ICU version (in particular, pre-54 versions do
not support options expressed as BCP-47 tags). It may be also
useful to search existing collations by their properties.  When
`exclude_defaults` is set to `true`, attributes that have their
default value are filtered out, to put in evidence the specifics
of collations. For instance, to find the only collations that use
`shifted` for the `Alternate` attribute:

     =# SELECT collname,collcollate,a.attribute,a.value FROM pg_collation
         JOIN LATERAL icu_collation_attributes(collcollate,true) a
         ON (attribute='ka') ;

       collname   | collcollate | attribute |  value  
     -------------+-------------+-----------+---------
      th-x-icu    | th          | ka        | shifted
      th-TH-x-icu | th-TH       | ka        | shifted
     (2 rows)

By default there is no filtering (`exclude_defaults` = false) so that
all attributes known by the function as well as the collation version
number are reported.

Example of checking a collation without any reference to `pg_collation`:

     =# SELECT * FROM icu_collation_attributes('fr-u-ks-level2-kn');
       attribute |  value   
     -----------+----------
      kn        | true
      kb        | false
      kk        | false
      ka        | noignore
      ks        | level2
      kf        | false
      kc        | false
      version   | 153.64

`icu_collation_attributes()` will error out if ICU is unable to open
a collator with the given argument.

<a id="icu_sort_key"></a>
### icu_sort_key(`string` text [, `collator` text])

Returns the binary sort key (type: `bytea`) corresponding to the
string with the given collation.
See http://userguide.icu-project.org/collation/architecture#TOC-Sort-Keys

When a `collator` argument is passed, it is interpreted as an ICU
locale independently of the persistent collations instantiated in
the database.
When there is no `collator` argument, the collation associated
to `string` gets used to generate the sort key. It must be
an ICU collation or the function will error out. This form
with a single argument is faster due to Postgres keeping its
collations "open" (in the sense of `ucol_open()/ucol_close()`) for the
duration of the session, whereas the other form with the
explicit `collator` argument does open and close the ICU collation
for each call.

Binary sort keys may be useful to circumvent a core PostgreSQL
limitation that two strings that differ in their byte representation
are never considered equal by deterministic collations (see for instance [this thread](https://www.postgresql.org/message-id/7f0120e8945c4befac964777d31912d7%40exmbdft5.ad.twosigma.com) in the pgsql-bugs mailing-list for a discussion of this problem in relation with the ICU integration).
With PostgreSQL 12 or newer versions, the "deterministic" property can be set
to `false` by [`CREATE COLLATION`](https://www.postgresql.org/docs/current/sql-createcollation.html) to request that string comparisons with these collations skip the tie-breaker.
With older versions, "deterministic" is always `true`.

You may order or rank by binary sort keys, or materialize them in a unique
index to achieve at the SQL level what cannot be done internally by
persistent collations, either because PostgreSQL is not recent enough
or because you don't want or lack the permission to instantiate
nondeterministic collations.


The function is declared IMMUTABLE to be usable in indexes, but please be
aware that it's only true as far as the "version" of the collation
doesn't change. (Typically it changes with every version of Unicode). In
short, consider rebuilding the affected indexes on ICU upgrades.

To simply compare pairs of strings, consider `icu_compare()` instead.

Example demonstrating a case-sensitive, accent-sensitive unique index:

    =# CREATE TABLE uniq(name text);

    =# CREATE UNIQUE INDEX idx ON uniq((icu_sort_key(name, 'fr-u-ks-level1')));

    =# INSERT INTO uniq values('été');
    INSERT 0 1

    =# INSERT INTO uniq values('Ête');
    ERROR:  duplicate key value violates unique constraint "idx"
    DETAIL:  Key (icu_sort_key(name, 'fr-u-ks-level1'::text))=(\x314f31) already exists.

    =# insert into uniq values('Êtes');
    INSERT 0 1

<a id="icu_compare"></a>
### icu_compare(`string1` text, `string2` text [, `collator` text])

Compare two strings with the given collation.
Return the result as a signed integer, similarly to strcoll(),
that is, the result is negative if string1 < string2,
zero if string = string2, and positive if string1 > string2.

When a `collator` argument is passed, it is taken as the ICU
locale (independently of the collations instantiated in the database)
to use to collate the strings.

When there is no `collator` argument, the collation associated
to `string1` and `string2` gets used for the comparison.
It must be an ICU collation and it must be the same for the two
arguments or the function will error out. With PostgreSQL 12 or newer,
it can be nondeterministic, but whether it is nondeterministic
or deterministic will not make any difference in the result of `icu_compare`,
contrary to comparisons done by PostgreSQL core with the equality operator.
The two-argument form is significantly faster due to Postgres keeping its
collations "open" (in the sense of `ucol_open()/ucol_close()`) for the
duration of the session, whereas the other form with the
explicit `collator` argument does open and close the ICU collation
for each call.


Example: case-sensitive, accent-insensitive comparison:

    =# SELECT icu_compare('abcé', 'abce', 'en-u-ks-level1-kc-true');
     icu_compare 
    -------------
               0

    =# SELECT icu_compare('Abcé', 'abce', 'en-u-ks-level1-kc-true');
     icu_compare 
    -------------
               1

With two arguments and a collation determined by the COLLATE clause:

    =# SELECT icu_compare('Abcé', 'abce' COLLATE "fr-x-icu");
     icu_compare 
    -------------
               1

With an implicit Postgres collation:

    =# CREATE COLLATION mycoll (locale='fr-u-ks-level1', provider='icu');
    CREATE COLLATION

    =# CREATE TABLE books (id int, title text COLLATE "mycoll");
    CREATE TABLE

    =# insert into books values(1, $$C'est l'été$$);
    INSERT 0 1

    =# select id,title from books where icu_compare (title, $$c'est l'ete$$) = 0;
     id |    title    
    ----+-------------
      1 | C'est l'été


<a id="icu_set_default_locale"></a>
### icu_set_default_locale(`locale` text)


Sets the default ICU locale for the session, and returns a canonicalized
version of the locale name. The POSIX syntax (`lang[_country[@attr]]`)
is accepted.  Call this function to change the output language of
`icu_locales_list()`.  
This setting should not have any effect on PostgreSQL core
functions, at least as of PG version 10.

Warning: passing bogus contents to this function may freeze the
backend with older versions of ICU (seen with 52.1).


<a id="icu_default_locale"></a>
### icu_default_locale()

Returns the name of the default ICU locale as a text. The initial value
is automatically set by ICU from the environment.

<a id="icu_character_boundaries"></a>
### icu_character_boundaries(`string` text, `locale` text)


Break down the string into its characters and return them as a set of text.
This is comparable to calling `regexp_split_to_table` with an empty regexp,
with some differences, for instance:
- CRLF sequences do not get split into two characters.
- Sequences with a base and a combining character are kept together.

Example (the "e" followed by the combining acute accent U+0301 may be
rendered as an accented e or differently depending on your browser):

    =# SELECT * FROM icu_character_boundaries('Ete'||E'\u0301', 'fr') as chars;
     chars
    -------
     E
     t
     é

See [Boundary
Analysis](http://userguide.icu-project.org/boundaryanalysis) in the
ICU User Guide for more information.

<a id="icu_word_boundaries"></a>
### icu_word_boundaries (`string` text, `locale` text)

Break down the string into words and non-words constituents,
and return them in a set of (tag, contents) tuples.
`tag` has values from the [UWordBreak enum](http://icu-project.org/apiref/icu4c/ubrk_8h_source.html) defined in ubrk.h indicating the nature of the piece of contents.
The current values are:

    UBRK_WORD_NONE           = 0,
    UBRK_WORD_NUMBER         = 100,
    UBRK_WORD_LETTER         = 200,
    UBRK_WORD_KANA           = 300,
    UBRK_WORD_IDEO           = 400,  /* up to 500 */

(strictly speaking, any number between the lower and the upper bounds
may be counted, as these numbers are meant to be intervals inside
which new subdivisions may be added in future versions of ICU).

Example:

    =# SELECT * FROM icu_word_boundaries($$I like O'Reilly books, like the japanese 初めてのPerl 第7版.$$ , 'en');
     tag | contents 
    -----+----------
     200 | I
       0 |  
     200 | like
       0 |  
     200 | O'Reilly
       0 |  
     200 | books
       0 | ,
       0 |  
     200 | like
       0 |  
     200 | the
       0 |  
     200 | japanese
       0 |  
     400 | 初めて
     400 | の
     200 | Perl
       0 |  
     400 | 第
     100 | 7
     400 | 版
       0 | .

or to count words in english:

     =# SELECT count(*) FROM icu_words_boundaries($$piece of text$$, 'en_US')
        WHERE tag=200;


<a id="icu_line_boundaries"></a>
### icu_line_boundaries (`string` text, `locale` text)

Split the string into pieces where a line break may occur, according
to the Unicode line breaking algorithm defined in [UAX #14](http://unicode.org/reports/tr14/),
and return them in a set of (tag, contents) tuples.
`tag` has values from the [ULineBreakTag enum](http://icu-project.org/apiref/icu4c/ubrk_8h_source.html) defined in ubrk.h indicating the nature of the break.
The current values are:

    UBRK_LINE_SOFT      = 0,
    UBRK_LINE_HARD      = 100,  /* up to 200 */

(strictly speaking, any number between the lower and the upper bounds
may be counted, as these numbers are meant to be intervals inside
which new subdivisions may be added in future versions of ICU).

Example:

    =#  SELECT *,convert_to( contents, 'utf-8') from icu_line_boundaries(
    $$Thus much let me avow--You are not wrong, who deem
    That my days have been a dream;
    Yet if hope has flown away
    In a night, or in a day,$$
    , 'en');

     tag | contents |    convert_to    
    -----+----------+------------------
     100 |         +| \x0a
         |          | 
       0 | Thus     | \x5468757320
       0 | much     | \x6d75636820
       0 | let      | \x6c657420
       0 | me       | \x6d6520
     100 | avow--  +| \x61766f772d2d0a
         |          | 
       0 | You      | \x596f7520
       0 | are      | \x61726520
       0 | not      | \x6e6f7420
       0 | wrong,   | \x77726f6e672c20
       0 | who      | \x77686f20
     100 | deem    +| \x6465656d0a
         |          | 
       0 | That     | \x5468617420
       0 | my       | \x6d7920
       0 | days     | \x6461797320
       0 | have     | \x6861766520
       0 | been     | \x6265656e20
       0 | a        | \x6120
     100 | dream;  +| \x647265616d3b0a
         |          | 
       0 | Yet      | \x59657420
       0 | if       | \x696620
       0 | hope     | \x686f706520
       0 | has      | \x68617320
       0 | flown    | \x666c6f776e20
     100 | away    +| \x617761790a
         |          | 
       0 | In       | \x496e20
       0 | a        | \x6120
       0 | night,   | \x6e696768742c20
       0 | or       | \x6f7220
       0 | in       | \x696e20
       0 | a        | \x6120
       0 | day,     | \x6461792c


<a id="icu_sentence_boundaries"></a>
### icu_sentence_boundaries (`string` text, `locale` text)


Split the string into sentences, according the Unicode text segmentation
rules defined in [UAX #29](http://unicode.org/reports/tr29/),
and return them in a set of (tag, contents) tuples.
`tag` has values from the [USentenceBreakTag enum](http://icu-project.org/apiref/icu4c/ubrk_8h_source.html) defined in ubrk.h indicating the nature of the break.
The current values are:

    UBRK_SENTENCE_TERM  = 0,
    UBRK_SENTENCE_SEP   = 100, /* up to 200 */

(strictly speaking, any number between the lower and the upper bounds
may be counted, as these numbers are meant to be intervals inside
which new subdivisions may be added in future versions of ICU).

Example:

    =# SELECT * FROM icu_sentence_boundaries('Mr. Barry Sheene was born in 1950. He was a motorcycle racer.',
       'en-u-ss-standard');
     tag |              contents               
    -----+-------------------------------------
       0 | Mr. Barry Sheene was born in 1950. 
       0 | He was a motorcycle racer.

Note: "Mr." followed by a space is recognized by virtue of the locale
as an abbreviation of the english "Mister", rather than the end of a
sentence.

<a id="icu_number_spellout"></a>
### icu_number_spellout (`number` double precision, `locale` text)

Return the spelled out text corresponding to the number expressed in the given locale.

Example:

    =# SELECT loc, icu_number_spellout(1234, loc)
        FROM (values ('en'),('fr'),('de'),('ru'),('ja')) AS s(loc);

      loc |            icu_number_spellout
     -----+-------------------------------------------
      en  | one thousand two hundred thirty-four
      fr  | mille deux cent trente-quatre
      de  | ein­tausend­zwei­hundert­vier­und­dreißig
      ru  | одна тысяча двести тридцать четыре
      ja  | 千二百三十四

(Note: the german output uses U+00AD (SOFT HYPHEN) to separate words. Github's
markdown to HTML conversion seems to remove them, so in the above text the spellout
might appear like a single long word.)

<a id="icu_char_name"></a>
### icu_char_name(`c` character)

Return the Unicode character name corresponding to the first codepoint of the input.

Example:

    =# SELECT c, to_hex(ascii(c)), icu_char_name(c)
       FROM regexp_split_to_table('El Niño', '') as c;

      c | to_hex |          icu_char_name
     ---+--------+---------------------------------
      E | 45     | LATIN CAPITAL LETTER E
      l | 6c     | LATIN SMALL LETTER L
        | 20     | SPACE
      N | 4e     | LATIN CAPITAL LETTER N
      i | 69     | LATIN SMALL LETTER I
      ñ | f1     | LATIN SMALL LETTER N WITH TILDE
      o | 6f     | LATIN SMALL LETTER O


<a id="icu_spoof_check"></a>
### icu_spoof_check (`string` text)

Return a boolean indicating whether the argument is likely to be an
attempt at confusing a reader.  The implementation is based on Unicode
Technical Reports [#36](http://unicode.org/reports/tr36) and
[#39](http://unicode.org/reports/tr39) and uses the ICU default
settings for spoof checks.

Example:

    =# SELECT txt, icu_spoof_check(txt) FROM (VALUES ('paypal'), (E'p\u0430ypal')) AS s(txt);
      txt   | icu_spoof_check
    --------+-----------------
     paypal | f
     pаypal | t

(Note: The second character in the second row is U+0430 (CYRILLIC
SMALL LETTER A) instead of the genuine ASCII U+0061 (LATIN SMALL LETTER A))


<a id="icu_confusable_strings_check"></a>
### icu_confusable_strings_check(`string1` text, `string2` text)

Return a boolean indicating whether the string arguments are visually
confusable with each other, according to data described in [Unicode
Technical Report #39](http://unicode.org/reports/tr39/#Confusable_Detection).
The settings and comparison levels are ICU defaults. For strictly
identical strings, it returns true.

Example:

    =# SELECT txt, icu_confusable_strings_check('phil', txt) AS confusable
        FROM (VALUES ('phiL'), ('phiI'), ('phi1'), (E'ph\u0131l')) AS s(txt);

     txt  | confusable
    ------+------------
     phiL | f
     phiI | t
     phi1 | t
     phıl | t

<a id="icu_transform"></a>
### icu_transform (`string` text, `transformations` text)

Return a string with some transformations applied. This function essentially calls
ICU's [utrans_transUChars()](http://icu-project.org/apiref/icu4c/utrans_8h.html#af415d8aa51e79d4494ebb8ef8fc76ae2).

The first argument is the string to transform, and the second is the transformation
to apply, expressed as a sequence of transforms and filters
(see the [ICU user guide on transforms](http://userguide.icu-project.org/transforms/general)
and the output of `icu_transforms_list()` mentioned below).

Examples:

Transliterate:

    =# select icu_transform('Владимир Путин', 'Cyrl-Latn'); -- just 'Latin' would work here too
     icu_transform
    ----------------
     Vladimir Putin

Transform Unicode names into the corresponding characters:

    =# select icu_transform('10\N{SUPERSCRIPT MINUS}\N{SUPERSCRIPT FOUR}'
			   '\N{MICRO SIGN}m = 1 \N{ANGSTROM SIGN}',
			 'Name-Any');
     icu_transform
    ---------------
     10⁻⁴µm = 1 Å

Remove diacritics (generalized "unaccent") through Unicode decomposition.

     =# select icu_transform('1 Å', 'any-NFD; [:nonspacing mark:] any-remove; any-NFC');

     icu_transform
    ---------------
     1 A

Generate hexadecimal codepoints for non-ASCII characters:

    =# select icu_transform('Ich muß essen.', '[:^ascii:]; Hex');
        icu_transform
    ---------------------
     Ich mu\u00DF essen.


<a id="icu_transforms_list"></a>
### icu_transforms_list ()

Return the list of built-in transliterations or transforms, as a set of text,
corresponding to "Basic IDs" in [ICU documentation](http://userguide.icu-project.org/transforms/general).
The initial set of transforms are transliterations between scripts
(like `Katakana-Latin` or `Latin-Cyrillic`), but they're
supplemented with functionalities related to accents, casing,
Unicode composition and decomposition with combining characters
and other conversions.

Values from this list are meant to be used individually as the 2nd argument of
`icu_transform()`, or assembled with semi-colon separators to form
compound transforms, possibly with filters added to limit the set of characters
to transform.

<a id="icu_strpos"></a>
### icu_strpos(`string` text, `substring` text [, `collator` text])

Like `strpos(text,text)` in Postgres core, except that it uses the
linguistic rules of `collator` to search `substring` in `string`,
and that it supports nondeterministic collations seamlessly.
When the substring is not found, it returns 0. Otherwise, It returns
the 1-based position of the first match of `substring` inside
`string`, or 1 if `substring` is empty.
When `collator` is not passed, the collation of the
arguments is used. As with the other functions in this extension, the
two-argument form is faster since it can keep the ICU collation open
across function calls.

Example:

    -- Search in names independently of punctuation, case and accents
    =# select name from addresses where
         icu_strpos(name, 'jeanrene', 'fr-u-ks-level1-ka-shifted') > 0

	   name
    ------------------
     jean-rené dupont
     Jean-René  Dupont
     jeanrenédupont

<a id="icu_replace"></a>
### icu_replace(`string` text, `from` text, `to` text  [, `collator` text])

Like `replace(string text, from text, to text)` in Postgres core,
except it uses the linguistic rules of `collator` to search
`substring` in `string` instead of a byte-wise comparison. It also
supports nondeterministic collations to search `from` as a substring.
It returns `strings` with all substrings that match `from` replaced by `to`.
When `collator` is not passed, the collation of the arguments is used,
which is faster because the ICU collation can be kept open across
function calls.

Example:

    -- Collation comparing independently of punctuation, case and accents
    =# CREATE COLLATION ciaipi (provider = icu, locale = 'und-u-ks-level1-ka-shifted');

    -- Replace names matching 'jeanrene' by a placeholder
    =# select s.n,  icu_replace(n, 'jeanrene', '{firstname}' collate "ciaipi")
         from (values('jeanrenédupont'),('Jean-René  Dupont')) as s(n) ;

	     n         |     icu_replace
    -------------------+---------------------
     jeanrenédupont    | {firstname}dupont
     Jean-René  Dupont | {firstname}  Dupont


<a id="icu_normalize"></a>
### icu_normalize(`string` text, `form` text)

Return `string` transformed into the Unicode normalized `form`,
which must be `nfc`, `nfkc`, `nfd`, or `nfkd` (upper case or mixed
case variants are accepted). Returns NULL if any input argument is NULL.
The database must use an Unicode encoding, which means UTF-8 in practice.
See the Unicode Annex [UAX #15](http://unicode.org/reports/tr15/#Introduction)
for an introduction on Unicode normal forms.

Example:

	=# select icu_normalize('éte'||E'\u0301', 'nfc') = E'ét\u00E9';
	 ?column?
	----------
	 t

<a id="icu_is_normalized"></a>
### icu_is_normalized(`string` text, `form` text)

Return true if `string` is in the Unicode normalized `form`,
which must be `nfc`, `nfkc`, `nfd`, or `nfkd` (upper case or mixed
case variants are accepted). Returns false otherwise, or NULL if
any input argument is NULL. The database must use an Unicode encoding,
which means UTF-8 in practice.

Example:

	 =# SELECT icu_is_normalized('ét'||E'\u0301', 'nfc');
	  icu_is_normalized
	 -------------------
	  f

	 =# SELECT icu_is_normalized('ét'||E'\u0301', 'nfd');
	  icu_is_normalized
	 -------------------
	  t



## License

This project is licensed under the PostgreSQL License -- see [LICENSE.md](LICENSE.md).
