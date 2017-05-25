#ifndef __LOCALE_INITIALIZER_H__

wxString GetFallbackLocale(wxString const& locale);

#ifdef __WXGTK__

#include <string>

class CInitializer
{
public:
	static bool SetLocale(const std::string& arg);

	static std::string GetLocaleOption();

	static bool error;

protected:
	static std::string GetAdjustedSettingsDir(); // Returns settings from fzdefaults.xml if it exists, otherwise calls GetSettingsDir
	static std::string GetUnadjustedSettingsDir(); // Returns standard settings dir as if there were no fzdefaults.xml
	static std::string ReadSettingsFromDefaults(std::string file);
	static std::string GetSettingFromFile(std::string file, const std::string& name);
	static std::string GetDefaultsXmlFile();
	static std::string CheckPathForDefaults(std::string path, int strip, std::string suffix);

	static bool SetLocaleReal(const std::string& locale);
	static std::string LocaleAddEncoding(const std::string& locale, const std::string& encoding);
};

#endif //__WXGTK__

#endif //__LOCALE_INITIALIZER_H__
