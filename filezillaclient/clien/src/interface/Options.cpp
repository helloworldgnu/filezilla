#include <filezilla.h>
#include "Options.h"
#include "xmlfunctions.h"
#include "filezillaapp.h"
#include <wx/tokenzr.h>
#include "ipcmutex.h"
#include <option_change_event_handler.h>
#include "sizeformatting.h"

#include <algorithm>
#include <string>

#ifdef __WXMSW__
	#include <shlobj.h>

	// Needed for MinGW:
	#ifndef SHGFP_TYPE_CURRENT
		#define SHGFP_TYPE_CURRENT 0
	#endif
#endif

COptions* COptions::m_theOptions = 0;

enum Type
{
	string,
	number
};

enum Flags
{
	normal,
	internal,
	default_only,
	default_priority // If that option is given in fzdefaults.xml, it overrides any user option
};

struct t_Option
{
	const char name[30];
	const enum Type type;
	const wxString defaultValue; // Default values are stored as string even for numerical options
	const Flags flags; // internal items won't get written to settings file nor loaded from there
};

#ifdef __WXMSW__
//case insensitive
#define DEFAULT_FILENAME_SORT   _T("0")
#else
//case sensitive
#define DEFAULT_FILENAME_SORT   _T("1")
#endif

// In C++14 we should be able to use this instead:
//   static_assert(OPTIONS_NUM <= changed_options_t().size());
static_assert(OPTIONS_NUM <= changed_options_size, "OPTIONS_NUM too big for changed_options_t");

static const t_Option options[OPTIONS_NUM] =
{
	// Note: A few options are versioned due to a changed
	// option syntax or past, unhealthy defaults

	// Engine settings
	{ "Use Pasv mode", number, _T("1"), normal },
	{ "Limit local ports", number, _T("0"), normal },
	{ "Limit ports low", number, _T("6000"), normal },
	{ "Limit ports high", number, _T("7000"), normal },
	{ "External IP mode", number, _T("0"), normal },
	{ "External IP", string, _T(""), normal },
	{ "External address resolver", string, _T("http://ip.filezilla-project.org/ip.php"), normal },
	{ "Last resolved IP", string, _T(""), normal },
	{ "No external ip on local conn", number, _T("1"), normal },
	{ "Pasv reply fallback mode", number, _T("0"), normal },
	{ "Timeout", number, _T("20"), normal },
	{ "Logging Debug Level", number, _T("0"), normal },
	{ "Logging Raw Listing", number, _T("0"), normal },
	{ "fzsftp executable", string, _T(""), internal },
	{ "Allow transfermode fallback", number, _T("1"), normal },
	{ "Reconnect count", number, _T("2"), normal },
	{ "Reconnect delay", number, _T("5"), normal },
	{ "Enable speed limits", number, _T("0"), normal },
	{ "Speedlimit inbound", number, _T("100"), normal },
	{ "Speedlimit outbound", number, _T("20"), normal },
	{ "Speedlimit burst tolerance", number, _T("0"), normal },
	{ "Preallocate space", number, _T("0"), normal },
	{ "View hidden files", number, _T("0"), normal },
	{ "Preserve timestamps", number, _T("0"), normal },
	{ "Socket recv buffer size (v2)", number, _T("4194304"), normal }, // Make it large enough by default
														 // to enable a large TCP window scale
	{ "Socket send buffer size (v2)", number, _T("262144"), normal },
	{ "FTP Keep-alive commands", number, _T("0"), normal },
	{ "FTP Proxy type", number, _T("0"), normal },
	{ "FTP Proxy host", string, _T(""), normal },
	{ "FTP Proxy user", string, _T(""), normal },
	{ "FTP Proxy password", string, _T(""), normal },
	{ "FTP Proxy login sequence", string, _T(""), normal },
	{ "SFTP keyfiles", string, _T(""), normal },
	{ "Proxy type", number, _T("0"), normal },
	{ "Proxy host", string, _T(""), normal },
	{ "Proxy port", number, _T("0"), normal },
	{ "Proxy user", string, _T(""), normal },
	{ "Proxy password", string, _T(""), normal },
	{ "Logging file", string, _T(""), normal },
	{ "Logging filesize limit", number, _T("10"), normal },
	{ "Logging show detailed logs", number, _T("0"), internal },
	{ "Size format", number, _T("0"), normal },
	{ "Size thousands separator", number, _T("1"), normal },
	{ "Size decimal places", number, _T("1"), normal },

	// Interface settings
	{ "Number of Transfers", number, _T("2"), normal },
	{ "Ascii Binary mode", number, _T("0"), normal },
	{ "Auto Ascii files", string, _T("am|asp|bat|c|cfm|cgi|conf|cpp|css|dhtml|diz|h|hpp|htm|html|in|inc|java|js|jsp|lua|m4|mak|md5|nfo|nsi|pas|patch|php|phtml|pl|po|py|qmail|sh|sha1|sha256|sha512|shtml|sql|svg|tcl|tpl|txt|vbs|xhtml|xml|xrc"), normal },
	{ "Auto Ascii no extension", number, _T("1"), normal },
	{ "Auto Ascii dotfiles", number, _T("1"), normal },
	{ "Theme", string, _T("opencrystal/"), normal },
	{ "Language Code", string, _T(""), normal },
	{ "Last Server Path", string, _T(""), normal },
	{ "Concurrent download limit", number, _T("0"), normal },
	{ "Concurrent upload limit", number, _T("0"), normal },
	{ "Update Check", number, _T("1"), normal },
	{ "Update Check Interval", number, _T("7"), normal },
	{ "Last automatic update check", string, _T(""), normal },
	{ "Update Check New Version", string, _T(""), normal },
	{ "Update Check Check Beta", number, _T("0"), normal },
	{ "Show debug menu", number, _T("0"), normal },
	{ "File exists action download", number, _T("0"), normal },
	{ "File exists action upload", number, _T("0"), normal },
	{ "Allow ascii resume", number, _T("0"), normal },
	{ "Greeting version", string, _T(""), normal },
	{ "Onetime Dialogs", string, _T(""), normal },
	{ "Show Tree Local", number, _T("1"), normal },
	{ "Show Tree Remote", number, _T("1"), normal },
	{ "File Pane Layout", number, _T("0"), normal },
	{ "File Pane Swap", number, _T("0"), normal },
	{ "Last local directory", string, _T(""), normal },
	{ "Filelist directory sort", number, _T("0"), normal },
	{ "Filelist name sort", number, DEFAULT_FILENAME_SORT, normal },
	{ "Queue successful autoclear", number, _T("0"), normal },
	{ "Queue column widths", string, _T(""), normal },
	{ "Local filelist colwidths", string, _T(""), normal },
	{ "Remote filelist colwidths", string, _T(""), normal },
	{ "Window position and size", string, _T(""), normal },
	{ "Splitter positions (v2)", string, _T(""), normal },
	{ "Local filelist sortorder", string, _T(""), normal },
	{ "Remote filelist sortorder", string, _T(""), normal },
	{ "Time Format", string, _T(""), normal },
	{ "Date Format", string, _T(""), normal },
	{ "Show message log", number, _T("1"), normal },
	{ "Show queue", number, _T("1"), normal },
	{ "Default editor", string, _T(""), normal },
	{ "Always use default editor", number, _T("0"), normal },
	{ "Inherit system associations", number, _T("1"), normal },
	{ "Custom file associations", string, _T(""), normal },
	{ "Comparison mode", number, _T("1"), normal },
	{ "Comparison threshold", number, _T("1"), normal },
	{ "Site Manager position", string, _T(""), normal },
	{ "Theme icon size", string, _T(""), normal },
	{ "Timestamp in message log", number, _T("0"), normal },
	{ "Sitemanager last selected", string, _T(""), normal },
	{ "Local filelist shown columns", string, _T(""), normal },
	{ "Remote filelist shown columns", string, _T(""), normal },
	{ "Local filelist column order", string, _T(""), normal },
	{ "Remote filelist column order", string, _T(""), normal },
	{ "Filelist status bar", number, _T("1"), normal },
	{ "Filter toggle state", number, _T("0"), normal },
	{ "Show quickconnect bar", number, _T("1"), normal },
	{ "Messagelog position", number, _T("0"), normal },
	{ "Last connected site", string, _T(""), normal },
	{ "File doubleclock action", number, _T("0"), normal },
	{ "Dir doubleclock action", number, _T("0"), normal },
	{ "Minimize to tray", number, _T("0"), normal },
	{ "Search column widths", string, _T(""), normal },
	{ "Search column shown", string, _T(""), normal },
	{ "Search column order", string, _T(""), normal },
	{ "Search window size", string, _T(""), normal },
	{ "Comparison hide identical", number, _T("0"), normal },
	{ "Search sort order", string, _T(""), normal },
	{ "Edit track local", number, _T("1"), normal },
	{ "Prevent idle sleep", number, _T("1"), normal },
	{ "Filteredit window size", string, _T(""), normal },
	{ "Enable invalid char filter", number, _T("1"), normal },
	{ "Invalid char replace", string, _T("_"), normal },
	{ "Already connected choice", number, _T("0"), normal },
	{ "Edit status dialog size", string, _T(""), normal },
	{ "Display current speed", number, _T("0"), normal },
	{ "Toolbar hidden", number, _T("0"), normal },
	{ "Strip VMS revisions", number, _T("0"), normal },
	{ "Show Site Manager on startup", number, _T("0"), normal },
	{ "Prompt password change", number, _T("0"), normal },
	{ "Persistent Choices", number, _T("0"), normal },

	// Default/internal options
	{ "Config Location", string, _T(""), default_only },
	{ "Kiosk mode", number, _T("0"), default_priority },
	{ "Disable update check", number, _T("0"), default_only }
};

BEGIN_EVENT_TABLE(COptions, wxEvtHandler)
EVT_TIMER(wxID_ANY, COptions::OnTimer)
END_EVENT_TABLE()

t_OptionsCache& t_OptionsCache::operator=(wxString const& v)
{
	strValue = v;
	long l;
	if (v.ToLong(&l)) {
		numValue = l;
	}
	else {
		numValue = 0;
	}
	return *this;
}

t_OptionsCache& t_OptionsCache::operator=(int v)
{
	numValue = v;
	strValue.Printf("%d", v);
	return *this;
}

COptions::COptions()
{
	m_theOptions = this;
	m_pXmlFile = 0;
	m_pLastServer = 0;

	SetDefaultValues();

	m_save_timer.SetOwner(this);

	auto const nameOptionMap = GetNameOptionMap();
	LoadGlobalDefaultOptions(nameOptionMap);

	CLocalPath const dir = InitSettingsDir();

	CInterProcessMutex mutex(MUTEX_OPTIONS);
	m_pXmlFile = new CXmlFile(dir.GetPath() + _T("filezilla.xml"));
	if (!m_pXmlFile->Load()) {
		wxString msg = m_pXmlFile->GetError() + _T("\n\n") + _("For this session the default settings will be used. Any changes to the settings will not be saved.");
		wxMessageBoxEx(msg, _("Error loading xml file"), wxICON_ERROR);
		delete m_pXmlFile;
		m_pXmlFile = 0;
	}
	else
		CreateSettingsXmlElement();

	LoadOptions(nameOptionMap);
}

std::map<std::string, unsigned int> COptions::GetNameOptionMap() const
{
	std::map<std::string, unsigned int> ret;
	for (unsigned int i = 0; i < OPTIONS_NUM; ++i) {
		if (options[i].flags != internal)
			ret.insert(std::make_pair(std::string(options[i].name), i));
	}
	return ret;
}

COptions::~COptions()
{
	COptionChangeEventHandler::UnregisterAll();

	delete m_pLastServer;
	delete m_pXmlFile;
}

int COptions::GetOptionVal(unsigned int nID)
{
	if (nID >= OPTIONS_NUM)
		return 0;

	scoped_lock l(m_sync_);
	return m_optionsCache[nID].numValue;
}

wxString COptions::GetOption(unsigned int nID)
{
	if (nID >= OPTIONS_NUM)
		return wxString();

	scoped_lock l(m_sync_);
	return m_optionsCache[nID].strValue;
}

bool COptions::SetOption(unsigned int nID, int value)
{
	if (nID >= OPTIONS_NUM)
		return false;

	if (options[nID].type != number)
		return false;

	ContinueSetOption(nID, value);
	return true;
}

bool COptions::SetOption(unsigned int nID, wxString const& value)
{
	if (nID >= OPTIONS_NUM)
		return false;

	if (options[nID].type != string) {
		long tmp;
		if (!value.ToLong(&tmp))
			return false;

		return SetOption(nID, tmp);
	}

	ContinueSetOption(nID, value);
	return true;
}

template<typename T>
void COptions::ContinueSetOption(unsigned int nID, T const& value)
{
	T validated = Validate(nID, value);

	{
		scoped_lock l(m_sync_);
		if (m_optionsCache[nID] == validated) {
			// Nothing to do
			return;
		}
		m_optionsCache[nID] = validated;
	}

	// Fixme: Setting options from other threads
	if (!wxIsMainThread())
		return;

	if (options[nID].flags == normal || options[nID].flags == default_priority) {
		SetXmlValue(nID, validated);

		if (!m_save_timer.IsRunning())
			m_save_timer.Start(15000, true);
	}

	if (changedOptions_.none()) {
		CallAfter(&COptions::NotifyChangedOptions);
	}
	changedOptions_.set(nID);
}

void COptions::NotifyChangedOptions()
{
	// Reset prior to notifying to correctly handle the case of an option being set while notifying
	auto changedOptions = changedOptions_;
	changedOptions_.reset();
	COptionChangeEventHandler::DoNotify(changedOptions);
}

bool COptions::OptionFromFzDefaultsXml(unsigned int nID)
{
	if (nID >= OPTIONS_NUM)
		return false;

	scoped_lock l(m_sync_);
	return m_optionsCache[nID].from_default;
}

TiXmlElement* COptions::CreateSettingsXmlElement()
{
	if (!m_pXmlFile)
		return 0;

	TiXmlElement* element = m_pXmlFile->GetElement();
	if (!element) {
		return 0;
	}

	TiXmlElement* settings = element->FirstChildElement("Settings");
	if (settings) {
		return settings;
	}

	settings = new TiXmlElement("Settings");
	element->LinkEndChild(settings);

	for (int i = 0; i < OPTIONS_NUM; ++i) {
		if (options[i].type == string) {
			SetXmlValue(i, GetOption(i));
		}
		else {
			SetXmlValue(i, GetOptionVal(i));
		}
	}

	return settings;
}

void COptions::SetXmlValue(unsigned int nID, int value)
{
	SetXmlValue(nID, wxString::Format(_T("%d"), value));
}

void COptions::SetXmlValue(unsigned int nID, wxString const& value)
{
	if (!m_pXmlFile)
		return;

	// No checks are made about the validity of the value, that's done in SetOption

	wxScopedCharBuffer utf8 = value.utf8_str();
	if (!utf8)
		return;

	TiXmlElement *settings = CreateSettingsXmlElement();
	if (settings) {
		TiXmlElement *setting = 0;
		for (setting = settings->FirstChildElement("Setting"); setting; setting = setting->NextSiblingElement("Setting")) {
			const char *attribute = setting->Attribute("name");
			if (!attribute)
				continue;
			if (!strcmp(attribute, options[nID].name))
				break;
		}
		if (setting) {
			setting->Clear();
		}
		else {
			setting = new TiXmlElement("Setting");
			setting->SetAttribute("name", options[nID].name);
			settings->LinkEndChild(setting);
		}
		setting->LinkEndChild(new TiXmlText(utf8));
	}
}

int COptions::Validate(unsigned int nID, int value)
{
	switch (nID)
	{
	case OPTION_UPDATECHECK_INTERVAL:
		if (value < 1 || value > 7)
			value = 7;
		break;
	case OPTION_LOGGING_DEBUGLEVEL:
		if (value < 0 || value > 4)
			value = 0;
		break;
	case OPTION_RECONNECTCOUNT:
		if (value < 0 || value > 99)
			value = 5;
		break;
	case OPTION_RECONNECTDELAY:
		if (value < 0 || value > 999)
			value = 5;
		break;
	case OPTION_FILEPANE_LAYOUT:
		if (value < 0 || value > 3)
			value = 0;
		break;
	case OPTION_SPEEDLIMIT_INBOUND:
	case OPTION_SPEEDLIMIT_OUTBOUND:
		if (value < 0)
			value = 0;
		break;
	case OPTION_SPEEDLIMIT_BURSTTOLERANCE:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_FILELIST_DIRSORT:
	case OPTION_FILELIST_NAMESORT:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_SOCKET_BUFFERSIZE_RECV:
		if (value != -1 && (value < 4096 || value > 4096 * 1024))
			value = -1;
		break;
	case OPTION_SOCKET_BUFFERSIZE_SEND:
		if (value != -1 && (value < 4096 || value > 4096 * 1024))
			value = 131072;
		break;
	case OPTION_COMPARISONMODE:
		if (value < 0 || value > 0)
			value = 1;
		break;
	case OPTION_COMPARISON_THRESHOLD:
		if (value < 0 || value > 1440)
			value = 1;
		break;
	case OPTION_SIZE_DECIMALPLACES:
		if (value < 0 || value > 3)
			value = 0;
		break;
	case OPTION_MESSAGELOG_POSITION:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_DOUBLECLICK_ACTION_FILE:
	case OPTION_DOUBLECLICK_ACTION_DIRECTORY:
		if (value < 0 || value > 3)
			value = 0;
		break;
	case OPTION_SIZE_FORMAT:
		if (value < 0 || value >= CSizeFormat::formats_count)
			value = 0;
		break;
	}
	return value;
}

wxString COptions::Validate(unsigned int nID, wxString const& value)
{
	if (nID == OPTION_INVALID_CHAR_REPLACE) {
		if (value.Len() > 1)
			return _T("_");
	}
	return value;
}

void COptions::SetServer(wxString path, const CServer& server)
{
	if (!m_pXmlFile)
		return;

	if (path.empty())
		return;

	TiXmlElement *element = m_pXmlFile->GetElement();

	while (!path.empty()) {
		wxString sub;
		int pos = path.Find('/');
		if (pos != -1) {
			sub = path.Left(pos);
			path = path.Mid(pos + 1);
		}
		else {
			sub = path;
			path = _T("");
		}
		wxScopedCharBuffer utf8 = sub.utf8_str();
		if (!utf8)
			return;
		TiXmlElement *newElement = element->FirstChildElement(utf8);
		if (newElement)
			element = newElement;
		else {
			TiXmlNode *node = element->LinkEndChild(new TiXmlElement(utf8));
			if (!node || !node->ToElement())
				return;
			element = node->ToElement();
		}
	}

	::SetServer(element, server);

	if (GetOptionVal(OPTION_DEFAULT_KIOSKMODE) == 2)
		return;

	CInterProcessMutex mutex(MUTEX_OPTIONS);
	m_pXmlFile->Save(true);
}

bool COptions::GetServer(wxString path, CServer& server)
{
	if (path.empty())
		return false;

	if (!m_pXmlFile)
		return false;
	TiXmlElement *element = m_pXmlFile->GetElement();

	while (!path.empty()) {
		wxString sub;
		int pos = path.Find('/');
		if (pos != -1) {
			sub = path.Left(pos);
			path = path.Mid(pos + 1);
		}
		else {
			sub = path;
			path = _T("");
		}
		wxScopedCharBuffer utf8 = sub.utf8_str();
		if (!utf8)
			return false;
		element = element->FirstChildElement(utf8);
		if (!element)
			return false;
	}

	bool res = ::GetServer(element, server);

	return res;
}

void COptions::SetLastServer(const CServer& server)
{
	if (!m_pLastServer)
		m_pLastServer = new CServer(server);
	else
		*m_pLastServer = server;
	SetServer(_T("Settings/LastServer"), server);
}

bool COptions::GetLastServer(CServer& server)
{
	if (!m_pLastServer) {
		bool res = GetServer(_T("Settings/LastServer"), server);
		if (res)
			m_pLastServer = new CServer(server);
		return res;
	}
	else {
		server = *m_pLastServer;
		if (server == CServer())
			return false;

		return true;
	}
}

void COptions::Init()
{
	if (!m_theOptions)
		new COptions(); // It sets m_theOptions internally itself
}

void COptions::Destroy()
{
	if (!m_theOptions)
		return;

	delete m_theOptions;
	m_theOptions = 0;
}

COptions* COptions::Get()
{
	return m_theOptions;
}

void COptions::Import(TiXmlElement* pElement)
{
	LoadOptions(GetNameOptionMap(), pElement);
	if (!m_save_timer.IsRunning())
		m_save_timer.Start(15000, true);
}

void COptions::LoadOptions(std::map<std::string, unsigned int> const& nameOptionMap, TiXmlElement* settings)
{
	if (!settings) {
		settings = CreateSettingsXmlElement();
		if (!settings) {
			return;
		}
	}

	TiXmlNode *node = 0;
	while ((node = settings->IterateChildren("Setting", node))) {
		TiXmlElement *setting = node->ToElement();
		if (!setting)
			continue;
		LoadOptionFromElement(setting, nameOptionMap, false);
	}
}

void COptions::LoadOptionFromElement(TiXmlElement* pOption, std::map<std::string, unsigned int> const& nameOptionMap, bool allowDefault)
{
	const char* name = pOption->Attribute("name");
	if (!name)
		return;

	auto const iter = nameOptionMap.find(name);
	if (iter != nameOptionMap.end()) {
		if (!allowDefault && options[iter->second].flags == default_only)
			return;
		wxString value;

		TiXmlNode *text = pOption->FirstChild();
		if (text) {
			if (!text->ToText())
				return;

			value = ConvLocal(text->Value());
		}

		if (options[iter->second].flags == default_priority) {
			if (allowDefault) {
				scoped_lock l(m_sync_);
				m_optionsCache[iter->second].from_default = true;
			}
			else {
				scoped_lock l(m_sync_);
				if (m_optionsCache[iter->second].from_default)
					return;
			}
		}

		if (options[iter->second].type == number) {
			long numValue = 0;
			value.ToLong(&numValue);
			numValue = Validate(iter->second, numValue);
			scoped_lock l(m_sync_);
			m_optionsCache[iter->second] = numValue;
		}
		else {
			value = Validate(iter->second, value);
			scoped_lock l(m_sync_);
			m_optionsCache[iter->second] = value;
		}
	}
}

void COptions::LoadGlobalDefaultOptions(std::map<std::string, unsigned int> const& nameOptionMap)
{
	CLocalPath const defaultsDir = wxGetApp().GetDefaultsDir();
	if (defaultsDir.empty())
		return;

	CXmlFile file(defaultsDir.GetPath() + _T("fzdefaults.xml"));
	if (!file.Load())
		return;

	TiXmlElement* pElement = file.GetElement();
	if (!pElement)
		return;

	pElement = pElement->FirstChildElement("Settings");
	if (!pElement)
		return;

	for (TiXmlElement* pSetting = pElement->FirstChildElement("Setting"); pSetting; pSetting = pSetting->NextSiblingElement("Setting")) {
		LoadOptionFromElement(pSetting, nameOptionMap, true);
	}
}

void COptions::OnTimer(wxTimerEvent&)
{
	Save();
}

void COptions::Save()
{
	if (GetOptionVal(OPTION_DEFAULT_KIOSKMODE) == 2)
		return;

	if (!m_pXmlFile)
		return;

	CInterProcessMutex mutex(MUTEX_OPTIONS);
	m_pXmlFile->Save(true);
}

void COptions::SaveIfNeeded()
{
	if (!m_save_timer.IsRunning())
		return;

	m_save_timer.Stop();
	Save();
}

namespace {
#ifndef __WXMSW__
wxString TryDirectory( wxString path, wxString const& suffix, bool check_exists )
{
	if( !path.empty() && path[0] == '/' ) {
		if( path[path.size() - 1] != '/' ) {
			path += '/';
		}

		path += suffix;

		if( check_exists ) {
			if( !wxFileName::DirExists(path) ) {
				path.clear();
			}
		}
	}
	else {
		path.clear();
	}
	return path;
}
#endif

wxString GetEnv(wxString const& env)
{
	wxString ret;
	if( !wxGetEnv(env, &ret) ) {
		ret.clear();
	}
	return ret;
}
}

CLocalPath COptions::GetUnadjustedSettingsDir()
{
	wxFileName fn;
#ifdef __WXMSW__
	wxChar buffer[MAX_PATH * 2 + 1];

	if (SUCCEEDED(SHGetFolderPath(0, CSIDL_APPDATA, 0, SHGFP_TYPE_CURRENT, buffer))) {
		fn = wxFileName(buffer, _T(""));
		fn.AppendDir(_T("FileZilla"));
	}
	else {
		// Fall back to directory where the executable is
		if (GetModuleFileName(0, buffer, MAX_PATH * 2))
			fn = buffer;
	}
#else
	wxString cfg = TryDirectory(GetEnv(_T("XDG_CONFIG_HOME")), _T("filezilla/"), true);
	if( cfg.empty() ) {
		cfg = TryDirectory(wxGetHomeDir(), _T(".config/filezilla/"), true);
	}
	if( cfg.empty() ) {
		cfg = TryDirectory(wxGetHomeDir(), _T(".filezilla/"), true);
	}
	if( cfg.empty() ) {
		cfg = TryDirectory(GetEnv(_T("XDG_CONFIG_HOME")), _T("filezilla/"), false);
	}
	if( cfg.empty() ) {
		cfg = TryDirectory(wxGetHomeDir(), _T(".config/filezilla/"), false);
	}
	if( cfg.empty() ) {
		cfg = TryDirectory(wxGetHomeDir(), _T(".filezilla/"), false);
	}
	fn = wxFileName(cfg, _T(""));
#endif
	return CLocalPath(fn.GetPath());
}

CLocalPath COptions::InitSettingsDir()
{
	CLocalPath p;

	wxString dir(GetOption(OPTION_DEFAULT_SETTINGSDIR));
	if (!dir.empty()) {
		wxStringTokenizer tokenizer(dir, _T("/\\"), wxTOKEN_RET_EMPTY_ALL);
		dir = _T("");
		while (tokenizer.HasMoreTokens()) {
			wxString token = tokenizer.GetNextToken();
			if (!token.empty() && token[0] == '$') {
				if (token.size() > 1 && token[1] == '$')
					token = token.Mid(1);
				else {
					token = GetEnv(token.Mid(1));
				}
			}
			dir += token;
			const wxChar delimiter = tokenizer.GetLastDelimiter();
			if (delimiter)
				dir += delimiter;
		}

		p.SetPath(wxGetApp().GetDefaultsDir().GetPath());
		p.ChangePath(dir);
	}
	else {
		p = GetUnadjustedSettingsDir();
	}

	if (!p.empty() && !p.Exists())
		wxFileName::Mkdir( p.GetPath(), 0700, wxPATH_MKDIR_FULL );

	SetOption(OPTION_DEFAULT_SETTINGSDIR, p.GetPath());

	return p;
}

void COptions::SetDefaultValues()
{
	scoped_lock l(m_sync_);
	for (int i = 0; i < OPTIONS_NUM; ++i) {
		m_optionsCache[i] = options[i].defaultValue;
		m_optionsCache[i].from_default = false;
	}
}
