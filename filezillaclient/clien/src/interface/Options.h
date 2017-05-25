#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include "local_path.h"

#include <option_change_event_handler.h>

#include <mutex.h>

#include <wx/timer.h>

enum interfaceOptions
{
	OPTION_NUMTRANSFERS = OPTIONS_ENGINE_NUM,
	OPTION_ASCIIBINARY,
	OPTION_ASCIIFILES,
	OPTION_ASCIINOEXT,
	OPTION_ASCIIDOTFILE,
	OPTION_THEME,
	OPTION_LANGUAGE,
	OPTION_LASTSERVERPATH,
	OPTION_CONCURRENTDOWNLOADLIMIT,
	OPTION_CONCURRENTUPLOADLIMIT,
	OPTION_UPDATECHECK,
	OPTION_UPDATECHECK_INTERVAL,
	OPTION_UPDATECHECK_LASTDATE,
	OPTION_UPDATECHECK_NEWVERSION,
	OPTION_UPDATECHECK_CHECKBETA,
	OPTION_DEBUG_MENU,
	OPTION_FILEEXISTS_DOWNLOAD,
	OPTION_FILEEXISTS_UPLOAD,
	OPTION_ASCIIRESUME,
	OPTION_GREETINGVERSION,
	OPTION_ONETIME_DIALOGS,
	OPTION_SHOW_TREE_LOCAL,
	OPTION_SHOW_TREE_REMOTE,
	OPTION_FILEPANE_LAYOUT,
	OPTION_FILEPANE_SWAP,
	OPTION_LASTLOCALDIR,
	OPTION_FILELIST_DIRSORT,
	OPTION_FILELIST_NAMESORT,
	OPTION_QUEUE_SUCCESSFUL_AUTOCLEAR,
	OPTION_QUEUE_COLUMN_WIDTHS,
	OPTION_LOCALFILELIST_COLUMN_WIDTHS,
	OPTION_REMOTEFILELIST_COLUMN_WIDTHS,
	OPTION_MAINWINDOW_POSITION,
	OPTION_MAINWINDOW_SPLITTER_POSITION,
	OPTION_LOCALFILELIST_SORTORDER,
	OPTION_REMOTEFILELIST_SORTORDER,
	OPTION_TIME_FORMAT,
	OPTION_DATE_FORMAT,
	OPTION_SHOW_MESSAGELOG,
	OPTION_SHOW_QUEUE,
	OPTION_EDIT_DEFAULTEDITOR,
	OPTION_EDIT_ALWAYSDEFAULT,
	OPTION_EDIT_INHERITASSOCIATIONS,
	OPTION_EDIT_CUSTOMASSOCIATIONS,
	OPTION_COMPARISONMODE,
	OPTION_COMPARISON_THRESHOLD,
	OPTION_SITEMANAGER_POSITION,
	OPTION_THEME_ICONSIZE,
	OPTION_MESSAGELOG_TIMESTAMP,
	OPTION_SITEMANAGER_LASTSELECTED,
	OPTION_LOCALFILELIST_COLUMN_SHOWN,
	OPTION_REMOTEFILELIST_COLUMN_SHOWN,
	OPTION_LOCALFILELIST_COLUMN_ORDER,
	OPTION_REMOTEFILELIST_COLUMN_ORDER,
	OPTION_FILELIST_STATUSBAR,
	OPTION_FILTERTOGGLESTATE,
	OPTION_SHOW_QUICKCONNECT,
	OPTION_MESSAGELOG_POSITION,
	OPTION_LAST_CONNECTED_SITE,
	OPTION_DOUBLECLICK_ACTION_FILE,
	OPTION_DOUBLECLICK_ACTION_DIRECTORY,
	OPTION_MINIMIZE_TRAY,
	OPTION_SEARCH_COLUMN_WIDTHS,
	OPTION_SEARCH_COLUMN_SHOWN,
	OPTION_SEARCH_COLUMN_ORDER,
	OPTION_SEARCH_SIZE,
	OPTION_COMPARE_HIDEIDENTICAL,
	OPTION_SEARCH_SORTORDER,
	OPTION_EDIT_TRACK_LOCAL,
	OPTION_PREVENT_IDLESLEEP,
	OPTION_FILTEREDIT_SIZE,
	OPTION_INVALID_CHAR_REPLACE_ENABLE,
	OPTION_INVALID_CHAR_REPLACE,
	OPTION_ALREADYCONNECTED_CHOICE,
	OPTION_EDITSTATUSDIALOG_SIZE,
	OPTION_SPEED_DISPLAY,
	OPTION_TOOLBAR_HIDDEN,
	OPTION_STRIP_VMS_REVISION,
	OPTION_INTERFACE_SITEMANAGER_ON_STARTUP,
	OPTION_PROMPTPASSWORDSAVE,
	OPTION_PERSISTENT_CHOICES,

	// Default/internal options
	OPTION_DEFAULT_SETTINGSDIR, // guaranteed to be (back)slash-terminated
	OPTION_DEFAULT_KIOSKMODE,
	OPTION_DEFAULT_DISABLEUPDATECHECK,

	// Has to be last element
	OPTIONS_NUM
};

struct t_OptionsCache
{
	bool operator==(wxString const& v) const { return strValue == v; }
	bool operator==(int v) const { return numValue == v; }
	t_OptionsCache& operator=(wxString const& v);
	t_OptionsCache& operator=(int v);

	bool from_default;
	int numValue;
	wxString strValue;
};

class CXmlFile;
class TiXmlElement;
class COptions : public wxEvtHandler, public COptionsBase
{
public:
	virtual int GetOptionVal(unsigned int nID);
	virtual wxString GetOption(unsigned int nID);

	virtual bool SetOption(unsigned int nID, int value);
	virtual bool SetOption(unsigned int nID, wxString const& value);

	bool OptionFromFzDefaultsXml(unsigned int nID);

	void SetLastServer(const CServer& server);
	bool GetLastServer(CServer& server);

	static COptions* Get();
	static void Init();
	static void Destroy();

	void Import(TiXmlElement* pElement);

	void SaveIfNeeded();

	static CLocalPath GetUnadjustedSettingsDir();

protected:
	COptions();
	virtual ~COptions();

	int Validate(unsigned int nID, int value);
	wxString Validate(unsigned int nID, wxString const& value);

	template<typename T> void ContinueSetOption(unsigned int nID, T const& value);
	void SetXmlValue(unsigned int nID, int value);
	void SetXmlValue(unsigned int nID, wxString const& value);

	// path is element path below document root, separated by slashes
	void SetServer(wxString path, const CServer& server);
	bool GetServer(wxString path, CServer& server);

	TiXmlElement* CreateSettingsXmlElement();

	std::map<std::string, unsigned int> GetNameOptionMap() const;
	void LoadOptions(std::map<std::string, unsigned int> const& nameOptionMap, TiXmlElement* settings = 0);
	void LoadGlobalDefaultOptions(std::map<std::string, unsigned int> const& nameOptionMap);
	void LoadOptionFromElement(TiXmlElement* pOption, std::map<std::string, unsigned int> const& nameOptionMap, bool allowDefault);
	CLocalPath InitSettingsDir();
	void SetDefaultValues();

	void Save();

	void NotifyChangedOptions();

	CXmlFile* m_pXmlFile;

	t_OptionsCache m_optionsCache[OPTIONS_NUM];

	CServer* m_pLastServer;

	static COptions* m_theOptions;

	wxTimer m_save_timer;

	DECLARE_EVENT_TABLE()
	void OnTimer(wxTimerEvent& event);

	mutex m_sync_;

	changed_options_t changedOptions_;
};

#endif //__OPTIONS_H__
