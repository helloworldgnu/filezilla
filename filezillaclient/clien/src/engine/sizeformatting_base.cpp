#include <filezilla.h>
#include "sizeformatting_base.h"
#include "optionsbase.h"
#ifndef __WXMSW__
#include <langinfo.h>
#endif

namespace {
const wxChar prefix[] = { ' ', 'K', 'M', 'G', 'T', 'P', 'E' };

wxString ToString(int64_t n, wxChar const* const sepBegin = 0, wxChar const* const sepEnd = 0)
{
	wxString ret;
	if (!n) {
		ret = _T("0");
	}
	else {
		bool neg = false;
		if (n < 0) {
			n *= -1;
			neg = true;
		}

		wxChar buf[60];
		wxChar * const end = &buf[sizeof(buf) / sizeof(wxChar) - 1];
		wxChar * p = end;

		int d = 0;
		while (n != 0) {
			*--p = '0' + n % 10;
			n /= 10;

			if (sepBegin && !(++d % 3) && n != 0) {
				wxChar *q = p - (sepEnd - sepBegin);
				for (wxChar const* s = sepBegin; s != sepEnd; ++s) {
					*q++ = *s;
				}
				p -= sepEnd - sepBegin;
			}
		}

		if (neg) {
			*--p = '-';
		}

		ret.assign(p, end - p);
	}
	return ret;
}
}

wxString CSizeFormatBase::Format(COptionsBase* pOptions, int64_t size, bool add_bytes_suffix, CSizeFormatBase::_format format, bool thousands_separator, int num_decimal_places)
{
	wxASSERT(format != formats_count);
	wxASSERT(size >= 0);
	if( size < 0 ) {
		size = 0;
	}

	if (format == bytes) {
		wxString result = FormatNumber(pOptions, size, &thousands_separator);

		if (!add_bytes_suffix)
			return result;
		else {
			// wxPLURAL has no support for wxLongLong
			int last;
			if (size > 1000000000)
				last = 1000000000 + (size % 1000000000);
			else
				last = size;
			return wxString::Format(wxPLURAL("%s byte", "%s bytes", last), result);
		}
	}

	wxString places;

	int divider;
	if (format == si1000)
		divider = 1000;
	else
		divider = 1024;

	// Exponent (2^(10p) or 10^(3p) depending on option
	int p = 0;

	int64_t r = size;
	int remainder = 0;
	bool clipped = false;
	while (r > divider && p < 6) {
		int64_t const rr = r / divider;
		if (remainder != 0)
			clipped = true;
		remainder = static_cast<int>(r - rr * divider);
		r = rr;
		++p;
	}
	if (!num_decimal_places) {
		if (remainder != 0 || clipped)
			++r;
	}
	else if (p) { // Don't add decimal places on exact bytes
		if (format != si1000) {
			// Binary, need to convert 1024 into range from 1-1000
			if (clipped) {
				++remainder;
				clipped = false;
			}
			remainder = (int)ceil((double)remainder * 1000 / 1024);
		}

		int max;
		switch (num_decimal_places)
		{
		default:
			num_decimal_places = 1;
			// Fall-through
		case 1:
			max = 9;
			divider = 100;
			break;
		case 2:
			max = 99;
			divider = 10;
			break;
		case 3:
			max = 999;
			break;
		}

		if (num_decimal_places != 3) {
			if (remainder % divider)
				clipped = true;
			remainder /= divider;
		}

		if (clipped)
			remainder++;
		if (remainder > max) {
			r++;
			remainder = 0;
		}

		wxChar fmt[] = _T("%00d");
		fmt[2] = '0' + num_decimal_places;
		places.Printf(fmt, remainder);
	}

	wxString result = ToString(r, 0, 0);
	if (!places.empty()) {
		const wxString& sep = GetRadixSeparator();

		result += sep;
		result += places;
	}
	result += ' ';

	static wxChar byte_unit = 0;
	if (!byte_unit) {
		wxString t = _("B <Unit symbol for bytes. Only translate first letter>");
		byte_unit = t[0];
	}

	if (!p)
		return result + byte_unit;

	result += prefix[p];
	if (format == iec)
		result += 'i';

	result += byte_unit;

	return result;
}

wxString CSizeFormatBase::Format(COptionsBase* pOptions, int64_t size, bool add_bytes_suffix /*=false*/)
{
	const _format format = _format(pOptions->GetOptionVal(OPTION_SIZE_FORMAT));
	const bool thousands_separator = pOptions->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0;
	const int num_decimal_places = pOptions->GetOptionVal(OPTION_SIZE_DECIMALPLACES);

	return Format(pOptions, size, add_bytes_suffix, format, thousands_separator, num_decimal_places);
}

wxString CSizeFormatBase::FormatUnit(COptionsBase* pOptions, int64_t size, CSizeFormatBase::_unit unit, int base /*=1024*/)
{
	_format format = _format(pOptions->GetOptionVal(OPTION_SIZE_FORMAT));
	if (base == 1000)
		format = si1000;
	else if (format != si1024)
		format = iec;
	return wxString::Format(_T("%s %s"), FormatNumber(pOptions, size), GetUnit(pOptions, unit, format));
}

wxString CSizeFormatBase::GetUnitWithBase(COptionsBase* pOptions, _unit unit, int base)
{
	_format format = _format(pOptions->GetOptionVal(OPTION_SIZE_FORMAT));
	if (base == 1000)
		format = si1000;
	else if (format != si1024)
		format = iec;
	return GetUnit(pOptions, unit, format);
}

wxString CSizeFormatBase::GetUnit(COptionsBase* pOptions, _unit unit, CSizeFormatBase::_format format /*=formats_count*/)
{
	wxString ret;
	if (unit != byte)
		ret = prefix[unit];

	if (format == formats_count)
		format = _format(pOptions->GetOptionVal(OPTION_SIZE_FORMAT));
	if (format == iec || format == bytes)
		ret += 'i';

	static wxChar byte_unit = 0;
	if (!byte_unit)
	{
		wxString t = _("B <Unit symbol for bytes. Only translate first letter>");
		byte_unit = t[0];
	}

	ret += byte_unit;

	return ret;
}

const wxString& CSizeFormatBase::GetThousandsSeparator()
{
	static wxString sep;
	static bool separator_initialized = false;
	if (!separator_initialized)
	{
		separator_initialized = true;
#ifdef __WXMSW__
		wxChar tmp[5];
		int count = ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, tmp, 5);
		if (count)
			sep = tmp;
#else
		char* chr = nl_langinfo(THOUSEP);
		if (chr && *chr)
		{
			sep = wxString(chr, wxConvLibc);
		}
#endif
		if (sep.size() > 5) {
			sep = sep.Left(5);
		}
	}

	return sep;
}

const wxString& CSizeFormatBase::GetRadixSeparator()
{
	static wxString sep;
	static bool separator_initialized = false;
	if (!separator_initialized)
	{
		separator_initialized = true;

#ifdef __WXMSW__
		wxChar tmp[5];
		int count = ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, tmp, 5);
		if (!count)
			sep = _T(".");
		else
		{
			tmp[4] = 0;
			sep = tmp;
		}
#else
		char* chr = nl_langinfo(RADIXCHAR);
		if (!chr || !*chr)
			sep = _T(".");
		else
		{
			sep = wxString(chr, wxConvLibc);
		}
#endif
	}

	return sep;
}

wxString CSizeFormatBase::FormatNumber(COptionsBase* pOptions, int64_t size, bool* thousands_separator /*=0*/)
{
	wxString sep;
	wxChar const* sepBegin = 0;
	wxChar const* sepEnd = 0;

	if ((!thousands_separator || *thousands_separator) && pOptions->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0) {
		sep = GetThousandsSeparator();
		if (!sep.empty()) {
			sepBegin = sep.c_str();
			sepEnd = sepBegin + sep.size();
		}
	}

	return ToString(size, sepBegin, sepEnd);
}
