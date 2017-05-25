#include <filezilla.h>
#include <gnutls/gnutls.h>
#include <sqlite3.h>
#include <random>
#include <cstdint>

#include "timeex.h"
#include "tlssocket.h"

wxString GetIPV6LongForm(wxString short_address)
{
	if (!short_address.empty() && short_address[0] == '[') {
		if (short_address.Last() != ']')
			return wxString();
		short_address.RemoveLast();
		short_address = short_address.Mid(1);
	}
	short_address.MakeLower();

	wxChar buffer[40] = { '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', ':',
						  '0', '0', '0', '0', 0
						};
	wxChar* out = buffer;

	const unsigned int len = short_address.Len();
	if (len > 39)
		return wxString();

	// First part, before possible ::
	unsigned int i = 0;
	unsigned int grouplength = 0;

	wxChar const* s = short_address.c_str(); // Get it zero-terminated.
	for (i = 0; i < len + 1; ++i) {
		const wxChar& c = s[i];
		if (c == ':' || !c) {
			if (!grouplength) {
				// Empty group length, not valid
				if (!c || s[i + 1] != ':')
					return wxString();
				++i;
				break;
			}

			out += 4 - grouplength;
			for (unsigned int j = grouplength; j > 0; j--)
				*out++ = s[i - j];
			// End of string...
			if (!c) {
				if (!*out)
					// ...on time
					return buffer;
				else
					// ...premature
					return wxString();
			}
			else if (!*out) {
				// Too long
				return wxString();
			}

			++out;

			grouplength = 0;
			if (s[i + 1] == ':') {
				++i;
				break;
			}
			continue;
		}
		else if ((c < '0' || c > '9') &&
				 (c < 'a' || c > 'f'))
		{
			// Invalid character
			return wxString();
		}
		// Too long group
		if (++grouplength > 4)
			return wxString();
	}

	// Second half after ::

	wxChar* end_first = out;
	out = &buffer[38];
	unsigned int stop = i;
	for (i = len - 1; i > stop; i--)
	{
		if (out < end_first)
		{
			// Too long
			return wxString();
		}

		const wxChar& c = s[i];
		if (c == ':')
		{
			if (!grouplength)
			{
				// Empty group length, not valid
				return wxString();
			}

			out -= 5 - grouplength;

			grouplength = 0;
			continue;
		}
		else if ((c < '0' || c > '9') &&
				 (c < 'a' || c > 'f'))
		{
			// Invalid character
			return wxString();
		}
		// Too long group
		if (++grouplength > 4)
			return wxString();
		*out-- = c;
	}
	if (!grouplength)
	{
		// Empty group length, not valid
		return wxString();
	}
	out -= 5 - grouplength;
	out += 2;

	int diff = out - end_first;
	if (diff < 0 || diff % 5)
		return wxString();

	return buffer;
}

int DigitHexToDecNum(wxChar c)
{
	if (c >= 'a')
		return c - 'a' + 10;
	if (c >= 'A')
		return c - 'A' + 10;
	else
		return c - '0';
}

bool IsRoutableAddress(const wxString& address, CSocket::address_family family)
{
	if (family == CSocket::ipv6)
	{
		wxString long_address = GetIPV6LongForm(address);
		if (long_address.size() != 39)
			return false;
		if (long_address[0] == '0')
		{
			// ::/128
			if (long_address == _T("0000:0000:0000:0000:0000:0000:0000:0000"))
				return false;
			// ::1/128
			if (long_address == _T("0000:0000:0000:0000:0000:0000:0000:0001"))
				return false;

			if (long_address.Left(30) == _T("0000:0000:0000:0000:0000:ffff:"))
			{
				// IPv4 mapped
				wxString ipv4 = wxString::Format(_T("%d.%d.%d.%d"),
						DigitHexToDecNum(long_address[30]) * 16 + DigitHexToDecNum(long_address[31]),
						DigitHexToDecNum(long_address[32]) * 16 + DigitHexToDecNum(long_address[33]),
						DigitHexToDecNum(long_address[35]) * 16 + DigitHexToDecNum(long_address[36]),
						DigitHexToDecNum(long_address[37]) * 16 + DigitHexToDecNum(long_address[38]));
				return IsRoutableAddress(ipv4, CSocket::ipv4);
			}

			return true;
		}
		if (long_address[0] == 'f')
		{
			if (long_address[1] == 'e')
			{
				// fe80::/10 (link local)
				const wxChar& c = long_address[2];
				int v;
				if (c >= 'a')
					v = c - 'a' + 10;
				else
					v = c - '0';
				if ((v & 0xc) == 0x8)
					return false;

				return true;
			}
			else if (long_address[1] == 'c' || long_address[1] == 'd')
			{
				// fc00::/7 (site local)
				return false;
			}
		}

		return true;
	}
	else
	{
		// Assumes address is already a valid IP address
		if (address.Left(3) == _T("127") ||
			address.Left(3) == _T("10.") ||
			address.Left(7) == _T("192.168") ||
			address.Left(7) == _T("169.254"))
			return false;

		if (address.Left(3) == _T("172"))
		{
			wxString middle = address.Mid(4);
			int pos = address.Find(_T("."));
			wxASSERT(pos != -1);
			long part;
			middle.Left(pos).ToLong(&part);

			if (part >= 16 && part <= 31)
				return false;
		}

		return true;
	}
}

bool IsIpAddress(const wxString& address)
{
	if (!GetIPV6LongForm(address).empty())
		return true;

	int segment = 0;
	int dotcount = 0;
	for (size_t i = 0; i < address.Len(); ++i) {
		wxChar const c = address[i];
		if (c == '.') {
			if (i + 1 < address.Len() && address[i + 1] == '.') {
				// Disallow multiple dots in a row
				return false;
			}

			if (segment > 255)
				return false;
			if (!dotcount && !segment)
				return false;
			dotcount++;
			segment = 0;
		}
		else if (c < '0' || c > '9') {
			return false;
		}
		else {
			segment = segment * 10 + c - '0';
		}
	}
	if (dotcount != 3)
		return false;

	if (segment > 255)
		return false;

	return true;
}


int GetRandomNumber(int min, int max)
{
	wxASSERT(min <= max);
	if (min >= max)
		return min;

	static std::mt19937_64 gen = std::mt19937_64(std::random_device()());
	std::uniform_int_distribution<int> dist(min, max);

	return dist(gen);
}

void MakeLowerAscii(wxString& str)
{
	for (size_t i = 0; i < str.Len(); i++) {
		wxChar c = str.GetChar(i);
		if (c >= 'A' && c <= 'Z') {
			c += 32;
			str.SetChar(i, wxUniChar(c));
		}
		else if (c == 0x130 || c == 0x131) {
			c = 'i';
			str.SetChar(i, wxUniChar(c));
		}
	}
}

wxString GetDependencyVersion(dependency::type d)
{
	switch (d) {
	case dependency::wxwidgets:
		return wxVERSION_NUM_DOT_STRING_T;
	case dependency::gnutls:
		{
			const char* v = gnutls_check_version(0);
			if (!v || !*v)
				return _T("unknown");

			return wxString(v, wxConvLibc);
		}
	case dependency::sqlite:
		return wxString::FromUTF8(sqlite3_libversion());
	default:
		return wxString();
	}
}

wxString GetDependencyName(dependency::type d)
{
	switch (d) {
	case dependency::wxwidgets:
		return _T("wxWidgets");
	case dependency::gnutls:
		return _T("GnuTLS");
	case dependency::sqlite:
		return _T("SQLite");
	default:
		return wxString();
	}
}

wxString ListTlsCiphers(const wxString& priority)
{
	return CTlsSocket::ListTlsCiphers(priority);
}

#ifdef __WXMSW__
bool IsAtLeast(int major, int minor = 0)
{
	OSVERSIONINFOEX vi = { 0 };
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	vi.dwMajorVersion = major;
	vi.dwMinorVersion = minor;
	vi.dwPlatformId = VER_PLATFORM_WIN32_NT;

	DWORDLONG mask = 0;
	VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(mask, VER_PLATFORMID, VER_EQUAL);
	return VerifyVersionInfo(&vi, VER_MAJORVERSION | VER_MINORVERSION | VER_PLATFORMID, mask) != 0;
}
#endif

bool GetRealOsVersion(int& major, int& minor)
{
#ifndef __WXMSW__
	return wxGetOsVersion(&major, &minor) != wxOS_UNKNOWN;
#else
	major = 4;
	minor = 0;
	while (IsAtLeast(++major, minor))
	{
	}
	--major;
	while (IsAtLeast(major, ++minor))
	{
	}
	--minor;

	return true;
#endif
}
