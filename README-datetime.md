# Date and time functionalities in icu_ext

Postgres core provides a comprehensive set of types and functions that
work with the widely used gregorian calendar, but does not support the
[traditional calendars](https://en.wikipedia.org/wiki/List_of_calendars)
used in some parts of the world.
These calendars differ mostly by when they start, how many
months there are in years and how they're named, and how many days
there are in months.


Since the ICU library can handle many of these traditional calendars,
`icu_ext` exposes them in Postgres through an alternate set of
SQL functions, types and operators.


## Locale settings

The calendar and the language used for date and time are
defined through a locale string: `language[_country][@calendar=caltype]`.

`language` and `country` are the usual short codes, as in `en_US` or `fr_CA`
(see the output of `icu_locales_list()` for a full list). The choice
of language selects the associated translations, and along with the country
it influences how dates are displayed when using the
basic formats with respect to cultural conventions (see the formatting
options below).

Default values will be guessed from the environment when the
language or calendar are not specified.

The accepted values for `caltype` are, as of ICU 70:

* buddhist
* chinese
* coptic
* dangi
* ethiopic
* ethiopic-amete-alem
* gregorian
* hebrew
* indian
* islamic
* islamic-civil
* islamic-rgsa
* islamic-tbla
* islamic-umalqura
* iso8601
* japanese
* persian
* roc

The locale can be passed to the `icu_parse_date()` and
`icu_format_date()` functions, or assigned to the `icu_ext.locale`
configuration setting to affect the behavior of the `icu_date`
and `icu_timestamptz` types implemented by the extension.

## Format strings for dates and timestamp
The fields available in the text representation of date and timestamps
are described in [Formatting Dates and Times](https://unicode-org.github.io/icu/userguide/format_parse/datetime/) (ICU documentation).
The format strings composed of these fields are passed to
`icu_format_date`, `icu_parse_date`, and used in the configuration
settings `icu_ext.timestamptz_format` and `icu_ext.date_format`
described below.

As an alternative to specifying individuals fields and separators, the
format string can consist of a reference to a basic format,
as described in the [CLDR](https://cldr.unicode.org/translation/date-time/date-time-patterns)
:
- `{short}`
- `{medium}`
- `{long}`
- `{full}`

The format code must be enclosed by curly brackets as shown in the
list, with nothing else in the format string.
When using these forms, which fields are displayed and in what order is determined
by the language and country of the ICU locale.

These values match the ICU enum [UDateFormatStyle](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/udat_8h.html#adb4c5a95efb888d04d38db7b3efff0c5)

Dates can also be expressed relatively to the current day with the `relative` keyword
added. The formats can be expressed as:

- `{short relative}`
- `{medium relative}`
- `{long relative}`
- `{full relative}`


## Functions taking core types

<a id="icu_format_date"></a>
### icu_format_date (`input` date, `format` text [,`locale` text])
Return the string representing the input date with the given `format`
and `locale` as described above. 
If `locale` is not specified, the current ICU locale is used.

Example:
```sql
=> select icu_format_date('2020-12-31'::date, '{medium}', 'en@calendar=ethiopic');
   icu_format_date
----------------------
 Tahsas 22, 2013 ERA1
```
<a id="icu_format_datetime"></a>
### icu_format_datetime (`input` timestamptz, `format` text [,`locale` text])
Return the string representing the time stamp wih time zone `ts`with the given `format`
and `locale` as described above. 
If `locale` is not specified, the current ICU locale is used.


Example:

    => SELECT icu_format_datetime(
		 now(),
		 'GGGG dd/MMMM/yyyy HH:mm:ss.SSS z',
		 'fr@calendar=buddhist'
	   );
	              icu_format_datetime
	------------------------------------------------
	 ère bouddhique 22/septembre/2566 14:55:48.133 UTC+2

<a id="icu_parse_date"></a>
### icu_parse_date (`input` text, `format` text [,`locale` text])
Return a `date` resulting from parsing the input string
according to `format` (see "format strings" above).

The function will error out if the input string interpreted with the
given `format` and `locale` does not strictly match the format
or cannot be converted into a date.
When `locale` is not specified, the current ICU locale is used.

Example:

	=> SET icu_ext.locale TO '@calendar=buddhist';
	=> SELECT icu_parse_date('25/09/2566', 'dd/MM/yyyy');
	 icu_parse_date 
	----------------
	 2023-09-25

<a id="icu_parse_datetime"></a>
### icu_parse_datetime (`input` text, `format` text [,`locale` text])

Return a `timestamp with time zone` resulting from parsing the input string
according to `format`. This is similar to `icu_parse_date()` except that
it parses a full timestamp instead of a date.

Example:

	=> SELECT icu_parse_datetime(
	  '11/Meskerem/2016 14:57:17',
	  'dd/MMMM/yyyy HH:mm:ss',
	  'en@calendar=ethiopic'
	); 
	   icu_parse_datetime   
	------------------------
	 2023-09-22 14:57:17+02


## Custom types
### icu_date
It differs from the core built-in type `date` in the input and output formats that are accepted. `icu_date` text representation works with respect to `icu_ext.date_format` if set, and otherwise with the default format of the current ICU locale.
To express non-finite dates, use `'infinity'::date::icu_date`.

Internally, the representation is the same as the `date` type, and `icu_date` can be cast implicitly to and from `date`.

Example:
```sql
 CREATE TABLE events(ev_name text, ev_date icu_date);

 INSERT INTO events VALUES('birthday', '2023-07-31'::date);

 SET icu_ext.locale TO 'orm@calendar=ethiopic';

 SELECT * FROM events;
 
 +----------+--------------------+
 | ev_name  |      ev_date       |
 +----------+--------------------+
 | birthday | 24-Hamle-2015 ERA1 |
 +----------+--------------------+
 
```


### icu_timestamptz

It differs from the core built-in type `timestamp with time zone` (or
`timestamptz` in short) in the input and output formats that are
accepted. The text representation for `icu_timestamptz` works with
respect to `icu_ext.timestamp_format` if set, and otherwise with the
default format of the current ICU locale.  To express non-finite
timestamps, use `'infinity'::timestamptz::icu_timestamptz`.

Internally, the representation is the same as the `timestamptz` type, and `icu_timestamptz` can be cast directly to and from `timestamptz`.

### icu_interval

Like the `interval` built-in data type, it represents spans of time
with years, months, days and microseconds components that are
meant to process calendar-aware calculations.

It differs from `interval` in not assuming that one year always equals
12 months. For instance, in the ethiopic calendar, there are 13 months
in a year. How spans of time are added to dates and timestamps depend
on the current calendar. `icu_interval` accepts the same textual inputs
as the `interval` data type. It also shares pretty much the same output
except for not converting months to years.

`icu_interval` can be cast from `interval`.

Example:
```sql
select '25 months'::interval, '25 months'::icu_interval;
+---------------+--------------+
|   interval    | icu_interval |
+---------------+--------------+
| 2 years 1 mon | 25 mons      |
+---------------+--------------+

```

## Operators

### icu_interval * int

Multiply each component of the interval (years, months...) by the integer number.
This operator is commutative.

### icu_date + icu_interval

Add the years, months, days and time from the interval to the date,
with respect to the rules of the calendar of the current locale (`icu_ext.locale`).

### icu_date - icu_interval

Substract the years, months, days and time from the interval to the date,
with respect to the rules of the calendar of the current locale (`icu_ext.locale`).

### icu_timestamptz + icu_interval

Add the years, months, days and time from the interval to the timestamp,
with respect to the rules of the calendar of the current locale (`icu_ext.locale`).

This operator is commutative.

### icu_timestamptz - icu_interval

Subtract the years, months, days and time from the interval to the timestamp,
with respect to the rules of the calendar of the current locale (`icu_ext.locale`).

### icu_interval + icu_interval

Add the intervals. The result does not depend on the current calendar.

### icu_interval - icu_interval

Subtract the intervals. The result does not depend on the current calendar.

## Configurable settings

There are three configuration settings that work together to control
input and output of the `icu_date` and `icu_timestamptz` types.

### icu_ext.locale

Locale to use for input/output and calendar-dependent calculations,
as described in "Locale format and settings" above.

```
-- vietnamese language, buddhist calendar
SET icu_ext.locale TO 'vi@calendar=buddhist';

SET icu_ext.timestamptz_format TO '{long}';

SELECT now()::icu_timestamptz;
                      now                       
------------------------------------------------
 Ngày 22 tháng 9 năm 2566 BE lúc 15:57:13 GMT+2

```

### icu_ext.date_format

Format string used for the text representation of the `icu_date` datatype, both for input and output. 
The format is described in [Formatting Dates and Times](https://unicode-org.github.io/icu/userguide/format_parse/datetime/) (ICU documentation).

The default value for this setting is `{medium}`.

### icu_ext.timestamptz_format

Format string used for the text representation of the `icu_timestamptz` datatype, both for input and output. 
The format is described in [Formatting Dates and Times](https://unicode-org.github.io/icu/userguide/format_parse/datetime/) (ICU documentation).

This setting also accepts the same references to basic formats (short, medium, ...) as `icu_ext.date_format`, and its default value is `{medium}`.
