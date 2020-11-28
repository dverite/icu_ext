/*
 * icu_ext.c
 *
 * Part of icu_ext: a PostgreSQL extension to expose functionality from ICU
 * (see http://icu-project.org)
 *
 * By Daniel Vérité, 2018-2019. See LICENSE.md
 */

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/tuplestore.h"
#include "utils/pg_locale.h"

#include "unicode/ucnv.h"
#include "unicode/ucol.h"
#include "unicode/uloc.h"
#include "unicode/umachine.h"
#include "unicode/uscript.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"
#include "unicode/uvernum.h"

#include "icu_ext.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(icu_version);
PG_FUNCTION_INFO_V1(icu_unicode_version);
PG_FUNCTION_INFO_V1(icu_collation_attributes);
PG_FUNCTION_INFO_V1(icu_locales_list);
PG_FUNCTION_INFO_V1(icu_default_locale);
PG_FUNCTION_INFO_V1(icu_set_default_locale);
PG_FUNCTION_INFO_V1(icu_compare);
PG_FUNCTION_INFO_V1(icu_compare_coll);
PG_FUNCTION_INFO_V1(icu_case_compare);
PG_FUNCTION_INFO_V1(icu_sort_key);
PG_FUNCTION_INFO_V1(icu_sort_key_coll);
PG_FUNCTION_INFO_V1(icu_char_name);


Datum
icu_version(PG_FUNCTION_ARGS)
{
	UVersionInfo version;
	char buf[U_MAX_VERSION_STRING_LENGTH+1];

	u_getVersion(version);
	u_versionToString(version, buf);

	PG_RETURN_TEXT_P(cstring_to_text(buf));
}

Datum
icu_unicode_version(PG_FUNCTION_ARGS)
{
	UVersionInfo version;
	char buf[U_MAX_VERSION_STRING_LENGTH+1];

	u_getUnicodeVersion(version);
	u_versionToString(version, buf);

	PG_RETURN_TEXT_P(cstring_to_text(buf));
}


/* Get the value of a collation attribute, aborting on error. */
static UColAttributeValue
get_attribute(const UCollator *coll, UColAttribute attr)
{
	UColAttributeValue val;
	UErrorCode status = U_ZERO_ERROR;

	val = ucol_getAttribute(coll, attr, &status);
	if (status != U_ZERO_ERROR) {
		elog(ERROR, "ucol_getAttribute failed");
	}
	return val;
}

/*
 * Return (attribute,value) tuples for all attributes of a collation,
 * with keys and values matching options defined at
 * http://unicode.org/reports/tr35/tr35-collation.html#Setting_Options
 * Optionally, the attributes kept at their default values are not
 * included in the results.
 */
Datum
icu_collation_attributes(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	Datum values[2];
	bool nulls[2];
	char *txt;
	const char *locale;
	bool include_defaults = !(PG_GETARG_BOOL(1));
	UCollator	*collator = NULL;
	UErrorCode	status = U_ZERO_ERROR;
	UColAttributeValue u_attr_val;

	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("set-valued function called in context that cannot accept a set")));

	/* Switch into long-lived context to construct returned data structures */
	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* Open ICU collation */
	locale = text_to_cstring(PG_GETARG_TEXT_P(0));
	collator = ucol_open(locale, &status);
	if (!collator) {
		elog(ERROR, "failed to open collation");
	}

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	memset(nulls, 0, sizeof(nulls));

	/* name (not a real attribute, added for convenience) */
	if (include_defaults)
	{
		/* Use a large initial buffer to avoid bug ICU-21157 */
		UChar dname_local[500];
		UChar *dname = dname_local;
		char *buf;
		int32_t ulen;

		ulen = uloc_getDisplayName(locale,
								   NULL,
								   dname,
								   sizeof(dname_local)/sizeof(UChar),
								   &status);
		if (status == U_BUFFER_OVERFLOW_ERROR)
		{
			dname = palloc((ulen+1)*sizeof(UChar));
			status = U_ZERO_ERROR;
			ulen = uloc_getDisplayName(locale,
									   NULL,
									   dname,
									   ulen,
									   &status);
		}
		if (U_FAILURE(status))
			elog(ERROR, "uloc_getDisplayName failed: %s", u_errorName(status));

		icu_from_uchar(&buf, dname, ulen);

		values[0] = CStringGetTextDatum("displayname");
		values[1] = CStringGetTextDatum(buf);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* UCOL_NUMERIC_COLLATION (key:kn) */
	u_attr_val = get_attribute(collator, UCOL_NUMERIC_COLLATION);
	if (include_defaults || u_attr_val != UCOL_OFF)
	{
		txt = (u_attr_val == UCOL_OFF) ? "false" : "true";
		values[0] = CStringGetTextDatum("kn");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* UCOL_FRENCH_COLLATION (key:kb, rule:[backwards 2]) */
	u_attr_val = get_attribute(collator, UCOL_FRENCH_COLLATION);
	if (include_defaults || u_attr_val != UCOL_OFF)
	{
		txt = (u_attr_val == UCOL_OFF) ? "false" : "true";
		values[0] = CStringGetTextDatum("kb");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* UCOL_NORMALIZATION_MODE (key:kk)*/
	u_attr_val = get_attribute(collator, UCOL_NORMALIZATION_MODE);
	if (include_defaults || u_attr_val != UCOL_OFF)
	{
		txt = (u_attr_val == UCOL_OFF) ? "false" : "true";
		values[0] = CStringGetTextDatum("kk");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* UCOL_ALTERNATE_HANDLING (key:ka) */
	u_attr_val = get_attribute(collator, UCOL_ALTERNATE_HANDLING);
	if (include_defaults || u_attr_val != UCOL_NON_IGNORABLE)
	{
		switch (u_attr_val) {
		case UCOL_NON_IGNORABLE:
			txt = "noignore";
			break;
		case UCOL_SHIFTED:
			txt = "shifted";
			break;
		default:
			txt = "";
			break;
		}
		values[0] = CStringGetTextDatum("ka");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* UCOL_STRENGTH (key:ks) */
	u_attr_val = get_attribute(collator, UCOL_STRENGTH);
	if (include_defaults || u_attr_val != UCOL_TERTIARY)
	{
		switch(u_attr_val) {
		case UCOL_PRIMARY:
			txt = "level1";
			break;
		case UCOL_SECONDARY:
			txt = "level2";
			break;
		case UCOL_TERTIARY:
			txt = "level3";
			break;
		case UCOL_QUATERNARY:
			txt = "level4";
			break;
		case UCOL_IDENTICAL:
			txt = "identic";
			break;
		default:
			txt = "";
			break;
		}
		values[0] = CStringGetTextDatum("ks");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* UCOL_CASE_FIRST (key:kf) */
	u_attr_val = get_attribute(collator, UCOL_CASE_FIRST);
	if (include_defaults || u_attr_val != UCOL_OFF)
	{
		switch(u_attr_val) {
		case UCOL_OFF:
			txt = "false";
			break;
		case UCOL_LOWER_FIRST:
			txt = "lower";
			break;
		case UCOL_UPPER_FIRST:
			txt = "upper";
			break;
		default:
			txt = "";
			break;
		}
		values[0] = CStringGetTextDatum("kf");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}


	/* UCOL_CASE_LEVEL (key:kc) */
	u_attr_val = get_attribute(collator, UCOL_CASE_LEVEL);
	if (include_defaults || u_attr_val != UCOL_OFF)
	{
		txt = (u_attr_val == UCOL_OFF) ? "false" : "true";
		values[0] = CStringGetTextDatum("kc");
		values[1] = CStringGetTextDatum(txt);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* Max variable (key:kv) */
	{
		UColReorderCode reorder_code = ucol_getMaxVariable(collator);
		const char *code_name = NULL;
		switch(reorder_code)
		{
			case UCOL_REORDER_CODE_SPACE:
				code_name = "space";
				break;
			case UCOL_REORDER_CODE_PUNCTUATION:
				code_name = "punct";
				break;
			case UCOL_REORDER_CODE_SYMBOL:
				code_name = "symbol";
				break;
			case UCOL_REORDER_CODE_CURRENCY:
				code_name = "currency";
				break;
			case UCOL_REORDER_CODE_DIGIT:
				code_name = "digit";
				break;
			default:
				break;
		}
		/* "punct" is the default. Omit it unless include_defaults is set */
		if (code_name != NULL && (include_defaults ||
					  reorder_code != UCOL_REORDER_CODE_PUNCTUATION))
		{
			values[0] = CStringGetTextDatum("kv");
			values[1] = CStringGetTextDatum(code_name);
			tuplestore_putvalues(tupstore, tupdesc, values, nulls);
		}
	}

	/* Reorder codes (key:kr) */
	{
		UErrorCode status = U_ZERO_ERROR;
		StringInfoData aggr_values;  /* 4-letter codes separated by hyphens */
		int32_t *reorder_codes = NULL;
		int32_t nb_reorderings = ucol_getReorderCodes(collator,
												NULL,
												0,
												&status);
		if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
			elog(ERROR, "uloc_getReorderCodes failed: %s", u_errorName(status));

		initStringInfo(&aggr_values);

		if (nb_reorderings > 0)
		{
			reorder_codes = palloc(nb_reorderings*sizeof(int32_t));
			status = U_ZERO_ERROR;
			nb_reorderings = ucol_getReorderCodes(collator,
												  reorder_codes,
												  nb_reorderings,
												  &status);
			if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
				elog(ERROR, "uloc_getReorderCodes failed: %s", u_errorName(status));
		}

		for (uint32_t idx=0; idx < nb_reorderings; idx++)
		{
			const char *value = NULL;
			if (reorder_codes[idx] >= UCOL_REORDER_CODE_FIRST)
			{
				switch(reorder_codes[idx])
				{
					case UCOL_REORDER_CODE_SPACE:
						value = "space";
						break;
					case UCOL_REORDER_CODE_PUNCTUATION:
						value = "punct";
						break;
					case UCOL_REORDER_CODE_SYMBOL:
						value = "symbol";
						break;
					case UCOL_REORDER_CODE_CURRENCY:
						value = "currency";
						break;
					case UCOL_REORDER_CODE_DIGIT:
						value = "digit";
						break;
				}
			}
			else
			{
				value  = uscript_getShortName((UScriptCode)reorder_codes[idx]);
			}
			if (value != NULL)
			{
				if (idx >= 1)
					appendStringInfoChar(&aggr_values, '-');
				appendStringInfoString(&aggr_values, value);
			}
		}
		if (aggr_values.len > 0)
		{
			values[0] = CStringGetTextDatum("kr");
			values[1] = CStringGetTextDatum(aggr_values.data);
			tuplestore_putvalues(tupstore, tupdesc, values, nulls);
		}
	}

	/* version (not a real attribute, added for convenience) */
	if (include_defaults)
	{
		UVersionInfo version;
		char buf[U_MAX_VERSION_STRING_LENGTH+1];
		ucol_getVersion(collator, version);
		u_versionToString(version, buf);
		values[0] = CStringGetTextDatum("version");
		values[1] = CStringGetTextDatum(buf);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	tuplestore_donestoring(tupstore);
	ucol_close(collator);

	return (Datum) 0;
}

/*
 * Add a piece of text as a new Datum value, setting it to NULL
 * if it's empty.
 */
static int
add_string(const char* value, int column, Datum *values, bool *nulls)
{
	if (*value)
		values[column] = CStringGetTextDatum(value);
	else
		values[column] = (Datum)0;
	nulls[column] = (*value == '\0');
	return column+1;
}


/*
 * Interface to uloc_getAvailable() for all locales.
 * Return a table of available locales with their main properties.
 */
Datum
icu_locales_list(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	int32_t loc_count = uloc_countAvailable();
	int32_t i;
	Datum values[7];
	bool nulls[7];

	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("set-valued function called in context that cannot accept a set")));

	/* Switch into long-lived context to construct returned data structures */
	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);


	for (i=0; i < loc_count; i++)
	{
		UErrorCode	status = U_ZERO_ERROR;
		int col_num = 0;
		const char *p = uloc_getAvailable(i);

		/* Name */
		col_num = add_string(p, col_num, values, nulls);


		/* Country */
		{
			UChar country_buf[200];
			char* country;		/* with the database encoding */

			uloc_getDisplayCountry(p, NULL /*ULOC_ENGLISH*/, country_buf,
								   sizeof(country_buf)/sizeof(UChar), &status);
			if (U_FAILURE(status))
				elog(ERROR, "uloc_getDisplayCountry() failed on locale '%s': %s",
					 p, u_errorName(status));
			icu_from_uchar(&country, country_buf, u_strlen(country_buf));
			col_num = add_string(country, col_num, values, nulls);
		}

		/* Country code */
		col_num = add_string(uloc_getISO3Country(p), col_num, values, nulls);

		/* Language */
		{
			UChar lang_buf[200];
			char* language;
			uloc_getDisplayLanguage(p, NULL, lang_buf,
									sizeof(lang_buf)/sizeof(UChar), &status);
			if (U_FAILURE(status))
				elog(ERROR, "uloc_getDisplayLanguage() failed on locale '%s': %s",
					 p, u_errorName(status));
			icu_from_uchar(&language, lang_buf, u_strlen(lang_buf));
			col_num = add_string(language, col_num, values, nulls);
		}

		/* Language code */
		col_num = add_string(uloc_getISO3Language(p), col_num, values, nulls);

		/* Script */
		{
			UChar script_buf[100];
			char* script;
			uloc_getDisplayScript(p, NULL, script_buf, sizeof(script_buf)/sizeof(UChar),
								  &status);
			if (U_FAILURE(status))
				elog(ERROR, "uloc_getDisplayScript() failed on locale '%s': %s",
					 p, u_errorName(status));
			icu_from_uchar(&script, script_buf, u_strlen(script_buf));
			col_num = add_string(script, col_num, values, nulls);
		}

		/* Character orientation */
		{
			const char* layout;
			ULayoutType t = uloc_getCharacterOrientation(p, &status);
			if (U_FAILURE(status))
				elog(ERROR, "uloc_getCharacterOrientation() failed on locale '%s': %s",
					 p, u_errorName(status));

			switch (t)
			{
			case ULOC_LAYOUT_LTR:
				layout = "LTR"; break;
			case ULOC_LAYOUT_RTL:
				layout = "RTL"; break;
			case ULOC_LAYOUT_TTB:
				layout = "TTB"; break;
			case ULOC_LAYOUT_BTT:
				layout = "BTT"; break;
			default:
				layout = ""; break;
			}

			col_num = add_string(layout, col_num, values, nulls);
		}

		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	tuplestore_donestoring(tupstore);
	return (Datum) 0;
}


/*
 * Return the default locale.
 */
Datum
icu_default_locale(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(uloc_getDefault()));
}


/*
 * Set the default locale to some name and return its canonicalized
 * name.
 * Warning: seen with ICU-52, passing a locale name with BCP-47
 * extensions makes ICU never return from uloc_setDefault() (it seems
 * to wait for some internal lock).
 * Note that ICU documentation says about uloc_setDefault():
 * "Do not use unless you know what you are doing."
 * This is useful in icu_ext to get translated versions of country
 * and language names from icu_locales_list().
 */
Datum
icu_set_default_locale(PG_FUNCTION_ARGS)
{
	UErrorCode	status = U_ZERO_ERROR;
	const char *locname = text_to_cstring(PG_GETARG_TEXT_P(0));
	char buf[1024];

	uloc_setDefault(locname, &status);
	if (U_FAILURE(status))
		elog(ERROR, "failed to set ICU locale: %s", u_errorName(status));

	uloc_canonicalize(locname, buf, sizeof(buf), &status);
	if (U_FAILURE(status))
		PG_RETURN_NULL();
	else
		PG_RETURN_TEXT_P(cstring_to_text(buf));
}

/*
 * Get the UCollator object corresponding to the collation in input.
 * This UCollator is kept open by the backend and pointed to by the
 * cached pg_locale_t object.
 */
UCollator*
ucollator_from_coll_id(Oid collid)
{
	pg_locale_t pg_locale;

	if (collid == DEFAULT_COLLATION_OID || !OidIsValid(collid))
	{
		/*
		 * This will need to be changed when a db will be able to have
		 * an ICU collation by default (not possible as of PG11).
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which ICU collation to use"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}

	pg_locale = pg_newlocale_from_collation(collid);

	if (!pg_locale || pg_locale->provider != 'i')
	{
		ereport(ERROR,
				(errcode(ERRCODE_COLLATION_MISMATCH),
				 errmsg("the collation provider of the input string must be ICU")));
	}
	return pg_locale->info.icu.ucol;
}

/*
 * The actual collation-aware comparison happens here.
 * the UCollator comes either from a cached pg_locale_t
 * or is just opened and closed immediately by icu_ext.
 */
static UCollationResult
our_strcoll(text *txt1, text *txt2, UCollator *collator )
{
	UCollationResult result;
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	int32_t len2 = VARSIZE_ANY_EXHDR(txt2);


	if (GetDatabaseEncoding() == PG_UTF8)
	{
		/* use the UTF-8 representation directly if possible */
		UErrorCode	status = U_ZERO_ERROR;

		result = ucol_strcollUTF8(collator,
								  text_to_cstring(txt1), len1,
								  text_to_cstring(txt2), len2,
								  &status);
		if (U_FAILURE(status))
			elog(ERROR, "ICU strcoll failed: %s", u_errorName(status));
	}
	else
	{
		int32_t ulen1, ulen2;
		UChar *uchar1, *uchar2;

		ulen1 = icu_to_uchar(&uchar1, text_to_cstring(txt1), len1);
		ulen2 = icu_to_uchar(&uchar2, text_to_cstring(txt2), len2);

		result = ucol_strcoll(collator,
							  uchar1, ulen1,
							  uchar2, ulen2);

		pfree(uchar1);
		pfree(uchar2);
	}
	return result;
}

/*
 * Compare two strings with the given collation.
 * Return the result as a signed integer, similarly to strcoll().
 */
Datum
icu_compare_coll(PG_FUNCTION_ARGS)
{
	text *txt1 = PG_GETARG_TEXT_PP(0);
	text *txt2 = PG_GETARG_TEXT_PP(1);
	const char *collname = text_to_cstring(PG_GETARG_TEXT_P(2));
	UCollator	*collator = NULL;
	UErrorCode	status = U_ZERO_ERROR;
	UCollationResult result;

	collator = ucol_open(collname, &status);
	if (!collator || U_FAILURE(status)) {
		elog(ERROR, "failed to open collation: %s", u_errorName(status));
	}

	result = our_strcoll(txt1, txt2, collator);
	ucol_close(collator);

	PG_RETURN_INT32(result == UCOL_EQUAL ? 0 :
					(result == UCOL_GREATER ? 1 : -1));
}

/*
 * Compare two strings with the collation of the function,
 * which must be an ICU collation.
 * Return the result as a signed integer, similarly to strcoll().
 */
Datum
icu_compare(PG_FUNCTION_ARGS)
{
	text *txt1 = PG_GETARG_TEXT_PP(0);
	text *txt2 = PG_GETARG_TEXT_PP(1);
	UCollator *collator = ucollator_from_coll_id(PG_GET_COLLATION());
	UCollationResult result;

	result = our_strcoll(txt1, txt2, collator);

	PG_RETURN_INT32(result == UCOL_EQUAL ? 0 :
					(result == UCOL_GREATER ? 1 : -1));
}


/*
 * Compare two strings with full case folding.
 */
Datum
icu_case_compare(PG_FUNCTION_ARGS)
{
	text *txt1 = PG_GETARG_TEXT_PP(0);
	int32_t len1 = VARSIZE_ANY_EXHDR(txt1);
	text *txt2 = PG_GETARG_TEXT_PP(1);
	int32_t len2 = VARSIZE_ANY_EXHDR(txt2);
	int32_t result;
	UChar *uchar1, *uchar2;

	(void)icu_to_uchar(&uchar1, text_to_cstring(txt1), len1);
	(void)icu_to_uchar(&uchar2, text_to_cstring(txt2), len2);

	result = u_strcasecmp(uchar1, uchar2, 0);

	pfree(uchar1);
	pfree(uchar2);

	PG_RETURN_INT32(result);
}

/*
 * Return a binary sort key corresponding to the string and
 * its collation (through a COLLATE clause).
 */
Datum
icu_sort_key(PG_FUNCTION_ARGS)
{
	text *txt = PG_GETARG_TEXT_PP(0);
    UCollator *collator = ucollator_from_coll_id(PG_GET_COLLATION());
	int32_t o_len = 1024;		/* first attempt */
	int32_t ulen;
	UChar *ustring;
	bytea *output;


	ulen = icu_to_uchar(&ustring, VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt));

	do
	{
		int32_t effective_len;
		output = (bytea*) palloc(o_len + VARHDRSZ);
		effective_len = ucol_getSortKey(collator,
										ustring,
										ulen,
										(uint8_t*)VARDATA(output),
										o_len);
		if (effective_len == 0)
		{
			elog(ERROR, "ucol_getSortKey() failed: internal error");
		}
		if (effective_len > o_len)
		{
			pfree(output);
			output = NULL;
		}
		o_len = effective_len;
	} while (output == NULL);	/* should loop at most once, if buffer too small */

	SET_VARSIZE(output, o_len + VARHDRSZ - 1);  /* -1 excludes the ending NUL byte */
	PG_RETURN_BYTEA_P(output);
}

/*
 * Return a binary sort key corresponding to the string and
 * the given collation.
 */
Datum
icu_sort_key_coll(PG_FUNCTION_ARGS)
{
	text *txt = PG_GETARG_TEXT_PP(0);
	const char *locname = text_to_cstring(PG_GETARG_TEXT_P(1));
	UCollator	*collator;
	UErrorCode	status = U_ZERO_ERROR;
	int32_t o_len = 1024;		/* first attempt */
	int32_t ulen;
	UChar *ustring;
	bytea *output;

	ulen = icu_to_uchar(&ustring, VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt));

	collator = ucol_open(locname, &status);
	if (!collator)
		elog(ERROR, "failed to open collation");

	do
	{
		int32_t effective_len;
		output = (bytea*) palloc(o_len + VARHDRSZ);
		effective_len = ucol_getSortKey(collator,
										ustring,
										ulen,
										(uint8_t*)VARDATA(output),
										o_len);
		if (effective_len == 0)
		{
			ucol_close(collator);
			elog(ERROR, "ucol_getSortKey() failed: internal error");
		}
		if (effective_len > o_len)
		{
			pfree(output);
			output = NULL;
		}
		o_len = effective_len;
	} while (output == NULL);	/* should loop at most once, if buffer too small */

	ucol_close(collator);

	SET_VARSIZE(output, o_len + VARHDRSZ - 1);  /* -1 excludes the ending NUL byte */
	PG_RETURN_BYTEA_P(output);
}

/* Return the first UChar32 of the char(1) string */
static UChar32
first_char32(BpChar* source)
{
	UChar32 c;
	UText *ut;
	int32_t ulen;
	UChar *ustring;
	UErrorCode status = U_ZERO_ERROR;

	ulen = icu_to_uchar(&ustring, VARDATA_ANY(source), VARSIZE_ANY_EXHDR(source));

	ut = utext_openUChars(NULL, ustring, ulen, &status);
	if (U_FAILURE(status))
		elog(ERROR, "utext_openUChars() failed: %s", u_errorName(status));
	c = utext_current32(ut);
	utext_close(ut);
	return c;
}

/*
 * Return the Unicode name corresponding to the the input character.
 */
Datum
icu_char_name(PG_FUNCTION_ARGS)
{
	BpChar *source = PG_GETARG_BPCHAR_PP(0);
	char local_buf[80];
	char *buffer;
	int32_t buflen = sizeof(local_buf);
	UChar32 first_char;
	int32_t ulen;
	UErrorCode status = U_ZERO_ERROR;

	first_char = first_char32(source);

	ulen = u_charName(first_char,
					  U_EXTENDED_CHAR_NAME,
					  local_buf,
					  buflen,
					  &status);
	if (status == U_BUFFER_OVERFLOW_ERROR)		/* buffer too small */
	{
		buffer = palloc((ulen+1)*sizeof(char));
		status = U_ZERO_ERROR;
		ulen = u_charName(first_char,
						  U_EXTENDED_CHAR_NAME,
						  buffer,
						  ulen+1,
						  &status);
	}
	else
		buffer = local_buf;

	if (U_FAILURE(status))
		elog(ERROR, "u_charName failed: %s", u_errorName(status));
	else
		PG_RETURN_TEXT_P(cstring_to_text(buffer));
}
