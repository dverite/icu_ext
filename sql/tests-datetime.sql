/* test date and time support */

\set format unaligned

set icu_ext.locale to 'en@calendar=gregorian';
set icu_ext.timestamptz_format to 'YYYY-MM-dd HH:mm:ss';
set timezone to 'Europe/Paris';

-- DST transition to summer time
select
 '2023-03-25 00:00:00'::timestamptz + '26.5 hours'::interval AS "core",
 '2023-03-25 00:00:00'::icu_timestamptz + '26.5 hours'::icu_interval AS "ext";

set icu_ext.locale to 'en@calendar=ethiopic';
set icu_ext.date_format to '{short}';
set icu_ext.timestamptz_format to '{short}';

-- 13-month year with 5 days in the last month
select '1/13/2016 ERA1'::icu_date + icu_interval '12 months' as d1,
  '1/13/2016 ERA1'::icu_date + icu_interval '13 months' as d2,
  '1/13/2016 ERA1'::icu_date + icu_interval '1 year' as d3;

select '13/5/2016 ERA1'::icu_date + 1;

set icu_ext.locale to 'en@calendar=gregorian';

select icu_parse_date('17/10/2023', 'dd/MM/yyyy');

select icu_parse_datetime('17/10/2023', 'dd/MM/yyyy');

select icu_parse_datetime('17/10/2023 12:02:40.653', 'dd/MM/yyyy HH:mm:ss.S');

set timezone to 'GMT';
select icu_parse_datetime('17/10/2023 12:02:40.653', 'dd/MM/yyyy HH:mm:ss.S');
