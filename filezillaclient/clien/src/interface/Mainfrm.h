#ifndef __MAINFRM_H__
#define __MAINFRM_H__

#include "statusbar.h"
#include "engine_context.h"

#include <wx/timer.h>

#ifndef __WXMAC__
#include <wx/taskbar.h>
#endif

#if FZ_MANUALUPDATECHECK
#include "updater.h"
#endif

class CAsyncRequestQueue;
class CContextControl;
class CLed;
class CMainFrameStateEventHandler;
class CMenuBar;
class CQueue;
class CQueueView;
class CQuickconnectBar;
class CSiteManagerItemData_Site;
class CSplitterWindowEx;
class CStatusView;
class CState;
class CThemeProvider;
class CToolBar;
class CWindowStateManager;

class CMainFrame final : public wxNavigationEnabled<wxFrame>
#if FZ_MANUALUPDATECHECK
	, protected CUpdateHandler
#endif
{
	friend class CMainFrameStateEventHandler;
public:
	CMainFrame();
	virtual ~CMainFrame();

	void UpdateActivityLed(int direction);

	CStatusView* GetStatusView() { return m_pStatusView; }
	CQueueView* GetQueue() { return m_pQueueView; }
	CQuickconnectBar* GetQuickconnectBar() { return m_pQuickconnectBar; }

	void UpdateLayout(int layout = -1, int swap = -1, int messagelog_position = -1);

	// Window size and position as well as pane sizes
	void RememberSplitterPositions();
	bool RestoreSplitterPositions();
	void SetDefaultSplitterPositions();

	void CheckChangedSettings();

	void ConnectNavigationHandler(wxEvtHandler* handler);

	wxStatusBar* GetStatusBar() const { return m_pStatusBar; }

	void ProcessCommandLine();

	void PostInitialize();

	bool ConnectToServer(const CServer& server, const CServerPath& path = CServerPath(), bool isReconnect = false);

	CContextControl* GetContextControl() { return m_pContextControl; }

	bool ConnectToSite(CSiteManagerItemData_Site & data, bool newTab = false);

	CFileZillaEngineContext& GetEngineContext() { return m_engineContext; }
protected:
	void FixTabOrder();

	bool CloseDialogsAndQuit(wxCloseEvent &event);
	bool CreateMenus();
	bool CreateQuickconnectBar();
	bool CreateMainToolBar();
	void OpenSiteManager(const CServer* pServer = 0);

	void FocusNextEnabled(std::list<wxWindow*>& windowOrder, std::list<wxWindow*>::iterator iter, bool skipFirst, bool forward);

	void SetBookmarksFromPath(const wxString& path);

	CStatusBar* m_pStatusBar{};
	CMenuBar* m_pMenuBar{};
	CToolBar* m_pToolBar{};
	CQuickconnectBar* m_pQuickconnectBar{};

	CSplitterWindowEx* m_pTopSplitter{}; // If log position is 0, splits message log from rest of panes
	CSplitterWindowEx* m_pBottomSplitter{}; // Top contains view splitter, bottom queue (or queuelog splitter if in position 1)
	CSplitterWindowEx* m_pQueueLogSplitter{};

	CFileZillaEngineContext m_engineContext;
	CContextControl* m_pContextControl{};

	CStatusView* m_pStatusView{};
	CQueueView* m_pQueueView{};
	CLed* m_pActivityLed[2];
	CThemeProvider* m_pThemeProvider{};
#if FZ_MANUALUPDATECHECK
	CUpdater* m_pUpdater{};
	virtual void UpdaterStateChanged( UpdaterState s, build const& v );
	void TriggerUpdateDialog();
	wxTimer update_dialog_timer_;
#endif

	void ShowDirectoryTree(bool local, bool show);

	void ShowDropdownMenu(wxMenu* pMenu, wxToolBar* pToolBar, wxCommandEvent& event);

#ifdef __WXMSW__
	virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif

	void HandleResize();

	// Event handlers
	DECLARE_EVENT_TABLE()
	void OnSize(wxSizeEvent& event);
	void OnMenuHandler(wxCommandEvent& event);
	void OnEngineEvent(wxFzEvent& event);
	void OnUpdateLedTooltip(wxCommandEvent&);
	void OnDisconnect(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	void OnClose(wxCloseEvent& event);
	void OnReconnect(wxCommandEvent&);
	void OnRefresh(wxCommandEvent&);
	void OnTimer(wxTimerEvent& event);
	void OnSiteManager(wxCommandEvent&);
	void OnProcessQueue(wxCommandEvent& event);
	void OnMenuEditSettings(wxCommandEvent&);
	void OnToggleToolBar(wxCommandEvent& event);
	void OnToggleLogView(wxCommandEvent&);
	void OnToggleDirectoryTreeView(wxCommandEvent& event);
	void OnToggleQueueView(wxCommandEvent& event);
	void OnMenuHelpAbout(wxCommandEvent&);
	void OnFilter(wxCommandEvent& event);
	void OnFilterRightclicked(wxCommandEvent& event);
#if FZ_MANUALUPDATECHECK
	void OnCheckForUpdates(wxCommandEvent& event);
#endif //FZ_MANUALUPDATECHECK
	void OnSitemanagerDropdown(wxCommandEvent& event);
	void OnNavigationKeyEvent(wxNavigationKeyEvent& event);
	void OnChar(wxKeyEvent& event);
	void OnActivate(wxActivateEvent& event);
	void OnToolbarComparison(wxCommandEvent& event);
	void OnToolbarComparisonDropdown(wxCommandEvent& event);
	void OnDropdownComparisonMode(wxCommandEvent& event);
	void OnDropdownComparisonHide(wxCommandEvent& event);
	void OnSyncBrowse(wxCommandEvent& event);
#ifdef __WXMAC__
	void OnChildFocused(wxChildFocusEvent& event);
#else
	void OnIconize(wxIconizeEvent& event);
	void OnTaskBarClick(wxTaskBarIconEvent&);
#endif
#ifdef __WXGTK__
	void OnTaskBarClick_Delayed(wxCommandEvent& event);
#endif
	void OnSearch(wxCommandEvent& event);
	void OnMenuNewTab(wxCommandEvent& event);
	void OnMenuCloseTab(wxCommandEvent& event);

	bool m_bInitDone{};
	bool m_bQuit{};
	wxEventType m_closeEvent{};
	wxTimer m_closeEventTimer;

	CAsyncRequestQueue* m_pAsyncRequestQueue{};
	CMainFrameStateEventHandler* m_pStateEventHandler{};

	CWindowStateManager* m_pWindowStateManager{};

	CQueue* m_pQueuePane{};

#ifndef __WXMAC__
	wxTaskBarIcon* m_taskBarIcon{};
#endif
#ifdef __WXGTK__
	// There is a bug in KDE, causing the window to toggle iconized state
	// several times a second after uniconizing it from taskbar icon.
	// Set m_taskbar_is_uniconizing in OnTaskBarClick and unset the
	// next time the pending event processing runs and calls OnTaskBarClick_Delayed.
	// While set, ignore iconize events.
	bool m_taskbar_is_uniconizing{};
#endif

	int m_comparisonToggleAcceleratorId{};

#ifdef __WXMAC__
	int m_lastFocusedChild{-1};
#endif
};

#endif
