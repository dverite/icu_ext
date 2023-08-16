# Date and time functionalities

While PostgreSQL core datatypes `date`, `timestamp` and `timestamptz`
are meant for the **gregorian calendar** only, ICU provides support for
many calendars:

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

These calendars provide different names for days, months, eras, and also
may have different numbers of days within months and months within years.


Also, ICU provides localized date and time inputs and outputs that differ in some aspects from PostgreSQL. The time and date fields are described in [Formatting Dates and Times](https://unicode-org.github.io/icu/userguide/format_parse/datetime/).

ICU date and time functionalities are exposed in two ways:

1. through functions like `icu_parse_date` and `icu_format_date`. Besides the date or time value and the format strings, these functions take an optional ICU locale argument which may specify a calendar, for instance `fr@calendar=buddhist`.

1. through the data types `icu_date`, `icu_timestamptz`, and `icu_interval`, and the settings `icu_ext.date_format`, `icu_ext.timestamptz_format`, `icu_ext.locale`.
Again the calendar can be specified in the locale.

## Functions

### icu_parse_date

### icu_format_date

## Types
### icu_date
It differs only from the core built-in type `date` in the input and output formats that are accepted. `icu_date` text representation works with respect to `icu_ext.date_format` if set, and otherwise with the default format of the current ICU locale.
To express non-finite dates, use: `'infinity'::date::icu_date`.

Internally, the representation is the same as the `date` type, and `icu_date` can be cast directly to and from `date`.


### icu_timestamptz
It differs only from the core built-in type `timestamptz` (also called `timestamp with time zone`) in the input and output formats that are accepted. The text representation for `icu_timestamptz` works with respect to `icu_ext.timestamp_format` if set, and otherwise with the default format of the current ICU locale.
To express non-finite timestamps, use: `'infinity'::timestamptz::icu_timestamptz`.

Internally, the representation if the same as the `timestamptz` type, and `icu_timestamptz` can be cast directly to and from `timestamptz`.

### icu_interval

Like the `interval` built-in data type, it represents spans of time,
but differs in not assuming that one year always equals 12 months. How
spans of time are added to dates and timestamps depend on the current
calendar. For instance, in the ethiopic traditional calendar, years
have 13 months. It also accepts a simplified text representation
compared to the `interval` data type.

The representation is `'<number>Y <number>M <number>D <number>h <number>m <number>s'`
for respectively years, months, days, hours, minutes and seconds, with only the
`seconds` field accepting decimal numbers with a fractional part (the
other fields must be integers). The fields with value of zero are normally not displayed.


`icu_interval` can be cast to and from `interval`.


## Configurable settings


### icu_ext.date_format
### icu_ext.timestamptz_format
### icu_ext.locale

