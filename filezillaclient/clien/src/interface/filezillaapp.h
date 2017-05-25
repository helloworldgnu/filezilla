#ifndef __FILEZILLAAPP_H__
#define __FILEZILLAAPP_H__

#if wxUSE_DEBUGREPORT && wxUSE_ON_FATAL_EXCEPTION
#include <wx/debugrpt.h>
#endif

#include <list>

#include "local_path.h"

class CWrapEngine;
class CCommandLine;
class CFileZillaApp : public wxApp
{
public:
	CFileZillaApp();
	virtual ~CFileZillaApp();

	virtual bool OnInit();
	virtual int OnExit();

	// Always (back)slash-terminated
	CLocalPath GetResourceDir() const { return m_resourceDir; }
	CLocalPath GetDefaultsDir() const { return m_defaultsDir; }
	CLocalPath GetLocalesDir() const { return m_localesDir; }

	wxString GetSettingsFile(wxString const& name) const;

	void CheckExistsFzsftp();

	void InitLocale();
	bool SetLocale(int language);
	int GetCurrentLanguage() const;
	wxString GetCurrentLanguageCode() const;

	void DisplayEncodingWarning();

	CWrapEngine* GetWrapEngine();

	const CCommandLine* GetCommandLine() const { return m_pCommandLine.get(); }

	void ShowStartupProfile();
	void AddStartupProfileRecord(const wxString& msg);

protected:
	bool InitDefaultsDir();
	bool LoadResourceFiles();
	bool LoadLocales();
	int ProcessCommandLine();

	std::unique_ptr<wxLocale> m_pLocale;

	CLocalPath m_resourceDir;
	CLocalPath m_defaultsDir;
	CLocalPath m_localesDir;

#if wxUSE_DEBUGREPORT && wxUSE_ON_FATAL_EXCEPTION
	virtual void OnFatalException();
#endif

	CLocalPath GetDataDir(wxString fileToFind) const;

	// FileExists accepts full paths as parameter,
	// with the addition that path segments may be obmitted
	// with a wildcard (*). A matching directory will then be searched.
	// Example: FileExists(_T("/home/*/.filezilla/filezilla.xml"));
	bool FileExists(const wxString& file) const;

	std::unique_ptr<CWrapEngine> m_pWrapEngine;

	std::unique_ptr<CCommandLine> m_pCommandLine;

	bool m_profilingActive;
	std::list<std::pair<wxDateTime, wxString> > m_startupProfile;
};

DECLARE_APP(CFileZillaApp)

#endif //__FILEZILLAAPP_H__
