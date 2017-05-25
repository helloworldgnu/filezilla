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

// UsersDlg.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "FileZilla server.h"
#include "UsersDlg.h"
#include "misc\sbdestination.h"
#include "entersomething.h"
#include "NewUserDlg.h"
#include "UsersDlgGeneral.h"
#include "UsersDlgSpeedLimit.h"
#include "UsersDlgSharedFolders.h"
#include "UsersDlgIpFilter.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CUsersDlg

CUsersDlg::CUsersDlg(CWnd* pParent, bool localConnection)
	: CSAPrefsDialog(IDD, pParent)
{
	m_insideSelchange = false;
	m_localConnection = localConnection;

	m_pGeneralPage = new CUsersDlgGeneral(this);
	m_pSpeedLimitPage = new CUsersDlgSpeedLimit(this);
	m_pSharedFoldersPage = new CUsersDlgSharedFolders(this);
	m_pIpFilterPage = new CUsersDlgIpFilter(this);

	AddPage(*m_pGeneralPage, _T("General"));
	AddPage(*m_pSharedFoldersPage, _T("Shared folders"));
	AddPage(*m_pSpeedLimitPage, _T("Speed Limits"));
	AddPage(*m_pIpFilterPage, _T("IP Filter"));
}

CUsersDlg::~CUsersDlg()
{
	delete m_pGeneralPage;
	delete m_pSpeedLimitPage;
	delete m_pSharedFoldersPage;
	delete m_pIpFilterPage;
}


void CUsersDlg::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUsersDlg)
	DDX_Control(pDX, IDC_USERLIST, m_cUserlist);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUsersDlg, CSAPrefsDialog)
	//{{AFX_MSG_MAP(CUsersDlg)
	ON_LBN_SELCHANGE(IDC_USERLIST, OnSelchangeUserlist)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_USERMENU_ADD, OnUsermenuAdd)
	ON_COMMAND(ID_USERMENU_COPY, OnUsermenuCopy)
	ON_COMMAND(ID_USERMENU_REMOVE, OnUsermenuRemove)
	ON_COMMAND(ID_USERMENU_RENAME, OnUsermenuRename)
	ON_BN_CLICKED(IDC_USERADD, OnUseradd)
	ON_BN_CLICKED(IDC_USERCOPY, OnUsercopy)
	ON_BN_CLICKED(IDC_USERREMOVE, OnUserremove)
	ON_BN_CLICKED(IDC_USERRENAME, OnUserrename)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CUsersDlg

BOOL CUsersDlg::OnInitDialog()
{
	CSAPrefsDialog::OnInitDialog();

	m_olduser = LB_ERR;

	m_cUserlist.ResetContent();

	for (unsigned int i = 0;i < m_UsersList.size(); ++i) {
		int index=m_cUserlist.AddString(m_UsersList[i].user);
		m_cUserlist.SetItemData(index, i);
	}

	if (m_UsersList.size()) {
		m_cUserlist.SetCurSel(0);
		OnSelchangeUserlist();
	}

	SetCtrlState();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

BOOL CUsersDlg::Validate()
{
	CString res = m_pGeneralPage->Validate();
	if (res != _T("")) {
		ShowPage(m_pGeneralPage);
		m_cUserlist.SetCurSel(m_olduser);
		MessageBox(res);
		return FALSE;
	}
	res = m_pSpeedLimitPage->Validate();
	if (res != _T("")) {
		ShowPage(m_pSpeedLimitPage);
		m_cUserlist.SetCurSel(m_olduser);
		MessageBox(res);
		return FALSE;
	}
	res = m_pSharedFoldersPage->Validate();
	if (res != _T("")) {
		ShowPage(m_pSharedFoldersPage);
		m_cUserlist.SetCurSel(m_olduser);
		MessageBox(res);
		return FALSE;
	}
	res = m_pIpFilterPage->Validate();
	if (res != _T("")) {
		ShowPage(m_pIpFilterPage);
		m_cUserlist.SetCurSel(m_olduser);
		MessageBox(res);
		return FALSE;
	}
	return TRUE;
}

void CUsersDlg::OnSelchangeUserlist()
{
	m_insideSelchange = true;
	if (!Validate()) {
		m_insideSelchange = false;
		return;
	}
	m_insideSelchange = false;

	if (m_olduser != LB_ERR) {
		int oldindex = m_cUserlist.GetItemData(m_olduser);
		t_user & user = m_UsersList[oldindex];
		SaveUser(user);
	}
	int nItem = m_cUserlist.GetCurSel();
	if (nItem != LB_ERR) {
		m_olduser = nItem;
		int index = m_cUserlist.GetItemData(nItem);
		VERIFY(m_pGeneralPage->DisplayUser(&m_UsersList[index]));
		VERIFY(m_pSpeedLimitPage->DisplayUser(&m_UsersList[index]));
		VERIFY(m_pSharedFoldersPage->DisplayUser(&m_UsersList[index]));
		VERIFY(m_pIpFilterPage->DisplayUser(&m_UsersList[index]));
	}
	else {
		VERIFY(m_pGeneralPage->DisplayUser(NULL));
		VERIFY(m_pSpeedLimitPage->DisplayUser(NULL));
		VERIFY(m_pSharedFoldersPage->DisplayUser(0));
		VERIFY(m_pIpFilterPage->DisplayUser(0));
	}
	m_pGeneralPage->UpdateData(FALSE);

	SetCtrlState();
}

void CUsersDlg::OnOK()
{
	if (!Validate())
		return;
	m_cUserlist.SetCurSel(-1);
	OnSelchangeUserlist();

	CSAPrefsDialog::OnOK();
}

void CUsersDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (pWnd == &m_cUserlist) {
		CMenu menu;
		menu.LoadMenu(IDR_USERCONTEXT);

		CMenu* pPopup = menu.GetSubMenu(0);
		ASSERT(pPopup != NULL);
		CWnd* pWndPopupOwner = this;
		while (pWndPopupOwner->GetStyle() & WS_CHILD)
			pWndPopupOwner = pWndPopupOwner->GetParent();

		if (m_cUserlist.GetCurSel() == LB_ERR) {
			pPopup->EnableMenuItem(ID_USERMENU_COPY, MF_GRAYED);
			pPopup->EnableMenuItem(ID_USERMENU_REMOVE, MF_GRAYED);
			pPopup->EnableMenuItem(ID_USERMENU_RENAME, MF_GRAYED);
		}
		if (point.x == -1)
			GetCursorPos(&point);
		pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			pWndPopupOwner);
	}
}

void CUsersDlg::OnUsermenuAdd()
{
	if (!Validate())
		return;

	CNewUserDlg dlg;
	for (t_GroupsList::iterator iter = m_GroupsList.begin(); iter != m_GroupsList.end(); ++iter) {
		dlg.m_GroupList.push_back(iter->group);
	}
	if (dlg.DoModal() == IDOK) {
		CString newname = dlg.m_Name;
		newname.MakeLower();
		for (int i = 0; i < m_cUserlist.GetCount(); ++i) {
			CString str;
			m_cUserlist.GetText(i, str);
			str.MakeLower();
			if (str == newname) {
				AfxMessageBox(IDS_ERRORMSG_USERALREADYEXISTS);
				return;
			}
		}

		t_user user;
		user.nEnabled = true;
		user.user = dlg.m_Name;
		if (dlg.m_Group != _T("")) {
			user.group = dlg.m_Group;
			user.nBypassUserLimit = 2;
			user.nBypassServerSpeedLimit[download] = 2;
			user.nBypassServerSpeedLimit[upload] = 2;
			user.nEnabled = 2;
			user.forceSsl = 2;
		}
		else {
			user.nBypassUserLimit = 0;
			user.nBypassServerSpeedLimit[download] = 0;
			user.nBypassServerSpeedLimit[upload] = 0;
			user.forceSsl = 0;
		}
		user.nIpLimit = 0;
		user.nUserLimit = 0;
		user.password = _T("");
		int nItem = m_cUserlist.AddString(user.user);
		if (nItem <= m_olduser)
			++m_olduser;
		m_UsersList.push_back(user);
		m_cUserlist.SetItemData(nItem, m_UsersList.size() - 1);
		m_cUserlist.SetCurSel(nItem);
		OnSelchangeUserlist();
	}
}

void CUsersDlg::OnUsermenuCopy()
{
	if (!Validate())
		return;

	int pos = m_cUserlist.GetCurSel();
	if (pos == LB_ERR)
		return;
	int index = m_cUserlist.GetItemData(pos);

	SaveUser(m_UsersList[index]);

	CEnterSomething dlg(IDS_COPYUSERDIALOG);
	if (dlg.DoModal() == IDOK) {
		int i;
		CString newname = dlg.m_String;
		newname.MakeLower();
		for (i = 0; i < m_cUserlist.GetCount(); ++i) {
			CString str;
			m_cUserlist.GetText(i, str);
			str.MakeLower();
			if (str == newname) {
				AfxMessageBox(IDS_ERRORMSG_USERALREADYEXISTS);
				return;
			}
		}

		t_user user = m_UsersList[index];
		user.user = dlg.m_String;

		int nItem = m_cUserlist.AddString(user.user);
		if (nItem <= m_olduser)
			++m_olduser;
		m_UsersList.push_back(user);
		m_cUserlist.SetItemData(nItem, m_UsersList.size() - 1);
		m_cUserlist.SetCurSel(nItem);

		OnSelchangeUserlist();
	}
}


void CUsersDlg::OnUsermenuRemove()
{
	int pos = m_cUserlist.GetCurSel();
	if (pos == LB_ERR)
		return;
	int index = m_cUserlist.GetItemData(pos);
	m_olduser = LB_ERR;
	int i = 0;
	for (t_UsersList::iterator iter = m_UsersList.begin(); iter != m_UsersList.end(); ++iter, ++i) {
		if (i == index) {
			m_UsersList.erase(iter);
			break;
		}
	}
	for (i = 0; i < m_cUserlist.GetCount(); ++i) {
		int data = m_cUserlist.GetItemData(i);
		if (data > index)
			m_cUserlist.SetItemData(i, data - 1);
	}
	m_cUserlist.DeleteString(pos);
	OnSelchangeUserlist();
}

void CUsersDlg::OnUsermenuRename()
{
	if (!Validate())
		return;

	int pos = m_cUserlist.GetCurSel();
	if (pos == LB_ERR)
		return;
	int index = m_cUserlist.GetItemData(pos);

	CEnterSomething dlg(IDS_INPUTDIALOGTEXT_RENAME);
	if (dlg.DoModal() == IDOK) {
		CString newname = dlg.m_String;
		newname.MakeLower();
		for (int i = 0; i < m_cUserlist.GetCount(); ++i) {
			if (i == pos)
				continue;
			CString str;
			m_cUserlist.GetText(i, str);
			str.MakeLower();
			if (str == newname) {
				AfxMessageBox(IDS_ERRORMSG_USERALREADYEXISTS);
				return;
			}
		}

		m_cUserlist.DeleteString(pos);
		pos = m_cUserlist.AddString(dlg.m_String);
		m_cUserlist.SetItemData(pos, index);
		m_cUserlist.SetCurSel(pos);
		m_olduser = pos;
		m_UsersList[index].user = dlg.m_String;
		OnSelchangeUserlist();
	}
}

void CUsersDlg::SetCtrlState()
{
	if (m_cUserlist.GetCurSel() == LB_ERR) {
		GetDlgItem(IDC_USERREMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_USERRENAME)->EnableWindow(FALSE);
		GetDlgItem(IDC_USERCOPY)->EnableWindow(FALSE);
	}
	else {
		GetDlgItem(IDC_USERREMOVE)->EnableWindow(TRUE);
		GetDlgItem(IDC_USERRENAME)->EnableWindow(TRUE);
		GetDlgItem(IDC_USERCOPY)->EnableWindow(TRUE);
	}
	m_pGeneralPage->SetCtrlState();
	m_pSpeedLimitPage->SetCtrlState();
	m_pSharedFoldersPage->SetCtrlState();
	m_pIpFilterPage->SetCtrlState();
}

void CUsersDlg::OnUseradd()
{
	OnUsermenuAdd();
}

void CUsersDlg::OnUsercopy()
{
	OnUsermenuCopy();
}

void CUsersDlg::OnUserremove()
{
	OnUsermenuRemove();
}

void CUsersDlg::OnUserrename()
{
	OnUsermenuRename();
}

bool CUsersDlg::GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength)
{
	if (!pBuffer) {
		return false;
	}

	DWORD len = 3 * 2;
	if (m_GroupsList.size() > 0xffffff || m_UsersList.size() > 0xffffff) {
		return false;
	}
	for (auto const& group : m_GroupsList) {
		len += group.GetRequiredBufferLen();
	}
	for (auto const& user : m_UsersList) {
		len += user.GetRequiredBufferLen();
	}

	*pBuffer = new unsigned char[len];
	unsigned char *p = *pBuffer;

	*p++ = ((m_GroupsList.size() / 256) / 256) & 255;
	*p++ = (m_GroupsList.size() / 256) % 256;
	*p++ = m_GroupsList.size() % 256;
	for (auto const& group : m_GroupsList) {
		p = group.FillBuffer(p);
		if (!p) {
			delete[] * pBuffer;
			*pBuffer = 0;
			return false;
		}
	}

	*p++ = ((m_UsersList.size() / 256) / 256) & 255;
	*p++ = (m_UsersList.size() / 256) % 256;
	*p++ = m_UsersList.size() % 256;
	for (auto const& user : m_UsersList) {
		p = user.FillBuffer(p);
		if (!p) {
			delete[] * pBuffer;
			*pBuffer = 0;
			return false;
		}
	}

	*nBufferLength = len;

	return true;
}

BOOL CUsersDlg::Init(unsigned char *pData, DWORD dwDataLength)
{
	unsigned char *p = pData;
	unsigned char const* const endMarker = p + dwDataLength;

	if ((endMarker - p) < 3)
		return FALSE;
	unsigned int num = *p * 256 * 256 + p[1] * 256 + p[2];
	p += 3;
	for (unsigned int i = 0; i < num; ++i) {
		t_group group;
		p = group.ParseBuffer(p, endMarker - p);
		if (!p)
			return FALSE;
		m_GroupsList.push_back(group);
	}

	if ((endMarker - p) < 3)
		return FALSE;
	num = *p * 256 * 256 + p[1] * 256 + p[2];
	p += 3;
	for (unsigned i = 0; i < num; ++i) {
		t_user user;
		p = user.ParseBuffer(p, endMarker - p);
		if (!p)
			return FALSE;
		m_UsersList.push_back(user);
	}
	return TRUE;
}

BOOL CUsersDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		if (pMsg->wParam == VK_F2) {
			if (GetFocus() == &m_cUserlist) {
				if (m_cUserlist.GetCurSel() == LB_ERR)
					return TRUE;
				OnUsermenuRename();
			}
			return TRUE;
		}
	}
	return CSAPrefsDialog::PreTranslateMessage(pMsg);
}

t_user* CUsersDlg::GetCurrentUser()
{
	if (m_cUserlist.GetCurSel() == LB_ERR) {
		return NULL;
	}
	else {
		if (m_insideSelchange) {
			if (m_olduser == LB_ERR)
				return NULL;
			else
				return &m_UsersList[m_cUserlist.GetItemData(m_olduser)];
		}
		return &m_UsersList[m_cUserlist.GetItemData(m_cUserlist.GetCurSel())];
	}
}

void CUsersDlg::SaveUser(t_user & user)
{
	VERIFY(m_pGeneralPage->SaveUser(user));
	VERIFY(m_pSpeedLimitPage->SaveUser(user));
	VERIFY(m_pSharedFoldersPage->SaveUser(user));
	VERIFY(m_pIpFilterPage->SaveUser(user));
}