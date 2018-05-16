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
[icu_default_locale](#icu_default_locale)  
[icu_line_boundaries](#icu_line_boundaries)  
[icu_locales_list](#icu_locales_list)  
[icu_number_spellout](#icu_number_spellout)
[icu_sentence_boundaries](#icu_sentence_boundaries)  
[icu_set_default_locale](#icu_set_default_locale)  
[icu_sort_key](#icu_sort_key)  
[icu_version](#icu_version)  
[icu_word_boundaries](#icu_word_boundaries)  

These functions work in both Unicode and non-Unicode databases.

<a id="icu_version"></a>
### icu_version()

  Returns the version of the ICU library linked with the server.

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

Lists the attributes and version of an ICU collation, returned as a set
of `(attribute,value)` tuples.  The `collator` argument must designate
an [ICU collator](http://userguide.icu-project.org/collation/api) and accepts
several different syntaxes. In particular, 
a [locale ID](http://userguide.icu-project.org/locale) or (if ICU>=54) a
[language tag](http://www.unicode.org/reports/tr35/tr35-collation.html#Collation_Settings)
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

      attribute |   value   
     -----------+-----------
      kn        | false
      kb        | true
      kk        | false
      ka        | noignore
      ks        | level3
      kf        | false
      kc        | false
      version   | 153.64.29


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
### icu_sort_key(`string` text, `collator` text)

Returns the binary sort key (type: `bytea`) corresponding to the
string with the given collation.
See http://userguide.icu-project.org/collation/architecture#TOC-Sort-Keys

`collator` is an ICU BCP-47 tag that is independent from the collations
instantiated in PostgreSQL.

Binary sort keys may be useful to circumvent the core PostgreSQL
limitation that two strings that differ in their byte representation
are not considered equal (see for instance [this thread](https://www.postgresql.org/message-id/7f0120e8945c4befac964777d31912d7%40exmbdft5.ad.twosigma.com) in the pgsql-bugs mailing-list for a discussion of this problem in relation with the ICU integration).

You may order or rank by binary sort keys, or materialize them in a unique
index to achieve at the SQL level what cannot be done internally by
PostgreSQL for case-insensitive or accent-insensitive collations.

The function is declared IMMUTABLE to be usable in indexes, but be
aware that it's only true as far as the version of the locale
associated with the collation doesn't change. (Typically it changes
between major ICU versions). In short, consider rebuilding
the affected indexes on ICU upgrades.

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
### icu_compare(`string1` text, `string2` text, `collator` text)

Compare two strings with the given collation.
Return the result as a signed integer, similarly to strcoll(),
that is, the result is negative if string1 < string2,
zero if string = string2, and positive if string1 > string2.

`collator` is an ICU BCP-47 tag that is independent from the collations
instantiated in PostgreSQL.

Example: case-sensitive, accent-insensitive comparison:

    =# SELECT icu_compare('abcé','abce','en-u-ks-level1-kc-true');
     icu_compare 
    -------------
               0

    =# SELECT icu_compare('Abcé','abce','en-u-ks-level1-kc-true');
     icu_compare 
    -------------
               1



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

(Note: the german output uses U+00AD (SOFT HYPHEN) to separate words)

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


## License

This project is licensed under the PostgreSQL License -- see [LICENSE.md](LICENSE.md).
