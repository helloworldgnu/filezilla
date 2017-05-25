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

// MainFrm.h : Schnittstelle der Klasse CMainFrame
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__741499DF_FFBB_481F_B214_8C14C31217BB__INCLUDED_)
#define AFX_MAINFRM_H__741499DF_FFBB_481F_B214_8C14C31217BB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "misc\systemtray.h"
#include "misc\led.h"
#include "Options.h"	// Hinzugefügt von der Klassenansicht
#include "splitex.h"

class CStatusView;
class CUsersView;
class CAdminSocket;

class CUsersDlg;
class CGroupsDlg;
//by zyl
class COptionsDlg;
class CMainFrame : public CFrameWnd
{

public:
	CMainFrame(COptions *pOptions);
protected:
	DECLARE_DYNAMIC(CMainFrame)

// Attribute
public:

// Operationen
public:
	void ParseReply(int nReplyID, unsigned char *pData, int nDataLength);
	void ParseStatus(int nStatusID, unsigned char *pData, int nDataLength);

	CStatusView* GetStatusPane();
	CUsersView* GetUsersPane();


// Überladungen
	// Vom Klassenassistenten generierte Überladungen virtueller Funktionen
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	//}}AFX_VIRTUAL

// Implementierung
public:
	void CloseAdminSocket(bool shouldReconnect = true);
	void ShowStatusRaw(const char *status, int nType);
	void ShowStatus(const CString& status, int nType);
	bool m_bQuit{};
	void SetIcon();
	BOOL SendCommand(int nType);
	BOOL SendCommand(int nType, void *pData, int nDataLength);
	void OnAdminInterfaceConnected();
	void OnAdminInterfaceClosed();

	virtual ~CMainFrame();

protected:
	void DoConnect();

	CAdminSocket *m_pAdminSocket;
	int m_nServerState{};
	CStatusView* m_pStatusPane;
	CUsersView* m_pUsersPane;

	void SetStatusbarText(int nIndex, CString str);
	CLed m_SendLed;
	CLed m_RecvLed;
	CStatusBar  m_wndStatusBar;
	CToolBar	m_wndToolBar;
	CToolBar	m_wndUserListToolBar;
	CSplitterWndEx m_wndSplitter;

	//// Internal support functions
	void SetupTrayIcon();

	//// Internal data
	CSystemTray m_TrayIcon;
	int nTrayNotificationMsg_;

	// static data member to hold window class name
	LPTSTR s_winClassName;

	CUsersDlg *m_pUsersDlg;
	CGroupsDlg *m_pGroupsDlg;
	COptionsDlg *m_pOptionsDlg;

// Generierte Message-Map-Funktionen
protected:
	int m_nEdit;
	COptions *m_pOptions;
	UINT_PTR m_nRateTimerID;
	unsigned __int64 m_nSendCount;
	unsigned __int64 m_nRecvCount;
	__int64 m_nOldSendCount;
	__int64 m_nOldRecvCount;
	DWORD m_lastchecktime;
	__int64 m_lastreaddiff;
	__int64 m_lastwritediff;
	UINT_PTR m_nTimerID;
	UINT_PTR m_nReconnectTimerID;
	int m_nReconnectCount;
	CReBar m_wndReBar;
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	afx_msg void OnEditSettings();
	afx_msg void OnActive();
	afx_msg void OnUpdateActive(CCmdUI* pCmdUI);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnTrayExit();
	afx_msg void OnTrayRestore();
	afx_msg void OnLock();
	afx_msg void OnUpdateLock(CCmdUI* pCmdUI);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnMenuEditUsers();
	afx_msg void OnMenuEditGroups();
	afx_msg void OnFileConnect();
	afx_msg void OnFileDisconnect();
	afx_msg void OnUpdateFileDisconnect(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditSettings(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMenuEditUsers(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMenuEditGroups(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUsers(CCmdUI* pCmdUI);
	afx_msg void OnUpdateGroups(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnDisplayLogicalNames();
	afx_msg void OnDisplayPhysicalNames();
	afx_msg void OnUpdateDisplayLogicalNames(CCmdUI* pCmdUI);
	afx_msg void OnUpdateDisplayPhysicalNames(CCmdUI* pCmdUI);
	afx_msg void OnDisplaySortMenu();
	afx_msg void OnToolbarDropDown(NMHDR* pnmh, LRESULT* plRes);
	afx_msg void OnDisplaySortByUserid();
	afx_msg void OnDisplaySortByAccount();
	afx_msg void OnDisplaySortByIP();
	afx_msg void OnUpdateDisplaySortByUserid(CCmdUI* pCmdUI);
	afx_msg void OnUpdateDisplaySortByAccount(CCmdUI* pCmdUI);
	afx_msg void OnUpdateDisplaySortByIP(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
protected:
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // !defined(AFX_MAINFRM_H__741499DF_FFBB_481F_B214_8C14C31217BB__INCLUDED_)
