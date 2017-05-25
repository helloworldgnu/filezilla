// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2016 - Tim Kosse <tim.kosse@filezilla-project.org>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// MainFrm.cpp : Implementierung der Klasse CMainFrame
//

#include "stdafx.h"
#include "FileZilla server.h"
#include "misc/led.h"
#include "MainFrm.h"
#include "../iputils.h"
#include "../platform.h"
#include "statusview.h"
#include "usersdlg.h"
#include "GroupsDlg.h"
//by zhangyl
#include "Options.h"
#include "usersview.h"
#include "userslistctrl.h"
#include "misc/systemtray.h"
#include "offlineaskdlg.h"
#include "../version.h"
//by zhangyl
#include "../AdminSocket.h"
//#include "AdminSocket.h"
#include "OptionsDlg.h"
#include "ConnectDialog.h"
#include "mainfrm.h"
#include "../defs.h"
#include "OutputFormat.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_COMMAND(ID_EDIT_SETTINGS, OnEditSettings)
	ON_COMMAND(ID_ACTIVE, OnActive)
	ON_UPDATE_COMMAND_UI(ID_ACTIVE, OnUpdateActive)
	ON_WM_SYSCOMMAND()
	ON_COMMAND(ID_TRAY_EXIT, OnTrayExit)
	ON_COMMAND(ID_TRAY_RESTORE, OnTrayRestore)
	ON_COMMAND(ID_LOCK, OnLock)
	ON_UPDATE_COMMAND_UI(ID_LOCK, OnUpdateLock)
	ON_WM_TIMER()
	ON_COMMAND(ID_USERS, OnMenuEditUsers)
	ON_COMMAND(ID_GROUPS, OnMenuEditGroups)
	ON_COMMAND(ID_FILE_CONNECT, OnFileConnect)
	ON_COMMAND(ID_FILE_DISCONNECT, OnFileDisconnect)
	ON_UPDATE_COMMAND_UI(ID_FILE_DISCONNECT, OnUpdateFileDisconnect)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETTINGS, OnUpdateEditSettings)
	ON_UPDATE_COMMAND_UI(ID_MENU_EDIT_USERS, OnUpdateMenuEditUsers)
	ON_UPDATE_COMMAND_UI(ID_MENU_EDIT_GROUPS, OnUpdateMenuEditGroups)
	ON_UPDATE_COMMAND_UI(ID_USERS, OnUpdateUsers)
	ON_UPDATE_COMMAND_UI(ID_GROUPS, OnUpdateGroups)
	ON_COMMAND(ID_MENU_EDIT_USERS, OnMenuEditUsers)
	ON_COMMAND(ID_MENU_EDIT_GROUPS, OnMenuEditGroups)
	ON_COMMAND(ID_USERLISTTOOLBAR_DISPLAYLOGICAL, OnDisplayLogicalNames)
	ON_COMMAND(ID_USERLISTTOOLBAR_DISPLAYPHYSICAL, OnDisplayPhysicalNames)
	ON_UPDATE_COMMAND_UI(ID_USERLISTTOOLBAR_DISPLAYLOGICAL, OnUpdateDisplayLogicalNames)
	ON_UPDATE_COMMAND_UI(ID_USERLISTTOOLBAR_DISPLAYPHYSICAL, OnUpdateDisplayPhysicalNames)
	ON_COMMAND(ID_USERLISTTOOLBAR_SORT, OnDisplaySortMenu)
	ON_NOTIFY(TBN_DROPDOWN, AFX_IDW_TOOLBAR, OnToolbarDropDown)
	ON_COMMAND(ID_DISPLAY_SORTBYUSERID, OnDisplaySortByUserid)
	ON_COMMAND(ID_DISPLAY_SORTBYACCOUNT, OnDisplaySortByAccount)
	ON_COMMAND(ID_DISPLAY_SORTBYIP, OnDisplaySortByIP)
	ON_UPDATE_COMMAND_UI(ID_DISPLAY_SORTBYUSERID, OnUpdateDisplaySortByUserid)
	ON_UPDATE_COMMAND_UI(ID_DISPLAY_SORTBYACCOUNT, OnUpdateDisplaySortByAccount)
	ON_UPDATE_COMMAND_UI(ID_DISPLAY_SORTBYIP, OnUpdateDisplaySortByIP)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,		   // Statusleistenanzeige
	ID_INDICATOR_RECVCOUNT,
	ID_INDICATOR_RECVRATE,
	ID_INDICATOR_SENDCOUNT,
	ID_INDICATOR_SENDRATE,
	ID_INDICATOR_RECVLED,
	ID_INDICATOR_SENDLED,
	ID_SEPARATOR
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame Konstruktion/Zerstörung

CMainFrame::CMainFrame(COptions *pOptions)
{
	ASSERT(pOptions);
	s_winClassName = 0;
	nTrayNotificationMsg_ = RegisterWindowMessage(_T("FileZilla Server Tray Notification Message"));
	m_nSendCount = 0;
	m_nRecvCount = 0;
	m_lastchecktime = GetTickCount();
	m_lastreaddiff = 0;
	m_lastwritediff = 0;
	m_nOldRecvCount = 0;
	m_nOldSendCount = 0;
	m_pOptions = pOptions;
	m_pAdminSocket = NULL;
	m_nEdit = 0;

	m_pOptionsDlg = 0;
	m_pUsersDlg = 0;
	m_pGroupsDlg = 0;

	m_nReconnectTimerID = 0;
	m_nReconnectCount = 0;
}

CMainFrame::~CMainFrame()
{
	delete m_pOptions;
	m_pOptions = 0;
	CloseAdminSocket(false);
	delete [] s_winClassName; //Does seem to crash
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetupTrayIcon();

	if (!m_wndReBar.Create(this))
		return -1;

	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP
		| CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Could not create Toolbar 1\n");
		return -1;	  // Fehler bei Erstellung
	}

	if (!m_wndUserListToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP | CBRS_ALIGN_LEFT
		| CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndUserListToolBar.LoadToolBar(IDR_USERLISTTOOLBAR))
	{
		TRACE0("Could not create Toolbar 2\n");
		return -1;	  // Fehler bei Erstellung
	}
	m_wndUserListToolBar.GetToolBarCtrl().SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);
	DWORD dwStyle = m_wndUserListToolBar.GetButtonStyle(m_wndUserListToolBar.CommandToIndex(ID_USERLISTTOOLBAR_SORT));
	dwStyle |= TBSTYLE_DROPDOWN;
	m_wndUserListToolBar.SetButtonStyle(m_wndUserListToolBar.CommandToIndex(ID_USERLISTTOOLBAR_SORT), dwStyle);

	m_wndReBar.AddBar(&m_wndToolBar);
	m_wndReBar.AddBar(&m_wndUserListToolBar);
	m_wndReBar.GetReBarCtrl().MinimizeBand(0);

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Statusleiste konnte nicht erstellt werden\n");
		return -1;	  // Fehler bei Erstellung
	}

	// The last statusbar pane is a fake one, it has zero width.
	m_wndStatusBar.SetPaneInfo(7, 0, SBPS_NOBORDERS, 0);

	CRect rect;
	m_wndStatusBar.GetItemRect(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVLED), rect);

	//Create the first LED control
	m_RecvLed.Create(_T(""), WS_VISIBLE|WS_CHILD, rect, &m_wndStatusBar, m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVLED));
	m_RecvLed.SetLed( CLed::LED_COLOR_GREEN, CLed::LED_OFF, CLed::LED_ROUND);

	//Create the second LED control
	m_SendLed.Create(_T(""), WS_VISIBLE|WS_CHILD, rect, &m_wndStatusBar, m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDLED));
	m_SendLed.SetLed( CLed::LED_COLOR_RED, CLed::LED_OFF, CLed::LED_ROUND);

	m_wndStatusBar.GetItemRect(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDLED), &rect);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVLED),ID_INDICATOR_RECVLED,SBPS_NOBORDERS,6);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDLED),ID_INDICATOR_SENDLED,SBPS_NOBORDERS,6);

	//ShowStatus(GetProductVersionString(), 0);
	ShowStatus(_T("Copyright 2001-2016 by Tim Kosse (tim.kosse@filezilla-project.org)"), 0);
	ShowStatus(_T("https://filezilla-project.org/"), 0);

	m_nTimerID = SetTimer(7777, 10000, 0);
	m_nRateTimerID = SetTimer(7778, 1000, 0);

	SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDCOUNT), _T("0 bytes sent"));
	SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVCOUNT), _T("0 bytes received"));
	SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVRATE), _T("0 B/s"));
	SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDRATE), _T("0 B/s"));


	CConnectDialog dlg(m_pOptions);
	//if (!m_pOptions->GetOptionVal(IOPTION_ALWAYS)) {
	//	if (!m_pOptions->GetOptionVal(IOPTION_STARTMINIMIZED)) {
	//		ShowWindow(SW_SHOW);
	//		if (dlg.DoModal() == IDOK) {
	//			DoConnect();
	//		}
	//	}
	//}
	//else {
	//	DoConnect();
	//}

	return 0;
}

//////////////////
// Helper function to register a new window class based on an already
// existing window class, but with a different name and icon.
// Returns new name if successful; otherwise NULL.
//
static bool RegisterSimilarClass(LPCTSTR lpszNewClassName,
	LPCTSTR lpszOldClassName, UINT nIDResource)
{
	// Get class info for old class.
	//
	HINSTANCE hInst = AfxGetInstanceHandle();
	WNDCLASS wc;
	if (!::GetClassInfo(hInst, lpszOldClassName, &wc)) {
		TRACE("Can't find window class %s\n", lpszOldClassName);
		return false;
	}

	// Register new class with same info, but different name and icon.
	//
	wc.lpszClassName = lpszNewClassName;
	wc.hIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(nIDResource));
	if (!AfxRegisterClass(&wc)) {
		TRACE("Unable to register window class%s\n", lpszNewClassName);
		return false;
	}
	return true;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;

	// ZU ERLEDIGEN: Ändern Sie hier die Fensterklasse oder das Erscheinungsbild, indem Sie
	//  CREATESTRUCT cs modifizieren.

	cs.lpszClass = AfxRegisterWndClass(0);

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	//Change the window class name
	if (!s_winClassName)
	{
		s_winClassName = new TCHAR[_tcslen(_T("FileZilla Server Main Window")) + 1];
		_tcscpy(s_winClassName, _T("FileZilla Server Main Window"));
		RegisterSimilarClass(s_winClassName, cs.lpszClass, IDR_MAINFRAME);
	}
	cs.lpszClass = s_winClassName;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame Nachrichten-Handler
void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
	// Fokus an das Ansichtfenster weitergeben
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// andernfalls die Standardbehandlung durchführen
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	CRect rect;
	GetClientRect(rect);

	// Unterteiltes Fenster erstellen
	if (!m_wndSplitter.CreateStatic(this, 2, 1))
		return FALSE;
	if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CStatusView), CSize(1, rect.Height() - 150), pContext))
	{
		m_wndSplitter.DestroyWindow();
		return FALSE;
	}
	m_pStatusPane = (CStatusView*)m_wndSplitter.GetPane(0, 0);

	if (!m_wndSplitter.CreateView(1, 0, RUNTIME_CLASS(CUsersView), CSize(1, 150), pContext))
	{
		m_wndSplitter.DestroyWindow();
		return FALSE;
	}
	m_pUsersPane = (CUsersView*) m_wndSplitter.GetPane(1, 0);

	// Filename display option.
	//GetUsersPane()->m_pListCtrl->SetDisplayPhysicalNames(m_pOptions->GetOptionVal(IOPTION_FILENAMEDISPLAY) != 0);

	// Set layout options.
	int sortInfo = /*(int)m_pOptions->GetOptionVal(IOPTION_USERSORTING)*/0;
	GetUsersPane()->m_pListCtrl->SetSortColumn(sortInfo & 0x0F, sortInfo >> 4);

	return CFrameWnd::OnCreateClient(lpcs, pContext);
}

CStatusView* CMainFrame::GetStatusPane()
{
	return m_pStatusPane;
}

CUsersView* CMainFrame::GetUsersPane()
{
	return m_pUsersPane;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	if (m_wndStatusBar.GetSafeHwnd())
	{
		if (nType!=SIZE_MAXIMIZED)
			m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDLED),ID_INDICATOR_SENDLED,SBPS_NOBORDERS,0);
		else
			m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDLED),ID_INDICATOR_SENDLED,SBPS_NOBORDERS,10);
	}

	if (m_wndSplitter.GetSafeHwnd())
	{
		//Hide the queue if visible
		m_wndSplitter.HideRow(1, 0);
	}
	//Now only the main splitter gets resized
	CFrameWnd::OnSize(nType, cx, cy);
	if (m_wndSplitter.GetSafeHwnd())
	{
		//Restore the queue
		m_wndSplitter.ShowRow(1);
	}

	if (m_wndStatusBar.GetSafeHwnd())
	{
		RECT rc;

		m_wndStatusBar.GetItemRect(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVLED), &rc);

		// Reposition the first LED correctly!
		m_RecvLed.SetWindowPos(&wndTop, rc.left, rc.top+1, rc.right - rc.left,
			rc.bottom - rc.top, 0);

		m_wndStatusBar.GetItemRect(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDLED), &rc);

		// Reposition the second LED correctly!
		m_SendLed.SetWindowPos(&wndTop, rc.left, rc.top+1, rc.right - rc.left,
				rc.bottom - rc.top, 0);
	}
}

void CMainFrame::OnClose()
{
	CFrameWnd::OnClose();
}

void CMainFrame::OnEditSettings()
{
	if (m_nEdit)
	{
		MessageBeep(MB_OK);
		return;
	}
	m_nEdit |= 0x04;
	SendCommand(5, 0, 0);
	ShowStatus(_T("Retrieving settings, please wait..."), 0);
}

void CMainFrame::OnActive()
{
	if (m_nServerState & STATE_ONLINE && !(m_nServerState & STATE_MASK_GOOFFLINE)) {
		if (!GetUsersPane()->m_pListCtrl->GetItemCount()) {
			if (AfxMessageBox(_T("Do you really want to take the server offline?"), MB_YESNO | MB_ICONQUESTION) != IDYES)
				return;
			int nServerState = m_nServerState | STATE_GOOFFLINE_NOW;
			unsigned char buffer[2];
			buffer[0] = nServerState / 256;
			buffer[1] = nServerState % 256;
			SendCommand(2, buffer, 2);
			ShowStatus(_T("Server is going offline..."), 0);
			return;
		}
		else {
			COfflineAskDlg dlg;
			if (dlg.DoModal() != IDOK)
				return;
			if (dlg.m_nRadio == 2) {
				int nServerState = m_nServerState | STATE_GOOFFLINE_WAITTRANSFER;
				unsigned char buffer[2];
				buffer[0] = nServerState / 256;
				buffer[1] = nServerState % 256;
				SendCommand(2, buffer, 2);
				ShowStatus(_T("Server is going offline when all transfers have finished..."), 0);
				return;
			}
			if (dlg.m_nRadio == 1) {
				int nServerState = m_nServerState | STATE_GOOFFLINE_LOGOUT;
				unsigned char buffer[2];
				buffer[0] = nServerState / 256;
				buffer[1] = nServerState % 256;
				SendCommand(2, buffer, 2);
				ShowStatus(_T("Server is going offline when all users have logged out..."), 0);
				return;
			}
			else {
				int nServerState = m_nServerState | STATE_GOOFFLINE_NOW;
				unsigned char buffer[2];
				buffer[0] = nServerState / 256;
				buffer[1] = nServerState % 256;
				SendCommand(2, buffer, 2);
				ShowStatus(_T("Server is going offline now..."), 0);
				return;
			}
		}
	}
	else {
		int nServerState = STATE_ONLINE | (m_nServerState & STATE_LOCKED);
		unsigned char buffer[2];
		buffer[0] = nServerState / 256;
		buffer[1] = nServerState % 256;
		SendCommand(2, buffer, 2);
	}
}

void CMainFrame::OnUpdateActive(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected());
	pCmdUI->SetCheck(m_nServerState & STATE_ONLINE && !(m_nServerState & STATE_MASK_GOOFFLINE));
}

void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam)
{
	CFrameWnd::OnSysCommand(nID, lParam);
	if (nID == SC_MINIMIZE)
		ShowWindow(SW_HIDE);
	else if (nID == SC_RESTORE)
		ShowWindow(SW_SHOW);
}

//// SetupTrayIcon /////////////////////////////////////////////////////
// If we're minimized, create an icon in the systray.  Otherwise, remove
// the icon, if one is present.

void CMainFrame::SetupTrayIcon()
{
	m_TrayIcon.Create(0, nTrayNotificationMsg_, _T("FileZilla Server"),
		0, IDR_SYSTRAY_MENU);
	m_TrayIcon.SetIcon(IDI_UNKNOWN);
}


//// SetupTaskBarButton ////////////////////////////////////////////////
// Show or hide the taskbar button for this app, depending on whether
// we're minimized right now or not.

void CMainFrame::OnTrayExit()
{
	if (!m_bQuit)
		OnClose();
}

void CMainFrame::OnTrayRestore()
{
	ShowWindow(SW_RESTORE);
	ShowWindow(SW_SHOW);
}

void CMainFrame::SetIcon()
{
	if (!m_pAdminSocket /*|| !m_pAdminSocket->IsConnected()*/) {
		m_TrayIcon.StopAnimation();
		m_TrayIcon.SetIcon(IDI_UNKNOWN);
	}
	else if (!(m_nServerState & STATE_ONLINE) || m_nServerState & STATE_MASK_GOOFFLINE) {
		if (!GetUsersPane()->m_pListCtrl->GetItemCount()) {
			m_TrayIcon.StopAnimation();
			m_TrayIcon.SetIcon(IDI_RED);
		}
		else {
			m_TrayIcon.SetIconList(IDI_GREEN, IDI_RED);
			m_TrayIcon.Animate(500);
		}
	}
	else if (m_nServerState & STATE_LOCKED) {
		if (GetUsersPane()->m_pListCtrl->GetItemCount()) {
			m_TrayIcon.SetIconList(IDI_GREEN, IDI_YELLOW);
			m_TrayIcon.Animate(300);
		}
		else {
			m_TrayIcon.SetIconList(IDI_YELLOW, IDI_RED);
			m_TrayIcon.Animate(500);
		}

	}
	else {
		m_TrayIcon.StopAnimation();
		m_TrayIcon.SetIcon(GetUsersPane()->m_pListCtrl->GetItemCount()?IDI_GREEN:IDI_YELLOW);
	}

}

void CMainFrame::OnLock()
{
	if (!(m_nServerState & STATE_ONLINE) || m_nServerState & STATE_MASK_GOOFFLINE)
		return;
	if (m_nServerState & STATE_LOCKED) {
		int nServerState = m_nServerState & ~STATE_LOCKED;
		unsigned char buffer[2];
		buffer[0] = nServerState / 256;
		buffer[1] = nServerState % 256;
		SendCommand(2, buffer, 2);
	}
	else {
		if (AfxMessageBox(_T("Do you really want to lock the server? No new connections will be accepted while locked."), MB_YESNO|MB_ICONQUESTION) != IDYES)
			return;
		int nServerState = m_nServerState | STATE_LOCKED;
		unsigned char buffer[2];
		buffer[0] = nServerState / 256;
		buffer[1] = nServerState % 256;
		SendCommand(2, buffer, 2);
		ShowStatus("Server locked", 0);
	}
}

void CMainFrame::OnUpdateLock(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected() && m_nServerState & STATE_ONLINE && !(m_nServerState & STATE_MASK_GOOFFLINE));
	pCmdUI->SetCheck((m_nServerState & STATE_LOCKED) ? 1 : 0);

}

CString FormatSpeed(__int64 diff, const int span)
{
	diff = (__int64)((double)diff * 1000 / span);

	CString str = _T("");
	CString digit;

	int shift = 0;
	while (diff >= 10000)
	{
		diff /= 10;
		shift++;
	}

	while (diff)
	{
		digit = (TCHAR)('0' + static_cast<TCHAR>(diff % 10));
		str = digit + str;
		diff /= 10;
	}

	if (str == _T(""))
		str = _T("0");

	if (shift % 3)
		str = str.Left((shift % 3) + 1) + "," + str.Right(str.GetLength() - (shift % 3) - 1);
	shift += 2;
	shift /= 3;
	if (!shift)
		str += _T(" B/s");
	else if (shift == 1)
		str += _T(" KB/s");
	else if (shift == 2)
		str += _T(" MB/s");
	else if (shift == 3)
		str += _T(" GB/s");
	else if (shift == 4)
		str += _T(" TB/s");
	else
		str = _T("n/a"); //If this happens, you really have a fast connection
	return str;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	if (!nIDEvent)
		return;
	else if (nIDEvent == m_nTimerID)
	{
		SendCommand(8);
		m_TrayIcon.RefreshIcon();
	}
	else if (nIDEvent == m_nRateTimerID)
	{
		const int span = GetTickCount() - m_lastchecktime;
		m_lastchecktime=GetTickCount();

		__int64 diff = m_nSendCount - m_nOldSendCount;
		m_nOldSendCount = m_nSendCount;

		if (m_lastwritediff && diff)
		{
			__int64 tmp = diff;
			diff = (diff + m_lastwritediff) / 2;
			m_lastwritediff = tmp;
		}
		else
			m_lastwritediff = diff;

		CString writeSpeed = FormatSpeed(diff, span);
		SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDRATE), writeSpeed);

		diff = m_nRecvCount - m_nOldRecvCount;
		m_nOldRecvCount = m_nRecvCount;

		if (m_lastreaddiff && diff)
		{
			__int64 tmp = diff;
			diff = (diff + m_lastreaddiff) / 2;
			m_lastreaddiff = tmp;
		}
		else
			m_lastreaddiff = diff;

		CString readSpeed = FormatSpeed(diff, span);
		SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVRATE), readSpeed);
	}
	else if (nIDEvent == m_nReconnectTimerID)
	{
		KillTimer(m_nReconnectTimerID);
		m_nReconnectTimerID = 0;

		if (m_pAdminSocket)
			return;
		DoConnect();
	}

	CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::SetStatusbarText(int nIndex,CString str)
{
	m_wndStatusBar.GetStatusBarCtrl().SetText(str,nIndex,0);
	HFONT hFont = (HFONT)m_wndStatusBar.SendMessage(WM_GETFONT);
	if (str=="")
	{
		str.LoadString(m_wndStatusBar.GetItemID(nIndex));
	}
	CClientDC dcScreen(NULL);
	HGDIOBJ hOldFont = NULL;
	if (hFont != NULL)
	hOldFont = dcScreen.SelectObject(hFont);
	int cx=dcScreen.GetTextExtent(str).cx;
	int cxold;
	unsigned int nID,nStyle;
	m_wndStatusBar.GetPaneInfo(nIndex,nID,nStyle,cxold);
	if (cx!=cxold)
	{
		if (cx<cxold)
			cx=cxold;
		m_wndStatusBar.SetPaneInfo(nIndex,nID,nStyle,cx);

	}
	if (hOldFont != NULL)
		dcScreen.SelectObject(hOldFont);

}

void CMainFrame::OnMenuEditUsers()
{
	if (m_nEdit)
	{
		MessageBeep(MB_OK);
		return;
	}
	m_nEdit |= 1;
	SendCommand(6, 0, 0);
	ShowStatus(_T("Retrieving account settings, please wait..."), 0);
}

void CMainFrame::OnMenuEditGroups()
{
	if (m_nEdit)
	{
		MessageBeep(MB_OK);
		return;
	}
	m_nEdit |= 2;
	SendCommand(6, 0, 0);
	ShowStatus(_T("Retrieving account settings, please wait..."), 0);
}

void CMainFrame::ShowStatus(const CString& status, int nType)
{
	CStatusView *view = GetStatusPane();
	view->ShowStatus(status, nType);
}

void CMainFrame::ShowStatusRaw(const char *status, int nType)
{
	//CString msg(ConvFromNetwork(status));
	//ShowStatus(msg, nType);
}

void CMainFrame::ParseReply(int nReplyID, unsigned char *pData, int nDataLength)
{
	switch(nReplyID)
	{
	case 0:
		{
			ShowStatus(_T("Logged on"), 0);
			SendCommand(2);
			unsigned char buffer = USERCONTROL_GETLIST;
			SendCommand(3, &buffer, 1);
		}
		break;
	case 1:
		{
			char *pBuffer = new char[nDataLength];
			memcpy(pBuffer, pData+1, nDataLength-1);
			pBuffer[nDataLength-1] = 0;
			ShowStatusRaw(pBuffer, *pData);
			delete [] pBuffer;
		}
		break;
	case 2:
		m_nServerState = *pData*256 + pData[1];
		SetIcon();
		break;
	case 3:
		{
			if (nDataLength<2)
			{
				ShowStatus(_T("Protocol error: Unexpected data length"), 1);
				return;
			}
			else if (!GetUsersPane()->m_pListCtrl->ParseUserControlCommand(pData, nDataLength))
				ShowStatus(_T("Protocol error: Invalid data"), 1);
		}
		break;
	case 5:
		if (nDataLength == 1)
		{
			if (*pData == 0)
				ShowStatus(_T("Done sending settings."), 0);
			else if (*pData == 1)
				ShowStatus(_T("Could not change settings"), 1);
			break;
		}
		ShowStatus(_T("Done retrieving settings"), 0);
		if (nDataLength<2)
			ShowStatus(_T("Protocol error: Unexpected data length"), 1);
		else
		{
			if ((m_nEdit & 0x1C) == 0x04)
			{
				//m_pOptionsDlg = new COptionsDlg(m_pOptions, m_pAdminSocket->IsLocal());
				m_nEdit |= 0x08;
				if (!m_pOptionsDlg->Init(pData, nDataLength))
				{
					ShowStatus(_T("Protocol error: Invalid data"), 1);
					delete m_pOptionsDlg;
					m_pOptionsDlg = 0;
					m_nEdit = 0;
				}
				else if (!PostMessage(WM_APP))
				{
					ShowStatus(_T("Can't send window message"), 1);
					delete m_pOptionsDlg;
					m_pOptionsDlg = 0;
					m_nEdit = 0;
				}
			}
		}
		break;
	case 6:
		if (nDataLength == 1)
		{
			if (*pData == 0)
				ShowStatus(_T("Done sending account settings."), 0);
			else if (*pData == 1)
				ShowStatus(_T("Could not change account settings"), 1);
			break;
		}
		ShowStatus(_T("Done retrieving account settings"), 0);
		if (nDataLength<2)
			ShowStatus(_T("Protocol error: Unexpected data length"), 1);
		else
		{
			if ((m_nEdit & 0x19) == 0x01)
			{
				//m_pUsersDlg = new CUsersDlg(this, m_pAdminSocket->IsLocal());
				m_nEdit |= 0x08;
				if (!m_pUsersDlg->Init(pData, nDataLength))
				{
					ShowStatus(_T("Protocol error: Invalid data"), 1);
					delete m_pUsersDlg;
					m_pUsersDlg = 0;
					m_nEdit = 0;
					break;
				}
				else if (!PostMessage(WM_APP))
				{
					ShowStatus(_T("Can't send window message"), 1);
					delete m_pUsersDlg;
					m_pUsersDlg = 0;
					m_nEdit = 0;
				}
			}
			if ((m_nEdit & 0x1A) == 0x02)
			{
				//m_pGroupsDlg = new CGroupsDlg(this, m_pAdminSocket->IsLocal());
				m_nEdit |= 0x08;
				if (!m_pGroupsDlg->Init(pData, nDataLength))
				{
					ShowStatus(_T("Protocol error: Invalid data"), 1);
					delete m_pGroupsDlg;
					m_pGroupsDlg = 0;
					m_nEdit = 0;
					break;
				}
				else if (!PostMessage(WM_APP))
				{
					ShowStatus(_T("Can't send window message"), 1);
					delete m_pGroupsDlg;
					m_pGroupsDlg = 0;
					m_nEdit = 0;
				}
			}
		}
		break;
	case 8:
		break;
	default:
		{
			CString str;
			str.Format(_T("Protocol error: Unexpected reply id (%d)."), nReplyID);
			ShowStatus(str, 1);
			break;
		}
	}
}

void CMainFrame::ParseStatus(int nStatusID, unsigned char *pData, int nDataLength)
{
	switch(nStatusID)
	{
	case 1:
		{
			char *pBuffer = new char[nDataLength];
			memcpy(pBuffer, pData+1, nDataLength-1);
			pBuffer[nDataLength-1] = 0;
			ShowStatusRaw(pBuffer, *pData);
			delete [] pBuffer;
		}
		break;
	case 2:
		m_nServerState = *pData*256 + pData[1];
		SetIcon();
		break;
	case 3:
		{
			if (nDataLength<2)
			{
				ShowStatus(_T("Protocol error: Unexpected data length"), 1);
				return;
			}
			else if (!GetUsersPane()->m_pListCtrl->ParseUserControlCommand(pData, nDataLength))
				ShowStatus(_T("Protocol error: Invalid data"), 1);
		}
		break;
	case 4:
		{
			if (nDataLength < 10)
			{
				ShowStatus(_T("Protocol error: Unexpected data length"), 1);
				return;
			}

			CString msg;

			char *buffer = new char[nDataLength - 9 + 1];
			unsigned char *p = pData + 9;
			char *q = buffer;
			int pos = 0;
			while ((pos + 9) < nDataLength)
			{
				if (*p == '-')
				{
					*q = 0;
					/*msg = ConvFromNetwork(buffer);*/
					q = buffer;

					DWORD timeHigh = GET32(pData + 1);
					DWORD timeLow = GET32(pData + 5);

					FILETIME fFileTime;
					fFileTime.dwHighDateTime = timeHigh;
					fFileTime.dwLowDateTime = timeLow;

					SYSTEMTIME sFileTime;
					FileTimeToSystemTime(&fFileTime, &sFileTime);

					TCHAR datetime[200];
					int res = GetDateFormat(
							LOCALE_USER_DEFAULT,	// locale for which date is to be formatted
							DATE_SHORTDATE,			// flags specifying function options
							&sFileTime,				// date to be formatted
							0,						// date format string
							datetime,				// buffer for storing formatted string
							200						// size of buffer
						);

					if (res)
					{
						msg += datetime;
						msg += ' ';
					}

					res = GetTimeFormat(
							LOCALE_USER_DEFAULT,	// locale for which date is to be formatted
							TIME_FORCE24HOURFORMAT,	// flags specifying function options
							&sFileTime,				// date to be formatted
							0,						// date format string
							datetime,				// buffer for storing formatted string
							200						// size of buffer
						);

					if (res)
					{
						msg += datetime;
						msg += ' ';
					}

					if ((nDataLength - pos - 9) > 0)
					{
						memcpy(q, p, nDataLength - pos - 9);
						q += nDataLength - pos - 9;
					}
					break;
				}
				else
					*(q++) = *(p++);

				pos++;
			}
			*q = 0;
			//if (q != buffer)
			//	msg += ConvFromNetwork(buffer);
			if (msg != _T(""))
				ShowStatus(msg, *pData);
			delete [] buffer;
		}
		break;
	case 7:
		if (nDataLength != 5)
			ShowStatus(_T("Protocol error: Invalid data"), 1);
		else
		{
			int nType = *pData;
			int size = (int)GET32(pData + 1);

			if (!nType)
			{
				m_nRecvCount += size;
				m_RecvLed.Ping(100);
				CString str;
				str.Format(_T("%s bytes received"), makeUserFriendlyString(m_nRecvCount).GetString());
				SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_RECVCOUNT), str);
			}
			else
			{
				m_nSendCount += size;
				m_SendLed.Ping(100);
				CString str;
				str.Format(_T("%s bytes sent"), makeUserFriendlyString(m_nSendCount).GetString());
				SetStatusbarText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SENDCOUNT), str);
			}

		}
		break;
	default:
		{
			CString str;
			str.Format(_T("Protocol error: Unexpected status id (%d)."), nStatusID);
			ShowStatus(str, 1);
		}
		break;

	}
}

BOOL CMainFrame::SendCommand(int nType)
{
	if (!m_pAdminSocket)
		return FALSE;
	//if (!m_pAdminSocket->SendCommand(nType))
	{
		CloseAdminSocket();
		ShowStatus("Error: Connection to server lost...", 1);
		return FALSE;
	}
	return TRUE;
}

BOOL CMainFrame::SendCommand(int nType, void *pData, int nDataLength)
{
	if (!m_pAdminSocket)
		return FALSE;
	//if (!m_pAdminSocket->SendCommand(nType, pData, nDataLength))
	{
		CloseAdminSocket();
		ShowStatus("Error: Connection to server lost...", 1);
		return FALSE;
	}
	return TRUE;
}

void CMainFrame::CloseAdminSocket(bool shouldReconnect)
{
	if (m_pAdminSocket) {
		//m_pAdminSocket->DoClose();
		delete m_pAdminSocket;
		m_pAdminSocket = NULL;
		SetIcon();

		CString title;
		title.LoadString(IDR_MAINFRAME);
		SetWindowText(title + _T(" (disconnected)"));
	}
	m_nEdit = 0;

	if (!shouldReconnect) {
		if (m_nReconnectTimerID) {
			KillTimer(m_nReconnectTimerID);
			m_nReconnectTimerID = 0;
		}
	}
	else {
		if (!m_nReconnectTimerID) {
			++m_nReconnectCount;
			//if (m_nReconnectCount < m_pOptions->GetOptionVal(IOPTION_RECONNECTCOUNT))
			{
				ShowStatus("Trying to reconnect in 5 seconds", 0);
				m_nReconnectTimerID = SetTimer(7779, 5000, 0);
			}
			//else
				m_nReconnectCount = 0;
		}
	}
}

void CMainFrame::OnFileConnect()
{
	if (m_nReconnectTimerID)
	{
		KillTimer(m_nReconnectTimerID);
		m_nReconnectTimerID = 0;
	}
	if (m_pAdminSocket)
		if (AfxMessageBox(_T("Do you really want to close the current connection?"), MB_ICONQUESTION|MB_YESNO) != IDYES)
			return;
	CConnectDialog dlg(m_pOptions);
	if (dlg.DoModal() == IDOK) {
		CloseAdminSocket(false);
		DoConnect();
	}
}

void CMainFrame::OnFileDisconnect()
{
	CloseAdminSocket(false);
}

void CMainFrame::OnUpdateFileDisconnect(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pAdminSocket?TRUE:FALSE);
}

void CMainFrame::OnUpdateEditSettings(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected());
}

void CMainFrame::OnUpdateMenuEditUsers(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected());
}

void CMainFrame::OnUpdateMenuEditGroups(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected());
}

void CMainFrame::OnUpdateUsers(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected());
}

void CMainFrame::OnUpdateGroups(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(m_pAdminSocket && m_pAdminSocket->IsConnected());
}

// this function gets called whenever the user decides to quit the app
// (by pressing the 'x' for example
void CMainFrame::OnDestroy()
{
	//m_pOptions->SetOption(IOPTION_USERSORTING, GetUsersPane()->m_pListCtrl->GetSortColumn() + (GetUsersPane()->m_pListCtrl->GetSortDirection() << 4));
	CloseAdminSocket(false);
	CFrameWnd::OnDestroy();
}

BOOL CMainFrame::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

LRESULT CMainFrame::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_APP) {
		if ((m_nEdit & 0x1C) == 0x0C) {
			m_nEdit |= 0x10;
			if (m_pOptionsDlg->Show()) {
				unsigned char *pBuffer{};
				DWORD dwBufferLength;
				if (m_pOptionsDlg->GetAsCommand(&pBuffer, &dwBufferLength)) {
					SendCommand(5, pBuffer, dwBufferLength);
					ShowStatus(_T("Sending settings, please wait..."), 0);
					delete [] pBuffer;
				}
				else {
					ShowStatus(_T("Could not serialize settings, too much data."), 1);
				}
			}
			delete m_pOptionsDlg;
			m_pOptionsDlg = 0;
			m_nEdit = 0;
		}
		else if ((m_nEdit & 0x19) == 0x09) {
			m_nEdit |= 0x10;
			if (m_pUsersDlg->DoModal() == IDOK) {
				unsigned char *pBuffer{};
				DWORD dwBufferLength;
				if (m_pUsersDlg->GetAsCommand(&pBuffer, &dwBufferLength)) {
					SendCommand(6, pBuffer, dwBufferLength);
					ShowStatus(_T("Sending account settings, please wait..."), 0);
					delete [] pBuffer;
				}
			}
			delete m_pUsersDlg;
			m_pUsersDlg = 0;
			m_nEdit = 0;
		}
		else if ((m_nEdit & 0x1A) == 0x0A) {
			m_nEdit |= 0x10;
			if (m_pGroupsDlg->DoModal() == IDOK) {
				unsigned char *pBuffer;
				DWORD dwBufferLength;
				if (m_pGroupsDlg->GetAsCommand(&pBuffer, &dwBufferLength)) {
					SendCommand(6, pBuffer, dwBufferLength);
					ShowStatus(_T("Sending account settings, please wait..."), 0);
					delete [] pBuffer;
				}
			}
			delete m_pGroupsDlg;
			m_pGroupsDlg = 0;
			m_nEdit = 0;
		}
	}
	else if (message == WM_APP + 1)
		OnAdminInterfaceClosed();

	return CFrameWnd::DefWindowProc(message, wParam, lParam);
}

void CMainFrame::OnAdminInterfaceConnected()
{
	m_nReconnectCount = 0;

	CString title;
	title.LoadString(IDR_MAINFRAME);
	CString ip;
	UINT port;
	//if (m_pAdminSocket->GetPeerName(ip, port))
	{
		//by zhangyl
		//if (ip.Find(':') != -1)
		//	ip = GetIPV6ShortForm(ip);
		SetWindowText(title + _T(" (") + ip + _T(")"));
	}
}

void CMainFrame::OnAdminInterfaceClosed()
{
	//if (m_pAdminSocket && m_pAdminSocket->IsClosed())
	{
		CloseAdminSocket();
	}
}

void CMainFrame::OnDisplayLogicalNames()
{
	GetUsersPane()->m_pListCtrl->SetDisplayPhysicalNames(false);
	//m_pOptions->SetOption(IOPTION_FILENAMEDISPLAY, 0);
}

void CMainFrame::OnDisplayPhysicalNames()
{
	GetUsersPane()->m_pListCtrl->SetDisplayPhysicalNames(true);
	//m_pOptions->SetOption(IOPTION_FILENAMEDISPLAY, 1);
}

void CMainFrame::OnUpdateDisplayLogicalNames(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(!GetUsersPane()->m_pListCtrl->GetDisplayPhysicalNames());
}

void CMainFrame::OnUpdateDisplayPhysicalNames(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetUsersPane()->m_pListCtrl->GetDisplayPhysicalNames());
}

void CMainFrame::OnDisplaySortMenu()
{
	// load and display popup menu
	CMenu menu;
	menu.LoadMenu(IDR_SORTMENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);

	CRect rc;
	m_wndUserListToolBar.GetItemRect(m_wndUserListToolBar.CommandToIndex(ID_USERLISTTOOLBAR_SORT), &rc);
	m_wndUserListToolBar.ClientToScreen(&rc);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		rc.left, rc.bottom, this, &rc);
}

void CMainFrame::OnToolbarDropDown(NMHDR* pnmh, LRESULT* plRes)
{
	NMTOOLBAR* pnmtb = (NMTOOLBAR*)pnmh;
	if (pnmtb->iItem == ID_USERLISTTOOLBAR_SORT)
		OnDisplaySortMenu();
}

void CMainFrame::OnDisplaySortByUserid()
{
	GetUsersPane()->m_pListCtrl->SetSortColumn(0);
}

void CMainFrame::OnDisplaySortByAccount()
{
	GetUsersPane()->m_pListCtrl->SetSortColumn(1);
}

void CMainFrame::OnDisplaySortByIP()
{
	GetUsersPane()->m_pListCtrl->SetSortColumn(2);
}

void CMainFrame::OnUpdateDisplaySortByUserid(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetUsersPane()->m_pListCtrl->GetSortColumn() == 0);
}

void CMainFrame::OnUpdateDisplaySortByAccount(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetUsersPane()->m_pListCtrl->GetSortColumn() == 1);
}

void CMainFrame::OnUpdateDisplaySortByIP(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(GetUsersPane()->m_pListCtrl->GetSortColumn() == 2);
}

void CMainFrame::DoConnect()
{
	//CStdString address = m_pOptions->GetOption(IOPTION_LASTSERVERADDRESS);
	//unsigned int port = static_cast<unsigned int>(m_pOptions->GetOptionVal(IOPTION_LASTSERVERPORT));

	//int family;
	////by zhangyl
	////if (!GetIPV6LongForm(address).IsEmpty()) {
	////	if (address.Left(1) != '[') {
	////		address = _T("[") + address + _T("]");
	////	}
	////	family = AF_INET6;
	////}
	////else
	////	family = AF_INET;

	//CString msg;
	//msg.Format(_T("Connecting to server %s:%u..."), address, port);
	//ShowStatus(msg, 0);

	//m_pAdminSocket = new CAdminSocket(this);
	//m_pAdminSocket->Create(0, SOCK_STREAM, FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE, 0, family);

	//m_pAdminSocket->m_Password = m_pOptions->GetOption(IOPTION_LASTSERVERPASS);
	//if (!m_pAdminSocket->Connect(address, (UINT)port) && WSAGetLastError() != WSAEWOULDBLOCK) {
	//	ShowStatus(_T("Error, could not connect to server"), 1);
	//	CloseAdminSocket();
	//}
}