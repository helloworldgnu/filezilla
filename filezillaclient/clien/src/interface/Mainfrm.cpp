#include <filezilla.h>
#include "Mainfrm.h"

#include "LocalListView.h"
#include "LocalTreeView.h"
#include "queue.h"
#include "RemoteListView.h"
#include "RemoteTreeView.h"
#include "StatusView.h"
#include "state.h"
#include "Options.h"
#include "asyncrequestqueue.h"
#include "commandqueue.h"
#include "led.h"
#include "sitemanager_dialog.h"
#include "settings/settingsdialog.h"
#include "themeprovider.h"
#include "filezillaapp.h"
#include "view.h"
#include "viewheader.h"
#include "aboutdialog.h"
#include "filter.h"
#include "netconfwizard.h"
#include "quickconnectbar.h"
#include "updater.h"
#include "update_dialog.h"
#include "defaultfileexistsdlg.h"
#include "loginmanager.h"
#include "conditionaldialog.h"
#include "clearprivatedata.h"
#include "export.h"
#include "import.h"
#include "recursive_operation.h"
#include <wx/tokenzr.h>
#include "edithandler.h"
#include "inputdialog.h"
#include "window_state_manager.h"
#include "cmdline.h"
#include "buildinfo.h"
#include "filelist_statusbar.h"
#include "manual_transfer.h"
#include "auto_ascii_files.h"
#include "splitter.h"
#include "bookmarks_dialog.h"
#include "search.h"
#include "power_management.h"
#include "welcome_dialog.h"
#include "context_control.h"
#include "speedlimits_dialog.h"
#include "toolbar.h"
#include "menu_bar.h"

#ifdef __WXMSW__
#include <wx/module.h>
#endif
#ifndef __WXMAC__
#include <wx/taskbar.h>
#else
#include <wx/combobox.h>
#endif

#include <functional>
#include <map>

#ifdef __WXGTK__
DECLARE_EVENT_TYPE(fzEVT_TASKBAR_CLICK_DELAYED, -1)
DEFINE_EVENT_TYPE(fzEVT_TASKBAR_CLICK_DELAYED)
#endif

static int tab_hotkey_ids[10];

#if FZ_MANUALUPDATECHECK
static int GetAvailableUpdateMenuId()
{
	static int updateAvailableMenuId = wxNewId();
	return updateAvailableMenuId;
}
#endif

std::map<int, std::pair<std::function<void(wxTextEntry*)>, wxChar>> keyboardCommands;

wxTextEntry* GetSpecialTextEntry(wxWindow* w, wxChar cmd)
{
#ifdef __WXMAC__
	if( cmd == 'A' || cmd == 'V' ) {
		wxTextCtrl* text = dynamic_cast<wxTextCtrl*>(w);
		if( text && text->GetWindowStyle() & wxTE_PASSWORD ) {
			return text;
		}
	}
	wxComboBox* combo = dynamic_cast<wxComboBox*>(w);
	if( combo ) {
		return combo;
	}
#endif
	return 0;
}

bool HandleKeyboardCommand(wxCommandEvent& event, wxWindow& parent)
{
	auto const& it = keyboardCommands.find(event.GetId());
	if( it == keyboardCommands.end() ) {
		return false;
	}

	wxTextEntry* e = GetSpecialTextEntry(parent.FindFocus(), it->second.second);
	if( e ) {
		it->second.first(e);
	}
	else {
		event.Skip();
	}
	return true;
}

BEGIN_EVENT_TABLE(CMainFrame, wxNavigationEnabled<wxFrame>)
	EVT_SIZE(CMainFrame::OnSize)
	EVT_MENU(wxID_ANY, CMainFrame::OnMenuHandler)
	EVT_FZ_NOTIFICATION(wxID_ANY, CMainFrame::OnEngineEvent)
	EVT_COMMAND(wxID_ANY, fzEVT_UPDATE_LED_TOOLTIP, CMainFrame::OnUpdateLedTooltip)
	EVT_TOOL(XRCID("ID_TOOLBAR_DISCONNECT"), CMainFrame::OnDisconnect)
	EVT_MENU(XRCID("ID_MENU_SERVER_DISCONNECT"), CMainFrame::OnDisconnect)
	EVT_TOOL(XRCID("ID_TOOLBAR_CANCEL"), CMainFrame::OnCancel)
	EVT_MENU(XRCID("ID_CANCEL"), CMainFrame::OnCancel)
	EVT_TOOL(XRCID("ID_TOOLBAR_RECONNECT"), CMainFrame::OnReconnect)
	EVT_TOOL(XRCID("ID_MENU_SERVER_RECONNECT"), CMainFrame::OnReconnect)
	EVT_TOOL(XRCID("ID_TOOLBAR_REFRESH"), CMainFrame::OnRefresh)
	EVT_MENU(XRCID("ID_REFRESH"), CMainFrame::OnRefresh)
	EVT_TOOL(XRCID("ID_TOOLBAR_SITEMANAGER"), CMainFrame::OnSiteManager)
	EVT_CLOSE(CMainFrame::OnClose)
#ifdef WITH_LIBDBUS
	EVT_END_SESSION(CMainFrame::OnClose)
#endif
	EVT_TIMER(wxID_ANY, CMainFrame::OnTimer)
	EVT_TOOL(XRCID("ID_TOOLBAR_PROCESSQUEUE"), CMainFrame::OnProcessQueue)
	EVT_TOOL(XRCID("ID_TOOLBAR_LOGVIEW"), CMainFrame::OnToggleLogView)
	EVT_TOOL(XRCID("ID_TOOLBAR_LOCALTREEVIEW"), CMainFrame::OnToggleDirectoryTreeView)
	EVT_TOOL(XRCID("ID_TOOLBAR_REMOTETREEVIEW"), CMainFrame::OnToggleDirectoryTreeView)
	EVT_TOOL(XRCID("ID_TOOLBAR_QUEUEVIEW"), CMainFrame::OnToggleQueueView)
	EVT_MENU(XRCID("ID_VIEW_TOOLBAR"), CMainFrame::OnToggleToolBar)
	EVT_MENU(XRCID("ID_VIEW_MESSAGELOG"), CMainFrame::OnToggleLogView)
	EVT_MENU(XRCID("ID_VIEW_LOCALTREE"), CMainFrame::OnToggleDirectoryTreeView)
	EVT_MENU(XRCID("ID_VIEW_REMOTETREE"), CMainFrame::OnToggleDirectoryTreeView)
	EVT_MENU(XRCID("ID_VIEW_QUEUE"), CMainFrame::OnToggleQueueView)
	EVT_MENU(wxID_ABOUT, CMainFrame::OnMenuHelpAbout)
	EVT_TOOL(XRCID("ID_TOOLBAR_FILTER"), CMainFrame::OnFilter)
	EVT_TOOL_RCLICKED(XRCID("ID_TOOLBAR_FILTER"), CMainFrame::OnFilterRightclicked)
#if FZ_MANUALUPDATECHECK
	EVT_MENU(XRCID("ID_CHECKFORUPDATES"), CMainFrame::OnCheckForUpdates)
	EVT_MENU(GetAvailableUpdateMenuId(), CMainFrame::OnCheckForUpdates)
#endif //FZ_MANUALUPDATECHECK
	EVT_TOOL_RCLICKED(XRCID("ID_TOOLBAR_SITEMANAGER"), CMainFrame::OnSitemanagerDropdown)
#ifdef EVT_TOOL_DROPDOWN
	EVT_TOOL_DROPDOWN(XRCID("ID_TOOLBAR_SITEMANAGER"), CMainFrame::OnSitemanagerDropdown)
#endif
	EVT_NAVIGATION_KEY(CMainFrame::OnNavigationKeyEvent)
	EVT_CHAR_HOOK(CMainFrame::OnChar)
	EVT_MENU(XRCID("ID_MENU_VIEW_FILTERS"), CMainFrame::OnFilter)
	EVT_ACTIVATE(CMainFrame::OnActivate)
	EVT_TOOL(XRCID("ID_TOOLBAR_COMPARISON"), CMainFrame::OnToolbarComparison)
	EVT_TOOL_RCLICKED(XRCID("ID_TOOLBAR_COMPARISON"), CMainFrame::OnToolbarComparisonDropdown)
#ifdef EVT_TOOL_DROPDOWN
	EVT_TOOL_DROPDOWN(XRCID("ID_TOOLBAR_COMPARISON"), CMainFrame::OnToolbarComparisonDropdown)
#endif
	EVT_MENU(XRCID("ID_COMPARE_SIZE"), CMainFrame::OnDropdownComparisonMode)
	EVT_MENU(XRCID("ID_COMPARE_DATE"), CMainFrame::OnDropdownComparisonMode)
	EVT_MENU(XRCID("ID_COMPARE_HIDEIDENTICAL"), CMainFrame::OnDropdownComparisonHide)
	EVT_TOOL(XRCID("ID_TOOLBAR_SYNCHRONIZED_BROWSING"), CMainFrame::OnSyncBrowse)
#ifdef __WXMAC__
	EVT_CHILD_FOCUS(CMainFrame::OnChildFocused)
#else
	EVT_ICONIZE(CMainFrame::OnIconize)
#endif
#ifdef __WXGTK__
	EVT_COMMAND(wxID_ANY, fzEVT_TASKBAR_CLICK_DELAYED, CMainFrame::OnTaskBarClick_Delayed)
#endif
	EVT_TOOL(XRCID("ID_TOOLBAR_FIND"), CMainFrame::OnSearch)
	EVT_MENU(XRCID("ID_MENU_SERVER_SEARCH"), CMainFrame::OnSearch)
	EVT_MENU(XRCID("ID_MENU_FILE_NEWTAB"), CMainFrame::OnMenuNewTab)
	EVT_MENU(XRCID("ID_MENU_FILE_CLOSETAB"), CMainFrame::OnMenuCloseTab)
END_EVENT_TABLE()

class CMainFrameStateEventHandler : public CStateEventHandler
{
public:
	CMainFrameStateEventHandler(CMainFrame* pMainFrame)
		: CStateEventHandler(0)
	{
		m_pMainFrame = pMainFrame;

		CContextManager::Get()->RegisterHandler(this, STATECHANGE_REMOTE_IDLE, false, true);
		CContextManager::Get()->RegisterHandler(this, STATECHANGE_SERVER, false, true);

		CContextManager::Get()->RegisterHandler(this, STATECHANGE_CHANGEDCONTEXT, false, false);
	}

protected:
	virtual void OnStateChange(CState* pState, enum t_statechange_notifications notification, const wxString&, const void*)
	{
		if (notification == STATECHANGE_CHANGEDCONTEXT)
		{
			// Update window title
			const CServer* pServer = pState ? pState->GetServer() : 0;
			if (!pServer)
				m_pMainFrame->SetTitle(_T("FileZilla"));
			else
				m_pMainFrame->SetTitle(pState->GetTitle() + _T(" - FileZilla"));

			return;
		}

		if (!pState)
			return;

		CContextControl::_context_controls* controls = m_pMainFrame->m_pContextControl->GetControlsFromState(pState);
		if (!controls)
			return;

		if (controls->tab_index == -1)
		{
			if (notification == STATECHANGE_REMOTE_IDLE || notification == STATECHANGE_SERVER)
				pState->Disconnect();

			return;
		}

		if (notification == STATECHANGE_SERVER)
		{
			const CServer* pServer = pState->GetServer();

			if (pState == CContextManager::Get()->GetCurrentContext())
			{
				if (!pServer)
					m_pMainFrame->SetTitle(_T("FileZilla"));
				else
				{
					m_pMainFrame->SetTitle(pState->GetTitle() + _T(" - FileZilla"));
					if (pServer->GetName().empty())
					{
						// Can only happen through quickconnect bar
						CMenuBar* pMenuBar = wxDynamicCast(m_pMainFrame->GetMenuBar(), CMenuBar);
						if (pMenuBar)
							pMenuBar->ClearBookmarks();
					}
				}
			}

			return;
		}
	}

	CMainFrame* m_pMainFrame;
};

CMainFrame::CMainFrame()
	: m_engineContext(*COptions::Get())
	, m_comparisonToggleAcceleratorId(wxNewId())
{
#ifdef __WXMAC__
	keyboardCommands[wxNewId()] = std::make_pair([](wxTextEntry* e){ e->Cut(); }, 'X');
	keyboardCommands[wxNewId()] = std::make_pair([](wxTextEntry* e){ e->Copy(); }, 'C');
	keyboardCommands[wxNewId()] = std::make_pair([](wxTextEntry* e){ e->Paste(); }, 'V');
	keyboardCommands[wxNewId()] = std::make_pair([](wxTextEntry* e){ e->SelectAll(); }, 'A');
#endif

	m_pActivityLed[0] = m_pActivityLed[1] = 0;

	wxGetApp().AddStartupProfileRecord(_T("CMainFrame::CMainFrame"));
	wxRect screen_size = CWindowStateManager::GetScreenDimensions();

	wxSize initial_size;
	initial_size.x = wxMin(900, screen_size.GetWidth() - 10);
	initial_size.y = wxMin(750, screen_size.GetHeight() - 50);

	Create(NULL, -1, _T("FileZilla"), wxDefaultPosition, initial_size);
	SetSizeHints(250, 250);

#ifdef __WXMSW__
	// In order for the --close commandline argument to work,
	// there has to be a way to find other instances.
	// Create a hidden window with a title no other program uses
	wxWindow* pChild = new wxWindow();
	pChild->Hide();
	pChild->Create(this, wxID_ANY);
	::SetWindowText((HWND)pChild->GetHandle(), _T("FileZilla process identificator 3919DB0A-082D-4560-8E2F-381A35969FB4"));
#endif

#ifdef __WXMSW__
	SetIcon(wxICON(appicon));
#else
	SetIcons(CThemeProvider::GetIconBundle(_T("ART_FILEZILLA")));
#endif

	m_pThemeProvider = new CThemeProvider();

	CPowerManagement::Create(this);

	// It's important that the context control gets created before our own state handler
	// so that contextchange events can be processed in the right order.
	m_pContextControl = new CContextControl(this);

	m_pStatusBar = new CStatusBar(this);
	if (m_pStatusBar)
	{
		m_pActivityLed[0] = new CLed(m_pStatusBar, 0);
		m_pActivityLed[1] = new CLed(m_pStatusBar, 1);

		m_pStatusBar->AddField(-1, widget_led_recv, m_pActivityLed[1]);
		m_pStatusBar->AddField(-1, widget_led_send, m_pActivityLed[0]);

		SetStatusBar(m_pStatusBar);
	}

	m_closeEventTimer.SetOwner(this);

	if (CFilterManager::HasActiveFilters(true))
	{
		if (COptions::Get()->GetOptionVal(OPTION_FILTERTOGGLESTATE))
			CFilterManager::ToggleFilters();
	}

	CreateMenus();
	CreateMainToolBar();
	if (COptions::Get()->GetOptionVal(OPTION_SHOW_QUICKCONNECT))
		CreateQuickconnectBar();

	m_pAsyncRequestQueue = new CAsyncRequestQueue(this);

#ifdef __WXMSW__
	long style = wxSP_NOBORDER | wxSP_LIVE_UPDATE;
#elif !defined(__WXMAC__)
	long style = wxSP_3DBORDER | wxSP_LIVE_UPDATE;
#else
	long style = wxSP_LIVE_UPDATE;
#endif

	wxSize clientSize = GetClientSize();

	m_pTopSplitter = new CSplitterWindowEx(this, -1, wxDefaultPosition, clientSize, style);
	m_pTopSplitter->SetMinimumPaneSize(50);

	m_pBottomSplitter = new CSplitterWindowEx(m_pTopSplitter, -1, wxDefaultPosition, wxDefaultSize, wxSP_NOBORDER | wxSP_LIVE_UPDATE);
	m_pBottomSplitter->SetMinimumPaneSize(10, 60);
	m_pBottomSplitter->SetSashGravity(1.0);

	const int message_log_position = COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_POSITION);
	m_pQueueLogSplitter = new CSplitterWindowEx(m_pBottomSplitter, -1, wxDefaultPosition, wxDefaultSize, wxSP_NOBORDER | wxSP_LIVE_UPDATE);
	m_pQueueLogSplitter->SetMinimumPaneSize(50, 250);
	m_pQueueLogSplitter->SetSashGravity(0.5);
	m_pQueuePane = new CQueue(m_pQueueLogSplitter, this, m_pAsyncRequestQueue);

	if (message_log_position == 1)
		m_pStatusView = new CStatusView(m_pQueueLogSplitter, -1);
	else
		m_pStatusView = new CStatusView(m_pTopSplitter, -1);

	m_pQueueView = m_pQueuePane->GetQueueView();

	m_pContextControl->Create(m_pBottomSplitter);

	m_pStateEventHandler = new CMainFrameStateEventHandler(this);

	m_pContextControl->CreateTab();

	switch (message_log_position)
	{
	case 1:
		m_pTopSplitter->Initialize(m_pBottomSplitter);
		if (COptions::Get()->GetOptionVal(OPTION_SHOW_MESSAGELOG))
		{
			if (COptions::Get()->GetOptionVal(OPTION_SHOW_QUEUE))
				m_pQueueLogSplitter->SplitVertically(m_pQueuePane, m_pStatusView);
			else
			{
				m_pQueueLogSplitter->Initialize(m_pStatusView);
				m_pQueuePane->Hide();
			}
		}
		else
		{
			if (COptions::Get()->GetOptionVal(OPTION_SHOW_QUEUE))
			{
				m_pStatusView->Hide();
				m_pQueueLogSplitter->Initialize(m_pQueuePane);
			}
			else
			{
				m_pQueuePane->Hide();
				m_pStatusView->Hide();
				m_pQueueLogSplitter->Hide();
			}
		}
		break;
	case 2:
		m_pTopSplitter->Initialize(m_pBottomSplitter);
		if (COptions::Get()->GetOptionVal(OPTION_SHOW_QUEUE))
			m_pQueueLogSplitter->Initialize(m_pQueuePane);
		else
		{
			m_pQueueLogSplitter->Hide();
			m_pQueuePane->Hide();
		}
		m_pQueuePane->AddPage(m_pStatusView, _("Message log"));
		break;
	default:
		if (COptions::Get()->GetOptionVal(OPTION_SHOW_QUEUE))
			m_pQueueLogSplitter->Initialize(m_pQueuePane);
		else
		{
			m_pQueuePane->Hide();
			m_pQueueLogSplitter->Hide();
		}
		if (COptions::Get()->GetOptionVal(OPTION_SHOW_MESSAGELOG))
			m_pTopSplitter->SplitHorizontally(m_pStatusView, m_pBottomSplitter);
		else
		{
			m_pStatusView->Hide();
			m_pTopSplitter->Initialize(m_pBottomSplitter);
		}
		break;
	}

	if (m_pQueueLogSplitter->IsShown())
		m_pBottomSplitter->SplitHorizontally(m_pContextControl, m_pQueueLogSplitter);
	else
	{
		m_pQueueLogSplitter->Hide();
		m_pBottomSplitter->Initialize(m_pContextControl);
	}

	m_pWindowStateManager = new CWindowStateManager(this);
	m_pWindowStateManager->Restore(OPTION_MAINWINDOW_POSITION);

	Layout();
	HandleResize();

	if (!RestoreSplitterPositions())
		SetDefaultSplitterPositions();

	std::vector<wxAcceleratorEntry> entries;
	//entries[0].Set(wxACCEL_CMD | wxACCEL_SHIFT, 'I', XRCID("ID_MENU_VIEW_FILTERS"));
	for (int i = 0; i < 10; i++) {
		tab_hotkey_ids[i] = wxNewId();
		entries.push_back(wxAcceleratorEntry(wxACCEL_CMD, (int)'0' + i, tab_hotkey_ids[i]));
	}
	entries.push_back(wxAcceleratorEntry(wxACCEL_CMD | wxACCEL_SHIFT, 'O', m_comparisonToggleAcceleratorId));
#ifdef __WXMAC__
	for (auto it = keyboardCommands.begin(); it != keyboardCommands.end(); ++it) {
		entries.push_back(wxAcceleratorEntry(wxACCEL_CMD, it->second.second, it->first));
	}
#endif
	wxAcceleratorTable accel(entries.size(), &entries[0]);
	SetAcceleratorTable(accel);

	ConnectNavigationHandler(m_pStatusView);
	ConnectNavigationHandler(m_pQueuePane);

	CEditHandler::Create()->SetQueue(m_pQueueView);

	CAutoAsciiFiles::SettingsChanged();

	FixTabOrder();
}

CMainFrame::~CMainFrame()
{
	CPowerManagement::Destroy();

	delete m_pStateEventHandler;

	CContextManager::Get()->DestroyAllStates();
	delete m_pAsyncRequestQueue;
#if FZ_MANUALUPDATECHECK
	delete m_pUpdater;
#endif

	CEditHandler* pEditHandler = CEditHandler::Get();
	if (pEditHandler)
	{
		// This might leave temporary files behind,
		// edit handler should clean them on next startup
		pEditHandler->Release();
	}

#ifndef __WXMAC__
	delete m_taskBarIcon;
#endif
}

void CMainFrame::HandleResize()
{
	wxSize clientSize = GetClientSize();
	if (clientSize.y <= 0) // Can happen if restoring from tray on XP if using ugly XP themes
		return;

	if (m_pQuickconnectBar)
		m_pQuickconnectBar->SetSize(0, 0, clientSize.GetWidth(), -1, wxSIZE_USE_EXISTING);
	if (m_pTopSplitter)
	{
		if (!m_pQuickconnectBar)
			m_pTopSplitter->SetSize(0, 0, clientSize.GetWidth(), clientSize.GetHeight());
		else
		{
			wxSize panelSize = m_pQuickconnectBar->GetSize();
			m_pTopSplitter->SetSize(0, panelSize.GetHeight(), clientSize.GetWidth(), clientSize.GetHeight() - panelSize.GetHeight());
		}
	}
}

void CMainFrame::OnSize(wxSizeEvent &event)
{
	wxFrame::OnSize(event);

	if (!m_pBottomSplitter)
		return;

	HandleResize();

#ifdef __WXGTK__
	if (m_pWindowStateManager && m_pWindowStateManager->m_maximize_requested && IsMaximized())
	{
		m_pWindowStateManager->m_maximize_requested = 0;
		if (!RestoreSplitterPositions())
			SetDefaultSplitterPositions();
	}
#endif
}

bool CMainFrame::CreateMenus()
{
	wxGetApp().AddStartupProfileRecord(_T("CMainFrame::CreateMenus"));
	CMenuBar* old = m_pMenuBar;

	m_pMenuBar = CMenuBar::Load(this);

	if (!m_pMenuBar) {
		m_pMenuBar = old;
		return false;
	}

	SetMenuBar(m_pMenuBar);
	delete old;

	return true;
}

bool CMainFrame::CreateQuickconnectBar()
{
	wxGetApp().AddStartupProfileRecord(_T("CMainFrame::CreateQuickconnectBar"));
	delete m_pQuickconnectBar;

	m_pQuickconnectBar = new CQuickconnectBar();
	if (!m_pQuickconnectBar->Create(this))
	{
		delete m_pQuickconnectBar;
		m_pQuickconnectBar = 0;
	}
	else
	{
		wxSize clientSize = GetClientSize();
		if (m_pTopSplitter)
		{
			wxSize panelSize = m_pQuickconnectBar->GetSize();
			m_pTopSplitter->SetSize(-1, panelSize.GetHeight(), -1, clientSize.GetHeight() - panelSize.GetHeight(), wxSIZE_USE_EXISTING);
		}
		m_pQuickconnectBar->SetSize(0, 0, clientSize.GetWidth(), -1);
	}

	return true;
}

void CMainFrame::OnMenuHandler(wxCommandEvent &event)
{
	if (event.GetId() == XRCID("wxID_EXIT")) {
		Close();
	}
	else if (event.GetId() == XRCID("ID_MENU_FILE_SITEMANAGER")) {
		OpenSiteManager();
	}
	else if (event.GetId() == XRCID("ID_MENU_FILE_COPYSITEMANAGER")) {
		CState* pState = CContextManager::Get()->GetCurrentContext();
		const CServer* pServer = pState ? pState->GetServer() : 0;
		if (!pServer) {
			wxMessageBoxEx(_("Not connected to any server."), _("Cannot add server to Site Manager"), wxICON_EXCLAMATION);
			return;
		}
		OpenSiteManager(pServer);
	}
	else if (event.GetId() == XRCID("ID_MENU_SERVER_CMD")) {
		CState* pState = CContextManager::Get()->GetCurrentContext();
		if (!pState || !pState->m_pCommandQueue || !pState->IsRemoteConnected() || !pState->IsRemoteIdle())
			return;

		CInputDialog dlg;
		dlg.Create(this, _("Enter custom command"), _("Please enter raw FTP command.\nUsing raw ftp commands will clear the directory cache."));
		if (dlg.ShowModal() != wxID_OK)
			return;

		pState = CContextManager::Get()->GetCurrentContext();
		if (!pState || !pState->m_pCommandQueue || !pState->IsRemoteConnected() || !pState->IsRemoteIdle()) {
			wxBell();
			return;
		}

		const wxString &command = dlg.GetValue();

		if (!command.Left(5).CmpNoCase(_T("quote")) || !command.Left(6).CmpNoCase(_T("quote "))) {
			CConditionalDialog condDlg(this, CConditionalDialog::rawcommand_quote, CConditionalDialog::yesno);
			condDlg.SetTitle(_("Raw FTP command"));

			condDlg.AddText(_("'quote' is usually a local command used by commandline clients to send the arguments following 'quote' to the server. You might want to enter the raw command without the leading 'quote'."));
			condDlg.AddText(wxString::Format(_("Do you really want to send '%s' to the server?"), command));

			if (!condDlg.Run())
				return;
		}

		pState = CContextManager::Get()->GetCurrentContext();
		if (!pState || !pState->m_pCommandQueue || !pState->IsRemoteConnected() || !pState->IsRemoteIdle()) {
			wxBell();
			return;
		}
		pState->m_pCommandQueue->ProcessCommand(new CRawCommand(dlg.GetValue()));
	}
	else if (event.GetId() == XRCID("wxID_PREFERENCES")) {
		OnMenuEditSettings(event);
	}
	else if (event.GetId() == XRCID("ID_MENU_EDIT_NETCONFWIZARD")) {
		CNetConfWizard wizard(this, COptions::Get(), m_engineContext);
		wizard.Load();
		wizard.Run();
	}
	// Debug menu
	else if (event.GetId() == XRCID("ID_CIPHERS")) {
		CInputDialog dlg;
		dlg.Create(this, _T("Ciphers"), _T("Priority string:"));
		dlg.AllowEmpty(true);
		if (dlg.ShowModal() == wxID_OK)
			wxMessageBoxEx(ListTlsCiphers(dlg.GetValue()), _T("Ciphers"));
	}
	else if (event.GetId() == XRCID("ID_CLEARCACHE_LAYOUT")) {
		CWrapEngine::ClearCache();
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_FILEEXISTS")) {
		CDefaultFileExistsDlg dlg;
		if (!dlg.Load(this, false))
			return;

		dlg.Run();
	}
	else if (event.GetId() == XRCID("ID_MENU_EDIT_CLEARPRIVATEDATA")) {
		CClearPrivateDataDialog* pDlg = CClearPrivateDataDialog::Create(this);
		if (!pDlg)
			return;

		pDlg->Run();
		pDlg->Delete();

		if (m_pMenuBar)
			m_pMenuBar->UpdateMenubarState();
		if (m_pToolBar)
			m_pToolBar->UpdateToolbarState();
	}
	else if (event.GetId() == XRCID("ID_MENU_SERVER_VIEWHIDDEN")) {
		bool showHidden = COptions::Get()->GetOptionVal(OPTION_VIEW_HIDDEN_FILES) ? 0 : 1;
		if (showHidden) {
			CConditionalDialog dlg(this, CConditionalDialog::viewhidden, CConditionalDialog::ok, false);
			dlg.SetTitle(_("Force showing hidden files"));

			dlg.AddText(_("Note that this feature is only supported using the FTP protocol."));
			dlg.AddText(_("A proper server always shows all files, but some broken servers hide files from the user. Use this option to force the server to show all files."));
			dlg.AddText(_("Keep in mind that not all servers support this feature and may return incorrect listings if this option is enabled. Although FileZilla performs some tests to check if the server supports this feature, the test may fail."));
			dlg.AddText(_("Disable this option again if you will not be able to see the correct directory contents anymore."));
			(void)dlg.Run();
		}

		COptions::Get()->SetOption(OPTION_VIEW_HIDDEN_FILES, showHidden ? 1 : 0);
		const std::vector<CState*> *pStates = CContextManager::Get()->GetAllStates();
		for (auto & pState : *pStates) {
			CServerPath path = pState->GetRemotePath();
			if (!path.empty() && pState->m_pCommandQueue)
				pState->ChangeRemoteDir(path, _T(""), LIST_FLAG_REFRESH);
		}
	}
	else if (event.GetId() == XRCID("ID_EXPORT")) {
		CExportDialog dlg(this, m_pQueueView);
		dlg.Run();
	}
	else if (event.GetId() == XRCID("ID_IMPORT")) {
		CImportDialog dlg(this, m_pQueueView);
		dlg.Run();
	}
	else if (event.GetId() == XRCID("ID_MENU_FILE_EDITED")) {
		CEditHandlerStatusDialog dlg(this);
		dlg.ShowModal();
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_TYPE_AUTO")) {
		COptions::Get()->SetOption(OPTION_ASCIIBINARY, 0);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_TYPE_ASCII")) {
		COptions::Get()->SetOption(OPTION_ASCIIBINARY, 1);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_TYPE_BINARY")) {
		COptions::Get()->SetOption(OPTION_ASCIIBINARY, 2);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_PRESERVETIMES")) {
		if (event.IsChecked()) {
			CConditionalDialog dlg(this, CConditionalDialog::confirm_preserve_timestamps, CConditionalDialog::ok, true);
			dlg.SetTitle(_("Preserving file timestamps"));
			dlg.AddText(_("Please note that preserving timestamps on uploads on FTP, FTPS and FTPES servers only works if they support the MFMT command."));
			dlg.Run();
		}
		COptions::Get()->SetOption(OPTION_PRESERVE_TIMESTAMPS, event.IsChecked() ? 1 : 0);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_PROCESSQUEUE")) {
		if (m_pQueueView)
			m_pQueueView->SetActive(event.IsChecked());
	}
	else if (event.GetId() == XRCID("ID_MENU_HELP_GETTINGHELP") ||
			 event.GetId() == XRCID("ID_MENU_HELP_BUGREPORT"))
	{
		wxString url(_T("https://filezilla-project.org/support.php?type=client&mode="));
		if (event.GetId() == XRCID("ID_MENU_HELP_GETTINGHELP"))
			url += _T("help");
		else
			url += _T("bugreport");
		wxString version = CBuildInfo::GetVersion();
		if (version != _T("custom build")) {
			url += _T("&version=");
			// We need to urlencode version number

			// Unbelievable, but wxWidgets does not have any method
			// to urlencode strings.
			// Do a crude approach: Drop everything unexpected...
			for (unsigned int i = 0; i < version.Len(); i++) {
				wxChar c = version.GetChar(i);
				if ((c >= '0' && c <= '9') ||
					(c >= 'a' && c <= 'z') ||
					(c >= 'A' && c <= 'Z') ||
					c == '-' || c == '.' ||
					c == '_')
				{
					url.Append(c);
				}
			}
		}
		wxLaunchDefaultBrowser(url);
	}
	else if (event.GetId() == XRCID("ID_MENU_VIEW_FILELISTSTATUSBAR")) {
		bool show = COptions::Get()->GetOptionVal(OPTION_FILELIST_STATUSBAR) == 0;
		COptions::Get()->SetOption(OPTION_FILELIST_STATUSBAR, show ? 1 : 0);
		CContextControl::_context_controls* controls = m_pContextControl ? m_pContextControl->GetCurrentControls() : 0;
		if (controls && controls->pLocalListViewPanel) {
			wxStatusBar* pStatusBar = controls->pLocalListViewPanel->GetStatusBar();
			if (pStatusBar) {
				pStatusBar->Show(show);
				wxSizeEvent evt;
				controls->pLocalListViewPanel->ProcessWindowEvent(evt);
			}
		}
		if (controls && controls->pRemoteListViewPanel) {
			wxStatusBar* pStatusBar = controls->pRemoteListViewPanel->GetStatusBar();
			if (pStatusBar) {
				pStatusBar->Show(show);
				wxSizeEvent evt;
				controls->pRemoteListViewPanel->ProcessWindowEvent(evt);
			}
		}
	}
	else if (event.GetId() == XRCID("ID_VIEW_QUICKCONNECT")) {
		if (!m_pQuickconnectBar)
			CreateQuickconnectBar();
		else {
			m_pQuickconnectBar->Destroy();
			m_pQuickconnectBar = 0;
			wxSize clientSize = GetClientSize();
			m_pTopSplitter->SetSize(0, 0, clientSize.GetWidth(), clientSize.GetHeight());
		}
		COptions::Get()->SetOption(OPTION_SHOW_QUICKCONNECT, m_pQuickconnectBar != 0);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_MANUAL")) {
		CState* pState = CContextManager::Get()->GetCurrentContext();
		if (!pState || !m_pQueueView) {
			wxBell();
			return;
		}
		CManualTransfer dlg(m_pQueueView);
		dlg.Run(this, pState);
	}
	else if (event.GetId() == XRCID("ID_BOOKMARK_ADD") || event.GetId() == XRCID("ID_BOOKMARK_MANAGE")) {
		CServer server;
		CState* pState = CContextManager::Get()->GetCurrentContext();
		if (!pState) {
			return;
		}
		const CServer* pServer = pState ? pState->GetServer() : 0;

		CContextControl::_context_controls* controls = m_pContextControl->GetCurrentControls();
		if (!controls)
			return;
		if (!pServer && !controls->site_bookmarks->path.empty()) {
			// Get server from site manager
			std::unique_ptr<CSiteManagerItemData_Site> data = CSiteManager::GetSiteByPath(controls->site_bookmarks->path);
			if (data) {
				server = data->m_server;
				pServer = &server;
			}
			else {
				controls->site_bookmarks->path.clear();
				controls->site_bookmarks->bookmarks.clear();
				if (m_pMenuBar)
					m_pMenuBar->UpdateBookmarkMenu();
			}
		}

		// controls->last_bookmark_path can get modified if it's empty now
		int res;
		if (event.GetId() == XRCID("ID_BOOKMARK_ADD")) {
			CNewBookmarkDialog dlg(this, controls->site_bookmarks->path, pServer);
			res = dlg.Run(pState->GetLocalDir().GetPath(), pState->GetRemotePath());
		}
		else {
			CBookmarksDialog dlg(this, controls->site_bookmarks->path, pServer);

			res = dlg.Run(pState->GetLocalDir().GetPath(), pState->GetRemotePath());
		}
		if (res == wxID_OK) {
			controls->site_bookmarks->bookmarks.clear();
			CSiteManager::GetBookmarks(controls->site_bookmarks->path, controls->site_bookmarks->bookmarks);
			if (m_pMenuBar)
				m_pMenuBar->UpdateBookmarkMenu();
		}
	}
	else if (event.GetId() == XRCID("ID_MENU_HELP_WELCOME")) {
		CWelcomeDialog dlg;
		dlg.Run(this, true);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_SPEEDLIMITS_ENABLE")) {
		bool enable = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_ENABLE) == 0;

		const int downloadLimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_INBOUND);
		const int uploadLimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_OUTBOUND);
		if (enable && !downloadLimit && !uploadLimit) {
			CSpeedLimitsDialog dlg;
			dlg.Run(this);
		}
		else
			COptions::Get()->SetOption(OPTION_SPEEDLIMIT_ENABLE, enable ? 1 : 0);
	}
	else if (event.GetId() == XRCID("ID_MENU_TRANSFER_SPEEDLIMITS_CONFIGURE")) {
		CSpeedLimitsDialog dlg;
		dlg.Run(this);
	}
	else if (event.GetId() == m_comparisonToggleAcceleratorId) {
		CState* pState = CContextManager::Get()->GetCurrentContext();
		if (!pState)
			return;

		int old_mode = COptions::Get()->GetOptionVal(OPTION_COMPARISONMODE);
		COptions::Get()->SetOption(OPTION_COMPARISONMODE, old_mode ? 0 : 1);

		CComparisonManager* pComparisonManager = pState->GetComparisonManager();
		if (pComparisonManager && pComparisonManager->IsComparing())
			pComparisonManager->CompareListings();
	}
	else if (HandleKeyboardCommand(event, *this)) {
		return;
	}
	else {
		for (int i = 0; i < 10; ++i) {
			if (event.GetId() != tab_hotkey_ids[i])
				continue;

			if (!m_pContextControl)
				return;

			int sel = i - 1;
			if (sel < 0)
				sel = 9;
			m_pContextControl->SelectTab(sel);

			return;
		}

		std::unique_ptr<CSiteManagerItemData_Site> pData = CSiteManager::GetSiteById(event.GetId());

		if (!pData) {
			event.Skip();
		}
		else {
			ConnectToSite(*pData);
		}
	}
}

void CMainFrame::OnEngineEvent(wxFzEvent &event)
{
	const std::vector<CState*> *pStates = CContextManager::Get()->GetAllStates();
	CState* pState = 0;
	for (std::vector<CState*>::const_iterator iter = pStates->begin(); iter != pStates->end(); ++iter) {
		if ((*iter)->m_pEngine != event.engine_)
			continue;

		pState = *iter;
		break;
	}
	if (!pState)
		return;

	std::unique_ptr<CNotification> pNotification = pState->m_pEngine->GetNextNotification();
	while (pNotification) {
		switch (pNotification->GetID())
		{
		case nId_logmsg:
			m_pStatusView->AddToLog(static_cast<CLogmsgNotification&>(*pNotification.get()));
			if (COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_POSITION) == 2)
				m_pQueuePane->Highlight(3);
			break;
		case nId_operation:
			pState->m_pCommandQueue->Finish(unique_static_cast<COperationNotification>(std::move(pNotification)));
			if (m_bQuit) {
				Close();
				return;
			}
			break;
		case nId_listing:
			{
				auto const& listingNotification = static_cast<CDirectoryListingNotification const&>(*pNotification.get());

				if (listingNotification.GetPath().empty())
					pState->SetRemoteDir(0, false);
				else {
					std::shared_ptr<CDirectoryListing> pListing = std::make_shared<CDirectoryListing>();
					if (listingNotification.Failed() ||
						pState->m_pEngine->CacheLookup(listingNotification.GetPath(), *pListing) != FZ_REPLY_OK)
					{
						pListing = std::make_shared<CDirectoryListing>();
						pListing->path = listingNotification.GetPath();
						pListing->m_flags |= CDirectoryListing::listing_failed;
						pListing->m_firstListTime = CMonotonicTime::Now();
					}

					pState->SetRemoteDir(pListing, listingNotification.Modified());
				}
			}
			break;
		case nId_asyncrequest:
			{
				auto pAsyncRequest = unique_static_cast<CAsyncRequestNotification>(std::move(pNotification));
				if (pAsyncRequest->GetRequestID() == reqId_fileexists)
					m_pQueueView->ProcessNotification(pState->m_pEngine, std::move(pAsyncRequest));
				else {
					if (pAsyncRequest->GetRequestID() == reqId_certificate)
						pState->SetSecurityInfo(static_cast<CCertificateNotification&>(*pAsyncRequest));
					m_pAsyncRequestQueue->AddRequest(pState->m_pEngine, std::move(pAsyncRequest));
				}
			}
			break;
		case nId_active:
			{
				CActiveNotification const& activeNotification = static_cast<CActiveNotification const&>(*pNotification.get());
				UpdateActivityLed(activeNotification.GetDirection());
			}
			break;
		case nId_transferstatus:
			m_pQueueView->ProcessNotification(pState->m_pEngine, std::move(pNotification));
			break;
		case nId_sftp_encryption:
			{
				pState->SetSecurityInfo(static_cast<CSftpEncryptionNotification&>(*pNotification));
			}
			break;
		case nId_local_dir_created:
			if (pState) {
				auto const& localDirCreatedNotification = static_cast<CLocalDirCreatedNotification const&>(*pNotification.get());
				pState->LocalDirCreated(localDirCreatedNotification.dir);
			}
			break;
		default:
			break;
		}

		pNotification = pState->m_pEngine->GetNextNotification();
	}
}

void CMainFrame::OnUpdateLedTooltip(wxCommandEvent&)
{
	wxString tooltipText;

	wxFileOffset downloadSpeed = m_pQueueView ? m_pQueueView->GetCurrentDownloadSpeed() : 0;
	wxFileOffset uploadSpeed = m_pQueueView ? m_pQueueView->GetCurrentUploadSpeed() : 0;

	CSizeFormat::_format format = static_cast<CSizeFormat::_format>(COptions::Get()->GetOptionVal(OPTION_SIZE_FORMAT));
	if (format == CSizeFormat::bytes)
		format = CSizeFormat::iec;

	const wxString downloadSpeedStr = CSizeFormat::Format(downloadSpeed, true, format,
														  COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0,
														  COptions::Get()->GetOptionVal(OPTION_SIZE_DECIMALPLACES));
	const wxString uploadSpeedStr = CSizeFormat::Format(uploadSpeed, true, format,
														COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0,
														COptions::Get()->GetOptionVal(OPTION_SIZE_DECIMALPLACES));
	tooltipText.Printf(_("Download speed: %s/s\nUpload speed: %s/s"), downloadSpeedStr, uploadSpeedStr);

	m_pActivityLed[0]->SetToolTip(tooltipText);
	m_pActivityLed[1]->SetToolTip(tooltipText);
}

bool CMainFrame::CreateMainToolBar()
{
	wxGetApp().AddStartupProfileRecord(_T("CMainFrame::CreateMainToolBar"));
	if (m_pToolBar)
	{
#ifdef __WXMAC__
		if (m_pToolBar)
			COptions::Get()->SetOption(OPTION_TOOLBAR_HIDDEN, m_pToolBar->IsShown() ? 0 : 1);
#endif
		SetToolBar(0);
		delete m_pToolBar;
		m_pToolBar = 0;
	}

#ifndef __WXMAC__
	if (COptions::Get()->GetOptionVal(OPTION_TOOLBAR_HIDDEN) != 0)
		return true;
#endif

	m_pToolBar = CToolBar::Load(this);
	if (!m_pToolBar)
	{
		wxLogError(_("Cannot load toolbar from resource file"));
		return false;
	}
	SetToolBar(m_pToolBar);

	if (m_pQuickconnectBar)
		m_pQuickconnectBar->Refresh();

	return true;
}

void CMainFrame::OnDisconnect(wxCommandEvent&)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState || !pState->IsRemoteConnected())
		return;

	if (!pState->IsRemoteIdle())
		return;

	pState->Disconnect();
}

void CMainFrame::OnCancel(wxCommandEvent&)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState || pState->m_pCommandQueue->Idle())
		return;

	if (wxMessageBoxEx(_("Really cancel current operation?"), _T("FileZilla"), wxYES_NO | wxICON_QUESTION) == wxYES)
	{
		pState->m_pCommandQueue->Cancel();
		pState->GetRecursiveOperationHandler()->StopRecursiveOperation();
	}
}

#ifdef __WXMSW__

BOOL CALLBACK FzEnumThreadWndProc(HWND hwnd, LPARAM lParam)
{
	// This function enumerates all dialogs and calls EndDialog for them
	TCHAR buffer[10];
	int c = GetClassName(hwnd, buffer, 9);
	// #32770 is the dialog window class.
	if (c && !_tcscmp(buffer, _T("#32770")))
	{
		*((bool*)lParam) = true;
		EndDialog(hwnd, IDCANCEL);
		return FALSE;
	}

	return TRUE;
}
#endif //__WXMSW__


bool CMainFrame::CloseDialogsAndQuit(wxCloseEvent &event)
{
#ifndef __WXMAC__
	if (m_taskBarIcon)
	{
		delete m_taskBarIcon;
		m_taskBarIcon = 0;
		m_closeEvent = event.GetEventType();
		m_closeEventTimer.Start(1, true);
		return false;
	}
#endif

	// We need to close all other top level windows on the stack before closing the main frame.
	// In other words, all open dialogs need to be closed.
	static int prev_size = 0;

	int size = wxTopLevelWindows.size();
	static wxTopLevelWindow* pLast = 0;
	if (wxTopLevelWindows.size())
	{
		wxWindowList::reverse_iterator iter = wxTopLevelWindows.rbegin();
		wxTopLevelWindow* pTop = (wxTopLevelWindow*)(*iter);
		while (pTop != this && (size != prev_size || pLast != pTop))
		{
			wxDialog* pDialog = wxDynamicCast(pTop, wxDialog);
			if (pDialog)
				pDialog->EndModal(wxID_CANCEL);
			else
			{
				wxWindow* pParent = pTop->GetParent();
				if (m_pQueuePane && pParent == m_pQueuePane)
				{
					// It's the AUI frame manager hint window. Ignore it
					++iter;
					if (iter == wxTopLevelWindows.rend())
						break;
					pTop = (wxTopLevelWindow*)(*iter);
					continue;
				}
				wxString title = pTop->GetTitle();
				pTop->Destroy();
			}

			prev_size = size;
			pLast = pTop;

			m_closeEvent = event.GetEventType();
			m_closeEventTimer.Start(1, true);

			return false;
		}
	}

#ifdef __WXMSW__
	// wxMessageBoxEx does not use wxTopLevelWindow, close it too
	bool dialog = false;
	EnumThreadWindows(GetCurrentThreadId(), FzEnumThreadWndProc, (LPARAM)&dialog);
	if (dialog)
	{
		m_closeEvent = event.GetEventType();
		m_closeEventTimer.Start(1, true);

		return false;
	}
#endif //__WXMSW__

	// At this point all other top level windows should be closed.
	return true;
}


void CMainFrame::OnClose(wxCloseEvent &event)
{
	if (!m_bQuit) {
		static bool quit_confirmation_displayed = false;
		if (quit_confirmation_displayed && event.CanVeto()) {
			event.Veto();
			return;
		}
		if (event.CanVeto()) {
			quit_confirmation_displayed = true;

			if (m_pQueueView && m_pQueueView->IsActive()) {
				CConditionalDialog dlg(this, CConditionalDialog::confirmexit, CConditionalDialog::yesno);
				dlg.SetTitle(_("Close FileZilla"));

				dlg.AddText(_("File transfers still in progress."));
				dlg.AddText(_("Do you really want to close FileZilla?"));

				if (!dlg.Run()) {
					event.Veto();
					quit_confirmation_displayed = false;
					return;
				}
				if (m_bQuit)
					return;
			}

			CEditHandler* pEditHandler = CEditHandler::Get();
			if (pEditHandler) {
				if (pEditHandler->GetFileCount(CEditHandler::remote, CEditHandler::edit) || pEditHandler->GetFileCount(CEditHandler::none, CEditHandler::upload) ||
					pEditHandler->GetFileCount(CEditHandler::none, CEditHandler::upload_and_remove) ||
					pEditHandler->GetFileCount(CEditHandler::none, CEditHandler::upload_and_remove_failed))
				{
					CConditionalDialog dlg(this, CConditionalDialog::confirmexit_edit, CConditionalDialog::yesno);
					dlg.SetTitle(_("Close FileZilla"));

					dlg.AddText(_("Some files are still being edited or need to be uploaded."));
					dlg.AddText(_("If you close FileZilla, your changes will be lost."));
					dlg.AddText(_("Do you really want to close FileZilla?"));

					if (!dlg.Run()) {
						event.Veto();
						quit_confirmation_displayed = false;
						return;
					}
					if (m_bQuit)
						return;
				}
			}
			quit_confirmation_displayed = false;
		}

		if (m_pWindowStateManager) {
			m_pWindowStateManager->Remember(OPTION_MAINWINDOW_POSITION);
			delete m_pWindowStateManager;
			m_pWindowStateManager = 0;
		}

		RememberSplitterPositions();

#ifdef __WXMAC__
		if (m_pToolBar)
			COptions::Get()->SetOption(OPTION_TOOLBAR_HIDDEN, m_pToolBar->IsShown() ? 0 : 1);
#endif
		m_bQuit = true;
	}

	Show(false);
	if (!CloseDialogsAndQuit(event))
		return;

	// Getting deleted by wxWidgets
	for (int i = 0; i < 2; i++)
		m_pActivityLed[i] = 0;
	m_pStatusBar = 0;
	m_pMenuBar = 0;
	m_pToolBar = 0;

	// We're no longer interested in these events
	delete m_pStateEventHandler;
	m_pStateEventHandler = 0;

	if (m_pQueueView && !m_pQueueView->Quit()) {
		if( event.CanVeto() ) {
			event.Veto();
		}
		return;
	}

	CEditHandler* pEditHandler = CEditHandler::Get();
	if (pEditHandler) {
		pEditHandler->RemoveAll(true);
		pEditHandler->Release();
	}

	bool res = true;
	const std::vector<CState*> *pStates = CContextManager::Get()->GetAllStates();
	for (CState* pState : *pStates) {
		if( pState->GetRecursiveOperationHandler() ) {
			pState->GetRecursiveOperationHandler()->StopRecursiveOperation();
		}

		if (pState->m_pCommandQueue) {
			if (!pState->m_pCommandQueue->Quit())
				res = false;
		}
	}

	if (!res) {
		if( event.CanVeto() ) {
			event.Veto();
		}
		return;
	}

	CContextControl::_context_controls* controls = m_pContextControl ? m_pContextControl->GetCurrentControls() : 0;
	if (controls) {
		COptions::Get()->SetLastServer(controls->pState->GetLastServer());
		COptions::Get()->SetOption(OPTION_LASTSERVERPATH, controls->pState->GetLastServerPath().GetSafePath());
		COptions::Get()->SetOption(OPTION_LAST_CONNECTED_SITE, controls->site_bookmarks ? controls->site_bookmarks->path : wxString());
	}

	for (std::vector<CState*>::const_iterator iter = pStates->begin(); iter != pStates->end(); ++iter) {
		CState *pState = *iter;
		pState->DestroyEngine();
	}

	CSiteManager::ClearIdMap();

	if (controls) {
		controls->pLocalListView->SaveColumnSettings(OPTION_LOCALFILELIST_COLUMN_WIDTHS, OPTION_LOCALFILELIST_COLUMN_SHOWN, OPTION_LOCALFILELIST_COLUMN_ORDER);
		controls->pRemoteListView->SaveColumnSettings(OPTION_REMOTEFILELIST_COLUMN_WIDTHS, OPTION_REMOTEFILELIST_COLUMN_SHOWN, OPTION_REMOTEFILELIST_COLUMN_ORDER);
	}

	bool filters_toggled = CFilterManager::HasActiveFilters(true) && !CFilterManager::HasActiveFilters(false);
	COptions::Get()->SetOption(OPTION_FILTERTOGGLESTATE, filters_toggled ? 1 : 0);

	Destroy();
}

void CMainFrame::OnReconnect(wxCommandEvent &)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	if (pState->IsRemoteConnected() || !pState->IsRemoteIdle())
		return;

	CServer server = pState->GetLastServer();

	if (server.GetLogonType() == ASK)
	{
		if (!CLoginManager::Get().GetPassword(server, false))
			return;
	}

	CServerPath path = pState->GetLastServerPath();
	ConnectToServer(server, path, true);
}

void CMainFrame::OnRefresh(wxCommandEvent &)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	pState->RefreshRemote();
	pState->RefreshLocal();
}

void CMainFrame::OnTimer(wxTimerEvent& event)
{
	if (event.GetId() == m_closeEventTimer.GetId())
	{
		if (m_closeEvent == 0)
			return;

		// When we get idle event, a dialog's event loop has been left.
		// Now we can close the top level window on the stack.
		wxCloseEvent *evt = new wxCloseEvent(m_closeEvent);
		evt->SetCanVeto(false);
		QueueEvent(evt);
	}
#if FZ_MANUALUPDATECHECK
	else if( event.GetId() == update_dialog_timer_.GetId() ) {
		TriggerUpdateDialog();
	}
#endif
}

void CMainFrame::OpenSiteManager(const CServer* pServer /*=0*/)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	CSiteManagerDialog dlg;

	std::set<wxString> handled_paths;
	std::vector<CSiteManagerDialog::_connected_site> connected_sites;

	if (pServer)
	{
		CSiteManagerDialog::_connected_site connected_site;
		connected_site.server = *pServer;
		connected_sites.push_back(connected_site);
	}

	for (int i = 0; i < m_pContextControl->GetTabCount(); i++)
	{
		CContextControl::_context_controls *controls =  m_pContextControl->GetControlsFromTabIndex(i);
		if (!controls)
			continue;

		const wxString& path = controls->site_bookmarks->path;
		if (path.empty())
			continue;
		if (handled_paths.find(path) != handled_paths.end())
			continue;

		CSiteManagerDialog::_connected_site connected_site;
		connected_site.old_path = path;
		connected_site.server = controls->pState->GetLastServer();
		connected_sites.push_back(connected_site);
		handled_paths.insert(path);
	}

	if (!dlg.Create(this, &connected_sites, pServer))
		return;

	int res = dlg.ShowModal();
	if (res == wxID_YES || res == wxID_OK)
	{
		// Update bookmark paths
		for (size_t j = 0; j < connected_sites.size(); j++)
		{
			for (int i = 0; i < m_pContextControl->GetTabCount(); i++)
			{
				CContextControl::_context_controls *controls =  m_pContextControl->GetControlsFromTabIndex(i);
				if (!controls)
					continue;

				if (connected_sites[j].old_path != controls->site_bookmarks->path)
					continue;

				controls->site_bookmarks->path = connected_sites[j].new_path;

				controls->site_bookmarks->bookmarks.clear();
				CSiteManager::GetBookmarks(controls->site_bookmarks->path, controls->site_bookmarks->bookmarks);

				break;
			}
		}
	}

	if (res == wxID_YES)
	{
		CSiteManagerItemData_Site data;
		if (!dlg.GetServer(data))
			return;

		ConnectToSite(data);
	}

	if (m_pMenuBar)
		m_pMenuBar->UpdateBookmarkMenu();
}

void CMainFrame::OnSiteManager(wxCommandEvent& e)
{
#ifdef __WXMAC__
	if (wxGetKeyState(WXK_SHIFT) ||
		wxGetKeyState(WXK_ALT) ||
		wxGetKeyState(WXK_CONTROL))
	{
		OnSitemanagerDropdown(e);
		return;
	}
#endif
	OpenSiteManager();
}

void CMainFrame::UpdateActivityLed(int direction)
{
	if (m_pActivityLed[direction])
		m_pActivityLed[direction]->Ping();
}

void CMainFrame::OnProcessQueue(wxCommandEvent& event)
{
	if (m_pQueueView)
		m_pQueueView->SetActive(event.IsChecked());
}

void CMainFrame::OnMenuEditSettings(wxCommandEvent&)
{
	CSettingsDialog dlg(m_engineContext);
	if (!dlg.Create(this))
		return;

	COptions* pOptions = COptions::Get();

	wxString oldTheme = pOptions->GetOption(OPTION_THEME);
	wxString oldThemeSize = pOptions->GetOption(OPTION_THEME_ICONSIZE);
	wxString oldLang = pOptions->GetOption(OPTION_LANGUAGE);

	int oldShowDebugMenu = pOptions->GetOptionVal(OPTION_DEBUG_MENU) != 0;

	bool oldTimestamps = pOptions->GetOptionVal(OPTION_MESSAGELOG_TIMESTAMP) != 0;

	int res = dlg.ShowModal();
	if (res != wxID_OK)
	{
		UpdateLayout();
		return;
	}

	bool newTimestamps = pOptions->GetOptionVal(OPTION_MESSAGELOG_TIMESTAMP) != 0;

	wxString newTheme = pOptions->GetOption(OPTION_THEME);
	wxString newThemeSize = pOptions->GetOption(OPTION_THEME_ICONSIZE);
	wxString newLang = pOptions->GetOption(OPTION_LANGUAGE);

	if (oldTheme != newTheme ||
		oldThemeSize != newThemeSize ||
		oldLang != newLang)
	{
		CreateMainToolBar();
		if (m_pToolBar)
			m_pToolBar->UpdateToolbarState();
	}

	if (oldLang != newLang ||
		oldTimestamps != newTimestamps)
	{
		m_pStatusView->InitDefAttr();
	}

	if (oldLang != newLang ||
		oldShowDebugMenu != pOptions->GetOptionVal(OPTION_DEBUG_MENU))
	{
		CreateMenus();
	}
	if (oldLang != newLang) {
		wxMessageBoxEx(_("FileZilla needs to be restarted for the language change to take effect."), _("Language changed"), wxICON_INFORMATION, this);
	}

	CheckChangedSettings();
}

void CMainFrame::OnToggleLogView(wxCommandEvent&)
{
	if (!m_pTopSplitter)
		return;

	bool shown;

	if (COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_POSITION) == 1)
	{
		if (!m_pQueueLogSplitter)
			return;
		if (m_pQueueLogSplitter->IsSplit())
		{
			m_pQueueLogSplitter->Unsplit(m_pStatusView);
			shown = false;
		}
		else if (m_pStatusView->IsShown())
		{
			m_pStatusView->Hide();
			m_pBottomSplitter->Unsplit(m_pQueueLogSplitter);
			shown = false;
		}
		else if (!m_pQueueLogSplitter->IsShown())
		{
			m_pQueueLogSplitter->Initialize(m_pStatusView);
			m_pBottomSplitter->SplitHorizontally(m_pContextControl, m_pQueueLogSplitter);
			shown = true;
		}
		else
		{
			m_pQueueLogSplitter->SplitVertically(m_pQueuePane, m_pStatusView);
			shown = true;
		}
	}
	else
	{
		if (m_pTopSplitter->IsSplit())
		{
			m_pTopSplitter->Unsplit(m_pStatusView);
			shown = false;
		}
		else
		{
			m_pTopSplitter->SplitHorizontally(m_pStatusView, m_pBottomSplitter);
			shown = true;
		}
	}

	if (COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_POSITION) != 2)
		COptions::Get()->SetOption(OPTION_SHOW_MESSAGELOG, shown);
}

void CMainFrame::OnToggleDirectoryTreeView(wxCommandEvent& event)
{
	if (!m_pContextControl)
		return;

	CContextControl::_context_controls* controls = m_pContextControl->GetCurrentControls();
	if (!controls)
		return;

	bool const local = event.GetId() == XRCID("ID_TOOLBAR_LOCALTREEVIEW") || event.GetId() == XRCID("ID_VIEW_LOCALTREE");
	CSplitterWindowEx* splitter = local ? controls->pLocalSplitter : controls->pRemoteSplitter;
	bool show = !splitter->IsSplit();
	ShowDirectoryTree(local, show);
}

void CMainFrame::ShowDirectoryTree(bool local, bool show)
{
	if (!m_pContextControl)
		return;

	const int layout = COptions::Get()->GetOptionVal(OPTION_FILEPANE_LAYOUT);
	const int swap = COptions::Get()->GetOptionVal(OPTION_FILEPANE_SWAP);
	for (int i = 0; i < m_pContextControl->GetTabCount(); ++i) {
		CContextControl::_context_controls* controls = m_pContextControl->GetControlsFromTabIndex(i);
		if (!controls)
			continue;

		CSplitterWindowEx* splitter = local ? controls->pLocalSplitter : controls->pRemoteSplitter;
		CView* tree = local ? controls->pLocalTreeViewPanel : controls->pRemoteTreeViewPanel;
		CView* list = local ? controls->pLocalListViewPanel : controls->pRemoteListViewPanel;

		if (show && !splitter->IsSplit()) {
			tree->SetHeader(list->DetachHeader());

			if (layout == 3 && swap)
				splitter->SplitVertically(list, tree);
			else if (layout)
				splitter->SplitVertically(tree, list);
			else
				splitter->SplitHorizontally(tree, list);
		}
		else if (!show && splitter->IsSplit()) {
			list->SetHeader(tree->DetachHeader());
			splitter->Unsplit(tree);
		}
	}

	COptions::Get()->SetOption(local ? OPTION_SHOW_TREE_LOCAL : OPTION_SHOW_TREE_REMOTE, show);
}

void CMainFrame::OnToggleQueueView(wxCommandEvent& event)
{
	if (!m_pBottomSplitter)
		return;

	bool shown;
	if (COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_POSITION) == 1)
	{
		if (!m_pQueueLogSplitter)
			return;
		if (m_pQueueLogSplitter->IsSplit())
		{
			m_pQueueLogSplitter->Unsplit(m_pQueuePane);
			shown = false;
		}
		else if (m_pQueuePane->IsShown())
		{
			m_pQueuePane->Hide();
			m_pBottomSplitter->Unsplit(m_pQueueLogSplitter);
			shown = false;
		}
		else if (!m_pQueueLogSplitter->IsShown())
		{
			m_pQueueLogSplitter->Initialize(m_pQueuePane);
			m_pBottomSplitter->SplitHorizontally(m_pContextControl, m_pQueueLogSplitter);
			shown = true;
		}
		else
		{
			m_pQueueLogSplitter->SplitVertically(m_pQueuePane, m_pStatusView);
			shown = true;
		}
	}
	else
	{
		if (m_pBottomSplitter->IsSplit())
			m_pBottomSplitter->Unsplit(m_pQueueLogSplitter);
		else
		{
			m_pQueueLogSplitter->Initialize(m_pQueuePane);
			m_pBottomSplitter->SplitHorizontally(m_pContextControl, m_pQueueLogSplitter);
		}
		shown = m_pBottomSplitter->IsSplit();
	}

	COptions::Get()->SetOption(OPTION_SHOW_QUEUE, shown);
}

void CMainFrame::OnMenuHelpAbout(wxCommandEvent&)
{
	CAboutDialog dlg;
	if (!dlg.Create(this))
		return;

	dlg.ShowModal();
}

void CMainFrame::OnFilter(wxCommandEvent& event)
{
	if (wxGetKeyState(WXK_SHIFT)) {
		OnFilterRightclicked(event);
		return;
	}

	bool const oldActive = CFilterManager::HasActiveFilters();

	CFilterDialog dlg;
	dlg.Create(this);
	dlg.ShowModal();

	if (oldActive == CFilterManager::HasActiveFilters() && m_pToolBar) {
		// Restore state
		m_pToolBar->ToggleTool(XRCID("ID_TOOLBAR_FILTER"), oldActive);
	}
}

#if FZ_MANUALUPDATECHECK
void CMainFrame::OnCheckForUpdates(wxCommandEvent& event)
{
	if( !m_pUpdater ) {
		return;
	}

	update_dialog_timer_.Stop();
	CUpdateDialog dlg( this, *m_pUpdater );
	dlg.ShowModal();
	update_dialog_timer_.Stop();
}

void CMainFrame::UpdaterStateChanged( UpdaterState s, build const& v )
{
#if FZ_AUTOUPDATECHECK
	if( !m_pMenuBar ) {
		return;
	}

	if( s == UpdaterState::idle ) {
		wxMenu* m = 0;
		wxMenuItem* pItem = m_pMenuBar->FindItem(GetAvailableUpdateMenuId(), &m);
		if( pItem && m ) {
			for( size_t i = 0; i != m_pMenuBar->GetMenuCount(); ++i ) {
				if( m_pMenuBar->GetMenu(i) == m ) {
					m_pMenuBar->Remove(i);
					delete m;
					break;
				}
			}
		}
		return;
	}
	else if( s != UpdaterState::newversion && s != UpdaterState::newversion_ready ) {
		return;
	}
	else if( v.version_.empty() ) {
		return;
	}

	wxString const name = wxString::Format(_("&Version %s"), v.version_);

	wxMenuItem* pItem = m_pMenuBar->FindItem(GetAvailableUpdateMenuId());
	if( !pItem ) {
		wxMenu* pMenu = new wxMenu();
		pMenu->Append(GetAvailableUpdateMenuId(), name);
		m_pMenuBar->Append(pMenu, _("&New version available!"));

		if( !update_dialog_timer_.IsRunning() ) {
			update_dialog_timer_.Start(1, true);
		}
	}
	else {
		pItem->SetItemLabel(name);
	}
#endif
}

void CMainFrame::TriggerUpdateDialog()
{
	if( CUpdateDialog::IsRunning() ) {
		return;
	}

	if( !wxDialogEx::CanShowPopupDialog() ) {
		update_dialog_timer_.Start( 1000, true );
		return;
	}

	CUpdateDialog dlg(this, *m_pUpdater);
	dlg.ShowModal();

	// In case the timer was started while the dialog was up.
	update_dialog_timer_.Stop();
}
#endif

void CMainFrame::UpdateLayout(int layout /*=-1*/, int swap /*=-1*/, int messagelog_position /*=-1*/)
{
	if (layout == -1)
		layout = COptions::Get()->GetOptionVal(OPTION_FILEPANE_LAYOUT);
	if (swap == -1)
		swap = COptions::Get()->GetOptionVal(OPTION_FILEPANE_SWAP);

	if (messagelog_position == -1)
		messagelog_position = COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_POSITION);

	// First handle changes in message log position as it can make size of the other panes change
	{
		bool shown = m_pStatusView->IsShown();
		wxWindow* parent = m_pStatusView->GetParent();

		bool changed;
		if (parent == m_pTopSplitter && messagelog_position != 0)
		{
			if (shown)
				m_pTopSplitter->Unsplit(m_pStatusView);
			changed = true;
		}
		else if (parent == m_pQueueLogSplitter && messagelog_position != 1)
		{
			if (shown)
			{
				if (m_pQueueLogSplitter->IsSplit())
					m_pQueueLogSplitter->Unsplit(m_pStatusView);
				else
					m_pBottomSplitter->Unsplit(m_pQueueLogSplitter);
			}
			changed = true;
		}
		else if (parent != m_pTopSplitter && parent != m_pQueueLogSplitter && messagelog_position != 2)
		{
			m_pQueuePane->RemovePage(3);
			changed = true;
			shown = true;
		}
		else
			changed = false;

		if (changed)
		{
			switch (messagelog_position)
			{
			default:
				m_pStatusView->Reparent(m_pTopSplitter);
				if (shown)
					m_pTopSplitter->SplitHorizontally(m_pStatusView, m_pBottomSplitter);
				break;
			case 1:
				m_pStatusView->Reparent(m_pQueueLogSplitter);
				if (shown)
				{
					if (m_pQueueLogSplitter->IsShown())
						m_pQueueLogSplitter->SplitVertically(m_pQueuePane, m_pStatusView);
					else
					{
						m_pQueueLogSplitter->Initialize(m_pStatusView);
						m_pBottomSplitter->SplitHorizontally(m_pContextControl, m_pQueueLogSplitter);
					}
				}
				break;
			case 2:
				m_pQueuePane->AddPage(m_pStatusView, _("Message log"));
				break;
			}
		}
	}

	// Now the other panes
	for (int i = 0; i < m_pContextControl->GetTabCount(); i++)
	{
		struct CContextControl::_context_controls *controls = m_pContextControl->GetControlsFromTabIndex(i);
		if (!controls)
			continue;

		int mode;
		if (!layout || layout == 2 || layout == 3)
			mode = wxSPLIT_VERTICAL;
		else
			mode = wxSPLIT_HORIZONTAL;

		int isMode = controls->pViewSplitter->GetSplitMode();

		int isSwap = controls->pViewSplitter->GetWindow1() == controls->pRemoteSplitter ? 1 : 0;

		if (mode != isMode || swap != isSwap)
		{
			controls->pViewSplitter->Unsplit();
			if (mode == wxSPLIT_VERTICAL)
			{
				if (swap)
					controls->pViewSplitter->SplitVertically(controls->pRemoteSplitter, controls->pLocalSplitter);
				else
					controls->pViewSplitter->SplitVertically(controls->pLocalSplitter, controls->pRemoteSplitter);
			}
			else
			{
				if (swap)
					controls->pViewSplitter->SplitHorizontally(controls->pRemoteSplitter, controls->pLocalSplitter);
				else
					controls->pViewSplitter->SplitHorizontally(controls->pLocalSplitter, controls->pRemoteSplitter);
			}
		}

		if (controls->pLocalSplitter->IsSplit())
		{
			if (!layout)
				mode = wxSPLIT_HORIZONTAL;
			else
				mode = wxSPLIT_VERTICAL;

			wxWindow* pFirst;
			wxWindow* pSecond;
			if (layout == 3 && swap)
			{
				pFirst = controls->pLocalListViewPanel;
				pSecond = controls->pLocalTreeViewPanel;
			}
			else
			{
				pFirst = controls->pLocalTreeViewPanel;
				pSecond = controls->pLocalListViewPanel;
			}

			if (mode != controls->pLocalSplitter->GetSplitMode() || pFirst != controls->pLocalSplitter->GetWindow1())
			{
				controls->pLocalSplitter->Unsplit();
				if (mode == wxSPLIT_VERTICAL)
					controls->pLocalSplitter->SplitVertically(pFirst, pSecond);
				else
					controls->pLocalSplitter->SplitHorizontally(pFirst, pSecond);
			}
		}

		if (controls->pRemoteSplitter->IsSplit())
		{
			if (!layout)
				mode = wxSPLIT_HORIZONTAL;
			else
				mode = wxSPLIT_VERTICAL;

			wxWindow* pFirst;
			wxWindow* pSecond;
			if (layout == 3 && !swap)
			{
				pFirst = controls->pRemoteListViewPanel;
				pSecond = controls->pRemoteTreeViewPanel;
			}
			else
			{
				pFirst = controls->pRemoteTreeViewPanel;
				pSecond = controls->pRemoteListViewPanel;
			}

			if (mode != controls->pRemoteSplitter->GetSplitMode() || pFirst != controls->pRemoteSplitter->GetWindow1())
			{
				controls->pRemoteSplitter->Unsplit();
				if (mode == wxSPLIT_VERTICAL)
					controls->pRemoteSplitter->SplitVertically(pFirst, pSecond);
				else
					controls->pRemoteSplitter->SplitHorizontally(pFirst, pSecond);
			}
		}

		if (layout == 3)
		{
			if (!swap)
			{
				controls->pRemoteSplitter->SetSashGravity(1.0);
				controls->pLocalSplitter->SetSashGravity(0.0);
			}
			else
			{
				controls->pLocalSplitter->SetSashGravity(1.0);
				controls->pRemoteSplitter->SetSashGravity(0.0);
			}
		}
		else
		{
			controls->pLocalSplitter->SetSashGravity(0.0);
			controls->pRemoteSplitter->SetSashGravity(0.0);
		}
	}
}

void CMainFrame::OnSitemanagerDropdown(wxCommandEvent& event)
{
	if (!m_pToolBar)
		return;

	std::unique_ptr<wxMenu> pMenu = CSiteManager::GetSitesMenu();
	if (pMenu) {
		ShowDropdownMenu(pMenu.release(), m_pToolBar, event);
	}
}

bool CMainFrame::ConnectToSite(CSiteManagerItemData_Site & data, bool newTab)
{
	if (data.m_server.GetLogonType() == ASK ||
		(data.m_server.GetLogonType() == INTERACTIVE && data.m_server.GetUser().empty()))
	{
		if (!CLoginManager::Get().GetPassword(data.m_server, false, data.m_server.GetName()))
			return false;
	}

	if (newTab)
		m_pContextControl->CreateTab();

	if (!ConnectToServer(data.m_server, data.m_remoteDir))
		return false;

	if (!data.m_localDir.empty()) {
		CState *pState = CContextManager::Get()->GetCurrentContext();
		if( pState ) {
			bool set = pState->SetLocalDir(data.m_localDir, 0, false);

			if (set && data.m_sync) {
				wxASSERT(!data.m_remoteDir.empty());
				pState->SetSyncBrowse(true, data.m_remoteDir);
			}
		}
	}

	SetBookmarksFromPath(data.m_path);
	if (m_pMenuBar)
		m_pMenuBar->UpdateBookmarkMenu();

	return true;
}

void CMainFrame::CheckChangedSettings()
{
	UpdateLayout();

	m_pAsyncRequestQueue->RecheckDefaults();

	CAutoAsciiFiles::SettingsChanged();

#if FZ_MANUALUPDATECHECK
	m_pUpdater->Init();
#endif
}

void CMainFrame::ConnectNavigationHandler(wxEvtHandler* handler)
{
	if (!handler)
		return;

	handler->Connect(wxEVT_NAVIGATION_KEY, wxNavigationKeyEventHandler(CMainFrame::OnNavigationKeyEvent), 0, this);
}

void CMainFrame::OnNavigationKeyEvent(wxNavigationKeyEvent& event)
{
	if (wxGetKeyState(WXK_CONTROL) && event.IsFromTab()) {
		if (m_pContextControl)
			m_pContextControl->AdvanceTab(event.GetDirection());
		return;
	}

	event.Skip();
}

void CMainFrame::OnChar(wxKeyEvent& event)
{
	if (event.GetKeyCode() != WXK_F6) {
		event.Skip();
		return;
	}

	// Jump between quickconnect bar and view headers

	std::list<wxWindow*> windowOrder;
	if (m_pQuickconnectBar)
		windowOrder.push_back(m_pQuickconnectBar);
	CContextControl::_context_controls* controls = m_pContextControl->GetCurrentControls();
	if (controls) {
		windowOrder.push_back(controls->pLocalViewHeader);
		windowOrder.push_back(controls->pRemoteViewHeader);
	}

	wxWindow* focused = FindFocus();

	bool skipFirst = false;
	std::list<wxWindow*>::iterator iter;
	if (!focused) {
		iter = windowOrder.begin();
		skipFirst = false;
	}
	else {
		wxWindow *parent = focused->GetParent();
		for (iter = windowOrder.begin(); iter != windowOrder.end(); ++iter) {
			if (*iter == focused || *iter == parent) {
				skipFirst = true;
				break;
			}
		}
		if (iter == windowOrder.end()) {
			iter = windowOrder.begin();
			skipFirst = false;
		}
	}

	FocusNextEnabled(windowOrder, iter, skipFirst, !event.ShiftDown());
}

void CMainFrame::FocusNextEnabled(std::list<wxWindow*>& windowOrder, std::list<wxWindow*>::iterator iter, bool skipFirst, bool forward)
{
	std::list<wxWindow*>::iterator start = iter;

	while (skipFirst || !(*iter)->IsShownOnScreen() || !(*iter)->IsEnabled()) {
		skipFirst = false;
		if (forward) {
			++iter;
			if (iter == windowOrder.end())
				iter = windowOrder.begin();
		}
		else {
			if (iter == windowOrder.begin())
				iter = windowOrder.end();
			--iter;
		}

		if (iter == start) {
			wxBell();
			return;
		}
	}

	(*iter)->SetFocus();
}

void CMainFrame::RememberSplitterPositions()
{
	CContextControl::_context_controls* controls = m_pContextControl ? m_pContextControl->GetCurrentControls() : 0;
	if (!controls)
		return;

	wxString posString;

	// top_pos
	posString += wxString::Format(_T("%d "), m_pTopSplitter->GetSashPosition());

	// bottom_height
	posString += wxString::Format(_T("%d "), m_pBottomSplitter->GetSashPosition());

	// view_pos
	posString += wxString::Format(_T("%d "), (int)(controls->pViewSplitter->GetRelativeSashPosition() * 1000000000));

	// local_pos
	posString += wxString::Format(_T("%d "), controls->pLocalSplitter->GetSashPosition());

	// remote_pos
	posString += wxString::Format(_T("%d "), controls->pRemoteSplitter->GetSashPosition());

	// queuelog splitter
	// Note that we cannot use %f, it is locale-dependent
	// m_lastQueueLogSplitterPos is a value between 0 and 1
	posString += wxString::Format(_T("%d"), (int)(m_pQueueLogSplitter->GetRelativeSashPosition() * 1000000000));

	COptions::Get()->SetOption(OPTION_MAINWINDOW_SPLITTER_POSITION, posString);
}

bool CMainFrame::RestoreSplitterPositions()
{
	if (wxGetKeyState(WXK_SHIFT) && wxGetKeyState(WXK_ALT) && wxGetKeyState(WXK_CONTROL))
		return false;

	// top_pos bottom_height view_pos view_height_width local_pos remote_pos
	wxString posString = COptions::Get()->GetOption(OPTION_MAINWINDOW_SPLITTER_POSITION);
	wxStringTokenizer tokens(posString, _T(" "));
	int count = tokens.CountTokens();
	if (count < 6)
		return false;

	long * aPosValues = new long[count];
	for (int i = 0; i < count; ++i) {
		wxString token = tokens.GetNextToken();
		if (!token.ToLong(aPosValues + i)) {
			delete [] aPosValues;
			return false;
		}
	}

	m_pTopSplitter->SetSashPosition(aPosValues[0]);

	m_pBottomSplitter->SetSashPosition(aPosValues[1]);

	CContextControl::_context_controls* controls = m_pContextControl ? m_pContextControl->GetCurrentControls() : 0;
	if (!controls) {
		delete [] aPosValues;
		return false;
	}

	double pos = (double)aPosValues[2] / 1000000000;
	if (pos >= 0 && pos <= 1)
		controls->pViewSplitter->SetRelativeSashPosition(pos);

	controls->pLocalSplitter->SetSashPosition(aPosValues[3]);
	controls->pRemoteSplitter->SetSashPosition(aPosValues[4]);

	pos = (double)aPosValues[5] / 1000000000;
	if (pos >= 0 && pos <= 1)
		m_pQueueLogSplitter->SetRelativeSashPosition(pos);
	delete [] aPosValues;

	return true;
}

void CMainFrame::SetDefaultSplitterPositions()
{
	m_pTopSplitter->SetSashPosition(97);

	wxSize size = m_pBottomSplitter->GetClientSize();
	int h = size.GetHeight() - 135;
	if (h < 50)
		h = 50;
	m_pBottomSplitter->SetSashPosition(h);

	m_pQueueLogSplitter->SetSashPosition(0);

	CContextControl::_context_controls* controls = m_pContextControl ? m_pContextControl->GetCurrentControls() : 0;
	if (controls) {
		controls->pViewSplitter->SetSashPosition(0);
		controls->pLocalSplitter->SetRelativeSashPosition(0.4);
		controls->pRemoteSplitter->SetRelativeSashPosition(0.4);
	}
}

void CMainFrame::OnActivate(wxActivateEvent& event)
{
	// According to the wx docs we should do this
	event.Skip();

	if (!event.GetActive())
		return;

#ifdef __WXMAC__
	// wxMac looses focus information if the window becomes inactive.
	// Restore focus to the previously focused child, otherwise focus ends up
	// in the quickconnect bar.
	// Go via ID of the last focused child to avoid issues with window lifetime.
	if (m_lastFocusedChild != -1)
		m_winLastFocused = FindWindow(m_lastFocusedChild);
#endif

	CEditHandler* pEditHandler = CEditHandler::Get();
	if (pEditHandler)
		pEditHandler->CheckForModifications(true);

	if (m_pAsyncRequestQueue)
		m_pAsyncRequestQueue->TriggerProcessing();
}

void CMainFrame::OnToolbarComparison(wxCommandEvent& event)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	CComparisonManager* pComparisonManager = pState->GetComparisonManager();
	if (pComparisonManager->IsComparing())
	{
		pComparisonManager->ExitComparisonMode();
		return;
	}

	if (!COptions::Get()->GetOptionVal(OPTION_FILEPANE_LAYOUT)) {
		CContextControl::_context_controls* controls = m_pContextControl->GetCurrentControls();
		if (!controls)
			return;

		if ((controls->pLocalSplitter->IsSplit() && !controls->pRemoteSplitter->IsSplit()) ||
			(!controls->pLocalSplitter->IsSplit() && controls->pRemoteSplitter->IsSplit()))
		{
			CConditionalDialog dlg(this, CConditionalDialog::compare_treeviewmismatch, CConditionalDialog::yesno);
			dlg.SetTitle(_("Directory comparison"));
			dlg.AddText(_("To compare directories, both file lists have to be aligned."));
			dlg.AddText(_("To do this, the directory trees need to be both shown or both hidden."));
			dlg.AddText(_("Show both directory trees and continue comparing?"));
			if (!dlg.Run()) {
				// Needed to restore non-toggle state of button
				pState->NotifyHandlers(STATECHANGE_COMPARISON);
				return;
			}

			ShowDirectoryTree(true, true);
			ShowDirectoryTree(false, true);
		}

		int pos = (controls->pLocalSplitter->GetSashPosition() + controls->pRemoteSplitter->GetSashPosition()) / 2;
		controls->pLocalSplitter->SetSashPosition(pos);
		controls->pRemoteSplitter->SetSashPosition(pos);
	}

	pComparisonManager->CompareListings();
}

void CMainFrame::OnToolbarComparisonDropdown(wxCommandEvent& event)
{
	if (!m_pToolBar)
		return;

	wxMenu* pMenu = wxXmlResource::Get()->LoadMenu(_T("ID_MENU_TOOLBAR_COMPARISON_DROPDOWN"));
	if (!pMenu)
		return;

	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	CComparisonManager* pComparisonManager = pState->GetComparisonManager();
	pMenu->FindItem(XRCID("ID_TOOLBAR_COMPARISON"))->Check(pComparisonManager->IsComparing());

	const int mode = COptions::Get()->GetOptionVal(OPTION_COMPARISONMODE);
	if (mode == 0)
		pMenu->FindItem(XRCID("ID_COMPARE_SIZE"))->Check();
	else
		pMenu->FindItem(XRCID("ID_COMPARE_DATE"))->Check();

	pMenu->Check(XRCID("ID_COMPARE_HIDEIDENTICAL"), COptions::Get()->GetOptionVal(OPTION_COMPARE_HIDEIDENTICAL) != 0);

	ShowDropdownMenu(pMenu, m_pToolBar, event);
}

void CMainFrame::ShowDropdownMenu(wxMenu* pMenu, wxToolBar* pToolBar, wxCommandEvent& event)
{
#ifdef EVT_TOOL_DROPDOWN
	if (event.GetEventType() == wxEVT_COMMAND_TOOL_DROPDOWN_CLICKED)
	{
		pToolBar->SetDropdownMenu(event.GetId(), pMenu);
		event.Skip();
	}
	else
#endif
	{
#ifdef __WXMSW__
		RECT r;
		if (::SendMessage((HWND)pToolBar->GetHandle(), TB_GETITEMRECT, pToolBar->GetToolPos(event.GetId()), (LPARAM)&r))
			pToolBar->PopupMenu(pMenu, r.left, r.bottom);
		else
#endif
			pToolBar->PopupMenu(pMenu);

		delete pMenu;
	}
}

void CMainFrame::OnDropdownComparisonMode(wxCommandEvent& event)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	int old_mode = COptions::Get()->GetOptionVal(OPTION_COMPARISONMODE);
	int new_mode = (event.GetId() == XRCID("ID_COMPARE_SIZE")) ? 0 : 1;
	COptions::Get()->SetOption(OPTION_COMPARISONMODE, new_mode);

	CComparisonManager* pComparisonManager = pState->GetComparisonManager();
	if (old_mode != new_mode && pComparisonManager && pComparisonManager->IsComparing())
		pComparisonManager->CompareListings();
}

void CMainFrame::OnDropdownComparisonHide(wxCommandEvent& event)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	bool old_mode = COptions::Get()->GetOptionVal(OPTION_COMPARE_HIDEIDENTICAL) != 0;
	COptions::Get()->SetOption(OPTION_COMPARE_HIDEIDENTICAL, old_mode ? 0 : 1);

	CComparisonManager* pComparisonManager = pState->GetComparisonManager();
	if (pComparisonManager && pComparisonManager->IsComparing())
		pComparisonManager->CompareListings();
}

void CMainFrame::ProcessCommandLine()
{
	const CCommandLine* pCommandLine = wxGetApp().GetCommandLine();
	if (!pCommandLine)
		return;

	wxString local;
	if ((local = pCommandLine->GetOption(CCommandLine::local)) != _T(""))
	{

		if (!wxDir::Exists(local))
		{
			wxString str = _("Path not found:");
			str += _T("\n") + local;
			wxMessageBoxEx(str, _("Syntax error in command line"));
			return;
		}

		CState *pState = CContextManager::Get()->GetCurrentContext();
		if (!pState)
			return;

		pState->SetLocalDir(local);
	}

	wxString site;
	if (pCommandLine->HasSwitch(CCommandLine::sitemanager)) {
		if (COptions::Get()->GetOptionVal(OPTION_INTERFACE_SITEMANAGER_ON_STARTUP) == 0) {
			Show();
			OpenSiteManager();
		}
	}
	else if ((site = pCommandLine->GetOption(CCommandLine::site)) != _T("")) {
		std::unique_ptr<CSiteManagerItemData_Site> pData = CSiteManager::GetSiteByPath(site);

		if (pData) {
			ConnectToSite(*pData);
		}
	}

	wxString param = pCommandLine->GetParameter();
	if (!param.empty()) {
		wxString error;

		CServer server;

		wxString logontype = pCommandLine->GetOption(CCommandLine::logontype);
		if (logontype == _T("ask"))
			server.SetLogonType(ASK);
		else if (logontype == _T("interactive"))
			server.SetLogonType(INTERACTIVE);

		CServerPath path;
		if (!server.ParseUrl(param, 0, _T(""), _T(""), error, path))
		{
			wxString str = _("Parameter not a valid URL");
			str += _T("\n") + error;
			wxMessageBoxEx(error, _("Syntax error in command line"));
		}

		if (server.GetLogonType() == ASK ||
			(server.GetLogonType() == INTERACTIVE && server.GetUser().empty()))
		{
			if (!CLoginManager::Get().GetPassword(server, false))
				return;
		}

		CState* pState = CContextManager::Get()->GetCurrentContext();
		if (!pState)
			return;

		ConnectToServer(server, path);
	}
}

void CMainFrame::OnFilterRightclicked(wxCommandEvent& event)
{
	const bool active = CFilterManager::HasActiveFilters();

	CFilterManager::ToggleFilters();

	if (active == CFilterManager::HasActiveFilters())
		return;

	CContextManager::Get()->NotifyAllHandlers(STATECHANGE_APPLYFILTER);
}

#ifdef __WXMSW__
WXLRESULT CMainFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	if (nMsg == WM_DEVICECHANGE)
	{
		// Let tree control handle device change message
		// They get sent by Windows on adding or removing drive
		// letters

		if (!m_pContextControl)
			return 0;

		for (int i = 0; i < m_pContextControl->GetTabCount(); i++)
		{
			CContextControl::_context_controls* controls = m_pContextControl->GetControlsFromTabIndex(i);
			if (controls && controls->pLocalTreeView)
				controls->pLocalTreeView->OnDevicechange(wParam, lParam);
		}
		return 0;
	}
	else if (nMsg == WM_DISPLAYCHANGE)
	{
		// wxDisplay caches the display configuration and does not
		// reset it if the display configuration changes.
		// wxDisplay uses this strange factory design pattern and uses a wxModule
		// to delete the factory on program shutdown.
		//
		// To reset the factory manually in response to WM_DISPLAYCHANGE,
		// create another instance of the module and call its Exit() member.
		// After that, the next call to a wxDisplay will create a new factory and
		// get the new display layout from Windows.
		//
		// Note: Both the factory pattern as well as the dynamic object system
		//	   are perfect example of bad design.
		//
		wxModule* pDisplayModule = (wxModule*)wxCreateDynamicObject(_T("wxDisplayModule"));
		if (pDisplayModule)
		{
			pDisplayModule->Exit();
			delete pDisplayModule;
		}
	}
	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}
#endif

void CMainFrame::OnSyncBrowse(wxCommandEvent& event)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	pState->SetSyncBrowse(!pState->GetSyncBrowse());
}

#ifndef __WXMAC__
void CMainFrame::OnIconize(wxIconizeEvent& event)
{
#ifdef __WXGTK__
	if (m_taskbar_is_uniconizing)
		return;
	if (m_taskBarIcon && m_taskBarIcon->IsIconInstalled()) // Only way to uniconize is via the taskbar icon.
		return;
#endif
	if (!event.IsIconized())
	{
		if (m_taskBarIcon)
			m_taskBarIcon->RemoveIcon();
		Show(true);

		if (m_pAsyncRequestQueue)
			m_pAsyncRequestQueue->TriggerProcessing();

		return;
	}

	if (!COptions::Get()->GetOptionVal(OPTION_MINIMIZE_TRAY))
		return;

	if (!m_taskBarIcon)
	{
		m_taskBarIcon = new wxTaskBarIcon();
		m_taskBarIcon->Connect(wxEVT_TASKBAR_LEFT_DCLICK, wxTaskBarIconEventHandler(CMainFrame::OnTaskBarClick), 0, this);
		m_taskBarIcon->Connect(wxEVT_TASKBAR_LEFT_UP, wxTaskBarIconEventHandler(CMainFrame::OnTaskBarClick), 0, this);
		m_taskBarIcon->Connect(wxEVT_TASKBAR_RIGHT_UP, wxTaskBarIconEventHandler(CMainFrame::OnTaskBarClick), 0, this);
	}

	bool installed;
	if (!m_taskBarIcon->IsIconInstalled())
		installed = m_taskBarIcon->SetIcon(CThemeProvider::GetIcon(_T("ART_FILEZILLA")), GetTitle());
	else
		installed = true;

	if (installed)
		Show(false);
}

void CMainFrame::OnTaskBarClick(wxTaskBarIconEvent&)
{
#ifdef __WXGTK__
	if (m_taskbar_is_uniconizing)
		return;
	m_taskbar_is_uniconizing = true;
#endif

	if (m_taskBarIcon)
		m_taskBarIcon->RemoveIcon();

	Show(true);
	Iconize(false);

	if (m_pAsyncRequestQueue)
		m_pAsyncRequestQueue->TriggerProcessing();

#ifdef __WXGTK__
	QueueEvent(new wxCommandEvent(fzEVT_TASKBAR_CLICK_DELAYED));
#endif
}

#ifdef __WXGTK__
void CMainFrame::OnTaskBarClick_Delayed(wxCommandEvent& event)
{
	m_taskbar_is_uniconizing = false;
}
#endif

#endif

void CMainFrame::OnSearch(wxCommandEvent& event)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return;

	CSearchDialog dlg(this, pState, m_pQueueView);
	if (!dlg.Load())
		return;

	dlg.Run();
}

void CMainFrame::PostInitialize()
{
#ifdef __WXMAC__
	// Focus first control
	NavigateIn(wxNavigationKeyEvent::IsForward);
#endif

#if FZ_MANUALUPDATECHECK
	// Need to do this after welcome screen to avoid simultaneous display of multiple dialogs
	if( !m_pUpdater ) {
		update_dialog_timer_.SetOwner(this);
		m_pUpdater = new CUpdater(*this, m_engineContext);
		m_pUpdater->Init();
	}
#endif

	if (COptions::Get()->GetOptionVal(OPTION_INTERFACE_SITEMANAGER_ON_STARTUP) != 0)
	{
		Show();
		OpenSiteManager();
	}
}

void CMainFrame::OnMenuNewTab(wxCommandEvent& event)
{
	if (m_pContextControl)
		m_pContextControl->CreateTab();
}

void CMainFrame::OnMenuCloseTab(wxCommandEvent&)
{
	if (!m_pContextControl)
		return;

	m_pContextControl->CloseTab(m_pContextControl->GetCurrentTab());
}

void CMainFrame::SetBookmarksFromPath(const wxString& path)
{
	if (!m_pContextControl)
		return;

	std::shared_ptr<CContextControl::_context_controls::_site_bookmarks> site_bookmarks;
	for (int i = 0; i < m_pContextControl->GetTabCount(); i++) {
		if (i == m_pContextControl->GetCurrentTab())
			continue;

		CContextControl::_context_controls *controls = m_pContextControl->GetControlsFromTabIndex(i);
		if (!controls || !controls->site_bookmarks || controls->site_bookmarks->path != path)
			continue;

		site_bookmarks = controls->site_bookmarks;
		site_bookmarks->bookmarks.clear();
	}
	if (!site_bookmarks) {
		site_bookmarks = std::make_shared<CContextControl::_context_controls::_site_bookmarks>();
		site_bookmarks->path = path;
	}

	CContextControl::_context_controls *controls = m_pContextControl->GetCurrentControls();
	if (controls) {
		controls->site_bookmarks = site_bookmarks;
		CSiteManager::GetBookmarks(controls->site_bookmarks->path, controls->site_bookmarks->bookmarks);
	}
}

bool CMainFrame::ConnectToServer(const CServer &server, const CServerPath &path /*=CServerPath()*/, bool isReconnect /*=true*/)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState)
		return false;

	if (pState->IsRemoteConnected() || !pState->IsRemoteIdle()) {
		int action = COptions::Get()->GetOptionVal(OPTION_ALREADYCONNECTED_CHOICE);
		if( action < 2 ) {
			wxDialogEx dlg;
			if (!dlg.Load(this, _T("ID_ALREADYCONNECTED")))
				return false;

			if (action != 0)
				XRCCTRL(dlg, "ID_OLDTAB", wxRadioButton)->SetValue(true);
			else
				XRCCTRL(dlg, "ID_NEWTAB", wxRadioButton)->SetValue(true);

			if (dlg.ShowModal() != wxID_OK)
				return false;

			if (XRCCTRL(dlg, "ID_NEWTAB", wxRadioButton)->GetValue()) {
				action = 0;
			}
			else {
				action = 1;
			}

			if( XRCCTRL(dlg, "ID_REMEMBER", wxCheckBox)->IsChecked() ) {
				action |= 2;
			}
			COptions::Get()->SetOption(OPTION_ALREADYCONNECTED_CHOICE, action);
		}

		if( !(action & 1) ) {
			m_pContextControl->CreateTab();
			pState = CContextManager::Get()->GetCurrentContext();
		}
	}

	CContextControl::_context_controls* controls = m_pContextControl->GetControlsFromState(pState);
	if (!isReconnect && controls) {
		controls->site_bookmarks.reset();
		m_pMenuBar->ClearBookmarks();
	}

	return pState->Connect(server, path);
}

void CMainFrame::OnToggleToolBar(wxCommandEvent& event)
{
	COptions::Get()->SetOption(OPTION_TOOLBAR_HIDDEN, event.IsChecked() ? 0 : 1);
#ifdef __WXMAC__
	if (m_pToolBar)
		m_pToolBar->Show( event.IsChecked() );
#else
	CreateMainToolBar();
	if (m_pToolBar)
		m_pToolBar->UpdateToolbarState();
	HandleResize();
#endif
}

void CMainFrame::FixTabOrder()
{
	if (m_pQuickconnectBar && m_pTopSplitter) {
		m_pQuickconnectBar->MoveBeforeInTabOrder(m_pTopSplitter);
	}
}

#ifdef __WXMAC__
void CMainFrame::OnChildFocused(wxChildFocusEvent& event)
{
	m_lastFocusedChild = event.GetWindow()->GetId();
}
#endif
