#ifndef __MISC_H__
#define __MISC_H__

#include "socket.h"

// Also verifies that it is a correct IPv6 address
wxString GetIPV6LongForm(wxString short_address);

int DigitHexToDecNum(wxChar c);

bool IsRoutableAddress(const wxString& address, CSocket::address_family family);

bool IsIpAddress(const wxString& address);

// Get a random number uniformly distributed in the closed interval [min, max]
int GetRandomNumber(int low, int high);

// Under some locales (e.g. Turkish), there is a different
// relationship between the letters a-z and A-Z.
// In Turkish for example there are different types of i
// (dotted and dotless), with i lowercase dotted and I
// uppercase dotless.
// If needed, use this function to transform the case manually
// and locale-independently
// In addition to the usual A-Z to a-z, the other two i's are
// transformed to lowercase dotted i as well.
void MakeLowerAscii(wxString& str);

// Strongly typed enum would be nice, but we need to support older compilers still.
namespace dependency {
enum type {
	wxwidgets,
	gnutls,
	sqlite,
	count
};
}

wxString GetDependencyName( dependency::type d );
wxString GetDependencyVersion( dependency::type d );

wxString ListTlsCiphers(const wxString& priority);

// Microsoft, in its insane stupidity, has decided to make GetVersion(Ex) useless, starting with Windows 8.1,
// this function no longer returns the operating system version but instead some arbitrary and random value depending
// on the phase of the moon.
// This function instead returns the actual Windows version. On non-Windows systems, it's equivalent to
// wxGetOsVersion
bool GetRealOsVersion( int& major, int& minor );

// C++11 sadly lacks make_unique, provide our own.
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename Derived, typename Base>
std::unique_ptr<Derived>
unique_static_cast(std::unique_ptr<Base>&& p)
{
	auto d = static_cast<Derived *>(p.release());
	return std::unique_ptr<Derived>(d);
}

#endif //__MISC_H__
