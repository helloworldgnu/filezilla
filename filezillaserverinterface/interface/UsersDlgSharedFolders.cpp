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

#include "stdafx.h"
#include "FileZilla server.h"
#include "UsersDlgSharedFolders.h"
#include "misc\sbdestination.h"
#include "entersomething.h"
#include "UsersDlg.h"

#include <set>

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CUsersDlgSharedFolders

CUsersDlgSharedFolders::CUsersDlgSharedFolders(CUsersDlg* pOwner)
	: CSAPrefsSubDlg(IDD)
{
	m_pOwner = pOwner;

	//{{AFX_DATA_INIT(CUsersDlgSharedFolders)
	m_bDirsCreate = FALSE;
	m_bDirsDelete = FALSE;
	m_bDirsList = FALSE;
	m_bDirsSubdirs = FALSE;
	m_bFilesAppend = FALSE;
	m_bFilesDelete = FALSE;
	m_bFilesRead = FALSE;
	m_bFilesWrite = FALSE;
	//}}AFX_DATA_INIT
}

CUsersDlgSharedFolders::~CUsersDlgSharedFolders()
{
}


void CUsersDlgSharedFolders::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUsersDlgSharedFolders)
	DDX_Control(pDX, IDC_DIRS_CREATE, m_cDirsCreate);
	DDX_Control(pDX, IDC_DIRS_DELETE, m_cDirsDelete);
	DDX_Control(pDX, IDC_DIRS_LIST, m_cDirsList);
	DDX_Control(pDX, IDC_DIRS_SUBDIRS, m_cDirsSubdirs);
	DDX_Control(pDX, IDC_FILES_READ, m_cFilesRead);
	DDX_Control(pDX, IDC_FILES_WRITE, m_cFilesWrite);
	DDX_Control(pDX, IDC_FILES_DELETE, m_cFilesDelete);
	DDX_Control(pDX, IDC_FILES_APPEND, m_cFilesAppend);
	DDX_Control(pDX, IDC_DIRS, m_cDirs);
	DDX_Check(pDX, IDC_DIRS_CREATE, m_bDirsCreate);
	DDX_Check(pDX, IDC_DIRS_DELETE, m_bDirsDelete);
	DDX_Check(pDX, IDC_DIRS_LIST, m_bDirsList);
	DDX_Check(pDX, IDC_DIRS_SUBDIRS, m_bDirsSubdirs);
	DDX_Check(pDX, IDC_FILES_APPEND, m_bFilesAppend);
	DDX_Check(pDX, IDC_FILES_DELETE, m_bFilesDelete);
	DDX_Check(pDX, IDC_FILES_READ, m_bFilesRead);
	DDX_Check(pDX, IDC_FILES_WRITE, m_bFilesWrite);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUsersDlgSharedFolders, CSAPrefsSubDlg)
	//{{AFX_MSG_MAP(CUsersDlgSharedFolders)
	ON_WM_CONTEXTMENU()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_DIRS, OnItemchangedDirs)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_DIRS, OnItemchangingDirs)
	ON_COMMAND(ID_DIRMENU_ADD, OnDirmenuAdd)
	ON_COMMAND(ID_DIRMENU_REMOVE, OnDirmenuRemove)
	ON_COMMAND(ID_DIRMENU_RENAME, OnDirmenuRename)
	ON_COMMAND(ID_DIRMENU_SETASHOMEDIR, OnDirmenuSetashomedir)
	ON_COMMAND(ID_DIRMENU_EDITALIASES, OnDirmenuEditAliases)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_DIRS, OnEndlabeleditDirs)
	ON_NOTIFY(NM_DBLCLK, IDC_DIRS, OnDblclkDirs)
	ON_BN_CLICKED(IDC_FILES_WRITE, OnFilesWrite)
	ON_BN_CLICKED(IDC_DIRADD, OnDiradd)
	ON_BN_CLICKED(IDC_DIRREMOVE, OnDirremove)
	ON_BN_CLICKED(IDC_DIRRENAME, OnDirrename)
	ON_BN_CLICKED(IDC_DIRSETASHOME, OnDirsetashome)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CUsersDlgSharedFolders

BOOL CUsersDlgSharedFolders::OnInitDialog()
{
	CSAPrefsSubDlg::OnInitDialog();

	m_cDirs.InsertColumn(0, _T("Directories"), LVCFMT_LEFT, 120);
	m_cDirs.InsertColumn(1, _T("Aliases"), LVCFMT_LEFT, 200);
	UpdateData(FALSE);

	m_imagelist.Create( 16, 16, ILC_MASK, 3, 3 );
	HICON icon;
	icon = AfxGetApp()->LoadIcon(IDI_EMPTY);
	m_imagelist.Add(icon);
	DestroyIcon(icon);
	icon = AfxGetApp()->LoadIcon(IDI_HOME);
	m_imagelist.Add(icon);
	DestroyIcon(icon);

	m_cDirs.SetImageList(&m_imagelist, LVSIL_SMALL);

	m_cDirs.SetExtendedStyle(LVS_EX_FULLROWSELECT);

	SetCtrlState();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

struct cmp
{
	bool operator()(CString const& lhs, CString const& rhs) {
		return lhs.CompareNoCase(rhs) < 0;
	}
};

CString CUsersDlgSharedFolders::Validate()
{
	UpdateData(TRUE);

	t_user* pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return _T("");

	if (pUser->group == _T("") && pUser->permissions.empty()) {
		m_cDirs.SetFocus();
		return _T("You need to share at least one directory and set it as home directory.");
	}

	CString home;
	for( auto & perm : pUser->permissions ) {
		if (perm.dir == _T("") || perm.dir == _T("/") || perm.dir == _T("\\")) {
			m_cDirs.SetFocus();
			return _T("At least one shared directory is not a valid local path.");
		}

		if (perm.bIsHome) {
			home = perm.dir;
			perm.aliases.clear();
		}
	}

	if (home.IsEmpty() && pUser->group == _T("")) {
		m_cDirs.SetFocus();
		return _T("You need to set a home directory");
	}

	std::list<CString> prefixes;
	for( auto & perm : pUser->permissions ) {
		if (!perm.aliases.empty() || perm.bIsHome ) {
			prefixes.push_back(perm.dir + _T("\\"));
		}
	}
	for (auto const& perm : pUser->permissions) {
		if (!perm.aliases.empty() || perm.bIsHome) {
			continue;
		}

		CString dir = perm.dir + _T("\\");
		bool out_of_home = true;
		for (auto const& prefix : prefixes) {
			if (!dir.Left(prefix.GetLength()).CompareNoCase(prefix)) {
				out_of_home = false;
				break;
			}
		}

		if (out_of_home && pUser->group == _T("")) {
			m_cDirs.SetFocus();
			return _T("You have shared multiple unrelated directories. You need to assign aliases to link them together.\nDouble-click the alias column next to the unlinked directory.\n\nUnlinked directory: " + perm.dir);
		}
	}

	return _T("");
}

void CUsersDlgSharedFolders::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (pWnd==&m_cDirs)
	{
		CMenu menu;
		menu.LoadMenu(IDR_DIRCONTEXT);

		CMenu* pPopup = menu.GetSubMenu(0);
		ASSERT(pPopup != NULL);
		CWnd* pWndPopupOwner = this;
		//while (pWndPopupOwner->GetStyle() & WS_CHILD)
		//	pWndPopupOwner = pWndPopupOwner->GetParent();

		if (!m_cDirs.GetFirstSelectedItemPosition())
		{
			pPopup->EnableMenuItem(ID_DIRMENU_REMOVE, MF_GRAYED);
			pPopup->EnableMenuItem(ID_DIRMENU_RENAME, MF_GRAYED);
			pPopup->EnableMenuItem(ID_DIRMENU_SETASHOMEDIR, MF_GRAYED);
			pPopup->EnableMenuItem(ID_DIRMENU_EDITALIASES, MF_GRAYED);
		}
		if (point.x == -1)
			GetCursorPos(&point);
		pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			pWndPopupOwner);
	}
}

void CUsersDlgSharedFolders::OnItemchangedDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	int nItem = pNMListView->iItem;
	POSITION selpos=m_cDirs.GetFirstSelectedItemPosition();
	if (selpos)
	{
		if (m_cDirs.GetNextSelectedItem(selpos) != nItem)
			return;
	}
	int index=pNMListView->lParam;
	if (nItem!=-1)
	{
		m_bFilesRead = pUser->permissions[index].bFileRead;
		m_bFilesWrite = pUser->permissions[index].bFileWrite;
		m_bFilesDelete = pUser->permissions[index].bFileDelete;
		m_bFilesAppend = pUser->permissions[index].bFileAppend;
		m_bDirsCreate = pUser->permissions[index].bDirCreate;
		m_bDirsDelete = pUser->permissions[index].bDirDelete;
		m_bDirsList = pUser->permissions[index].bDirList;
		m_bDirsSubdirs = pUser->permissions[index].bDirSubdirs;
	}
	UpdateData(FALSE);

	SetCtrlState();
	*pResult = 0;
}

void CUsersDlgSharedFolders::OnItemchangingDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	UpdateData(TRUE);
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (selpos)
	{
		int item = m_cDirs.GetNextSelectedItem(selpos);
		int index = m_cDirs.GetItemData(item);
		pUser->permissions[index].bFileRead = m_bFilesRead;
		pUser->permissions[index].bFileWrite = m_bFilesWrite;
		pUser->permissions[index].bFileDelete = m_bFilesDelete;
		pUser->permissions[index].bFileAppend = m_bFilesAppend;
		pUser->permissions[index].bDirCreate = m_bDirsCreate;
		pUser->permissions[index].bDirDelete = m_bDirsDelete;
		pUser->permissions[index].bDirList = m_bDirsList;
		pUser->permissions[index].bDirSubdirs = m_bDirsSubdirs;
	}
}

void CUsersDlgSharedFolders::SetCtrlState()
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
	{
		m_cDirs.EnableWindow(FALSE);
		m_cFilesRead.EnableWindow(FALSE);
		m_cFilesWrite.EnableWindow(FALSE);
		m_cFilesDelete.EnableWindow(FALSE);
		m_cFilesAppend.EnableWindow(FALSE);
		m_cDirsCreate.EnableWindow(FALSE);
		m_cDirsDelete.EnableWindow(FALSE);
		m_cDirsList.EnableWindow(FALSE);
		m_cDirsSubdirs.EnableWindow(FALSE);
		GetDlgItem(IDC_DIRADD)->EnableWindow(FALSE);
		GetDlgItem(IDC_DIRREMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_DIRRENAME)->EnableWindow(FALSE);
		GetDlgItem(IDC_DIRSETASHOME)->EnableWindow(FALSE);

		m_bFilesAppend = m_bFilesDelete = m_bFilesRead = m_bFilesWrite = FALSE;
		m_bDirsCreate = m_bDirsDelete = m_bDirsList = m_bDirsSubdirs = FALSE;
		UpdateData(FALSE);
	}
	else
	{
		m_cDirs.EnableWindow(TRUE);
		GetDlgItem(IDC_DIRADD)->EnableWindow(TRUE);

		if (m_cDirs.GetFirstSelectedItemPosition())
		{
			m_cFilesRead.EnableWindow(TRUE);
			m_cFilesWrite.EnableWindow(TRUE);
			m_cFilesDelete.EnableWindow(TRUE);
			if (m_bFilesWrite)
				m_cFilesAppend.EnableWindow(TRUE);
			else
				m_cFilesAppend.EnableWindow(FALSE);
			m_cDirsCreate.EnableWindow(TRUE);
			m_cDirsDelete.EnableWindow(TRUE);
			m_cDirsList.EnableWindow(TRUE);
			m_cDirsSubdirs.EnableWindow(TRUE);
			GetDlgItem(IDC_DIRREMOVE)->EnableWindow(TRUE);
			GetDlgItem(IDC_DIRRENAME)->EnableWindow(TRUE);
			GetDlgItem(IDC_DIRSETASHOME)->EnableWindow(TRUE);
		}
		else
		{
			m_bFilesAppend = m_bFilesDelete = m_bFilesRead = m_bFilesWrite = FALSE;
			m_bDirsCreate = m_bDirsDelete = m_bDirsList = m_bDirsSubdirs = FALSE;
			UpdateData(FALSE);
			m_cFilesRead.EnableWindow(FALSE);
			m_cFilesWrite.EnableWindow(FALSE);
			m_cFilesDelete.EnableWindow(FALSE);
			m_cFilesAppend.EnableWindow(FALSE);
			m_cDirsCreate.EnableWindow(FALSE);
			m_cDirsDelete.EnableWindow(FALSE);
			m_cDirsList.EnableWindow(FALSE);
			m_cDirsSubdirs.EnableWindow(FALSE);
			GetDlgItem(IDC_DIRREMOVE)->EnableWindow(FALSE);
			GetDlgItem(IDC_DIRRENAME)->EnableWindow(FALSE);
			GetDlgItem(IDC_DIRSETASHOME)->EnableWindow(FALSE);
		}
	}
}

void CUsersDlgSharedFolders::OnDirmenuAdd()
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	t_directory dir;
	dir.bFileRead = dir.bDirList = dir.bDirSubdirs = TRUE;
	dir.bDirCreate = dir.bDirDelete =
		dir.bFileAppend = dir.bFileDelete =
		dir.bAutoCreate = dir.bFileWrite =
		dir.bIsHome = FALSE;
	dir.dir = _T("");
	if (pUser->group == _T("") && !m_cDirs.GetItemCount())
		dir.bIsHome = TRUE;
	else
		dir.bIsHome = FALSE;

	pUser->permissions.push_back(dir);
	int nItem = m_cDirs.InsertItem(LVIF_TEXT |LVIF_PARAM|LVIF_IMAGE, 0, _T("<new directory>"), 0, 0, dir.bIsHome?1:0, pUser->permissions.size()-1);
	m_cDirs.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
	m_cDirs.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
	m_cDirs.SetFocus();
	OnDblclkDirs(0, 0);
}

void CUsersDlgSharedFolders::OnDirmenuRemove()
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	POSITION selpos;
	selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);
	int index = m_cDirs.GetItemData(nItem);
	m_cDirs.DeleteItem(nItem);
	int i = 0;
	for (std::vector<t_directory>::iterator iter = pUser->permissions.begin(); iter != pUser->permissions.end(); ++iter, ++i)
		if (i == index)
		{
			pUser->permissions.erase(iter);
			break;
		}
	for (i = 0; i < m_cDirs.GetItemCount(); i++)
	{
		int data = m_cDirs.GetItemData(i);
		if (data > index)
		{
			m_cDirs.SetItemData(i, data - 1);
		}
	}
	SetCtrlState();
}

void CUsersDlgSharedFolders::OnDirmenuRename()
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);

	m_cDirs.SetFocus();
	m_cDirs.EditLabel(nItem);
	CString dir = m_cDirs.GetItemText(nItem, 0);
	dir.Replace('/', '\\');
	dir.TrimRight('\\');
	m_cDirs.SetItemText(nItem, 0, dir);
}

void CUsersDlgSharedFolders::OnDirmenuSetashomedir()
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	POSITION selpos;
	selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);

	for (unsigned int j = 0; j<pUser->permissions.size(); j++)
	{
		LVITEM item;
		memset(&item, 0, sizeof(item));
		item.mask = LVIF_IMAGE|LVIF_PARAM;
		item.iItem = j;
		m_cDirs.GetItem(&item);
		item.iImage = (j == (unsigned int)nItem) ? 1 : 0;
		pUser->permissions[item.lParam].bIsHome = 0;
		m_cDirs.SetItem(&item);
	}
	int index = m_cDirs.GetItemData(nItem);
	pUser->permissions[index].bIsHome = 1;
}

void CUsersDlgSharedFolders::OnEndlabeleditDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	if (pDispInfo->item.pszText)
	{
		if (pDispInfo->item.pszText[0] == 0)
		{
			AfxMessageBox(_T("Please select a folder!"));
			*pResult = FALSE;
		}
		else
		{
			t_user *pUser = m_pOwner->GetCurrentUser();
			if (!pUser)
				return;
			pUser->permissions[pDispInfo->item.lParam].dir = pDispInfo->item.pszText;
			*pResult = TRUE;
		}
	}
	else
	{
		if (m_cDirs.GetItemText(pDispInfo->item.iItem, 0) == _T(""))
		{
			t_user *pUser = m_pOwner->GetCurrentUser();
			if (!pUser)
				return;

			m_cDirs.DeleteItem(pDispInfo->item.iItem);
			int i=0;
			for (std::vector<t_directory>::iterator iter = pUser->permissions.begin(); iter != pUser->permissions.end(); iter++, i++)
				if (i == pDispInfo->item.lParam)
				{
					pUser->permissions.erase(iter);
					break;
				}
		}
	}
}

void CUsersDlgSharedFolders::OnDblclkDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	NMITEMACTIVATE *pItemActivate = (NMITEMACTIVATE *)pNMHDR;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);
	int index = m_cDirs.GetItemData(nItem);

	if (!pItemActivate || !pItemActivate->iSubItem)
	{
		if (m_pOwner->IsLocalConnection())
		{
			CSBDestination sb(m_hWnd, IDS_BROWSEFORFOLDER);
			sb.SetFlags(BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT);
			sb.SetInitialSelection(m_cDirs.GetItemText(nItem, 0));
			if (sb.SelectFolder())
			{
				m_cDirs.SetItemText(nItem, 0, sb.GetSelectedFolder());
				pUser->permissions[index].dir = sb.GetSelectedFolder();
			}
		}
		else
		{
			m_cDirs.SetFocus();
			m_cDirs.EditLabel(nItem);
		}
		CString dir = m_cDirs.GetItemText(nItem, 0);
		dir.Replace('/', '\\');
		dir.TrimRight('\\');
		m_cDirs.SetItemText(nItem, 0, dir);
	}
	else
		OnDirmenuEditAliases();

	if (pResult)
		*pResult = 0;
}

void CUsersDlgSharedFolders::OnFilesWrite()
{
	UpdateData(TRUE);
	SetCtrlState();
}

void CUsersDlgSharedFolders::OnDiradd()
{
	OnDirmenuAdd();
}

void CUsersDlgSharedFolders::OnDirremove()
{
	OnDirmenuRemove();
}

void CUsersDlgSharedFolders::OnDirrename()
{
	OnDirmenuRename();
}

void CUsersDlgSharedFolders::OnDirsetashome()
{
	OnDirmenuSetashomedir();
}

BOOL CUsersDlgSharedFolders::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message==WM_KEYDOWN)
	{
		if (pMsg->wParam==VK_F2)
		{
			if (GetFocus() == &m_cDirs)
			{
				if (m_cDirs.GetEditControl())
					return TRUE;
				OnDirmenuRename();
			}
			return TRUE;
		}
	}
	return CSAPrefsSubDlg::PreTranslateMessage(pMsg);
}

BOOL CUsersDlgSharedFolders::DisplayUser(t_user *pUser)
{
	if (!pUser)
	{
		m_cDirs.DeleteAllItems();
		m_bFilesRead = m_bFilesWrite = m_bFilesDelete = m_bFilesAppend = FALSE;
		m_bDirsCreate = m_bDirsList = m_bDirsDelete = m_bDirsSubdirs = FALSE;
		return TRUE;
	}

	UpdateData(FALSE);

	//Fill the dirs list
	m_cDirs.DeleteAllItems();
	for (unsigned int j=0; j<pUser->permissions.size(); j++)
	{
		int nItem = m_cDirs.InsertItem(j, pUser->permissions[j].dir);
		LVITEM item;
		memset(&item,0,sizeof(item));
		item.mask = LVIF_IMAGE | LVIF_PARAM;
		item.iItem = nItem;
		m_cDirs.GetItem(&item);
		item.lParam = j;
		item.iImage = pUser->permissions[j].bIsHome?1:0;
		m_cDirs.SetItem(&item);

		CString aliases;
		for (auto const& alias : pUser->permissions[j].aliases) {
			aliases += alias + "|";
		}
		aliases.TrimRight('|');
		m_cDirs.SetItemText(nItem, 1, aliases);
	}

	return TRUE;
}

BOOL CUsersDlgSharedFolders::SaveUser(t_user & user)
{
	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (selpos) {
		int item = m_cDirs.GetNextSelectedItem(selpos);
		int index = m_cDirs.GetItemData(item);
		user.permissions[index].bFileRead = m_bFilesRead;
		user.permissions[index].bFileWrite = m_bFilesWrite;
		user.permissions[index].bFileDelete = m_bFilesDelete;
		user.permissions[index].bFileAppend = m_bFilesAppend;
		user.permissions[index].bDirCreate = m_bDirsCreate;
		user.permissions[index].bDirDelete = m_bDirsDelete;
		user.permissions[index].bDirList = m_bDirsList;
		user.permissions[index].bDirSubdirs = m_bDirsSubdirs;
	}
	return TRUE;
}

void CUsersDlgSharedFolders::OnDirmenuEditAliases()
{
	t_user *pUser = m_pOwner->GetCurrentUser();
	if (!pUser)
		return;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);
	int index = m_cDirs.GetItemData(nItem);

	if (pUser->permissions[index].bIsHome)
	{
		AfxMessageBox(_T("Can't set aliases for home dir, this would create a recursive directory structure."));
		return;
	}

	CString aliases = m_cDirs.GetItemText(nItem, 1);
	bool valid = false;
	while( !valid ) {
		CEnterSomething dlg(IDS_SHAREDFOLDERS_ENTERALIASES, IDD_ENTERSOMETHING_LARGE);
		dlg.m_String = aliases;
		dlg.allowEmpty = true;

		if( dlg.DoModal() != IDOK) {
			return;
		}

		aliases = dlg.m_String;
		aliases.Replace('\\', '/');
		while (aliases.Replace('//', '/'));
		while (aliases.Replace(_T("||"), _T("|")));
		aliases.TrimLeft(_T("|"));
		aliases.TrimRight(_T("|"));

		std::vector<CString> aliasList;
		aliases += _T("|");
		int pos;

		CString error;
		valid = true;

		std::set<CString> seen;
		do {
			pos = aliases.Find(_T("|"));

			CString alias = aliases.Left(pos);
			alias.TrimRight('/');

			if (alias != _T("") && seen.insert(alias).second) {

				aliasList.push_back(alias);

				if (alias.GetLength() < 2 || alias[0] != '/') {
					valid = false;
					error = alias;
				}
			}
			aliases = aliases.Mid(pos + 1);
		} while (pos != -1);

		aliases.Empty();
		for (auto const& alias : aliasList) {
			aliases += alias + _T("|");
		}
		aliases.TrimRight(_T("|"));

		if (valid) {
			pUser->permissions[index].aliases = std::move(aliasList);
			m_cDirs.SetItemText(nItem, 1, aliases);
		}
		else {
			AfxMessageBox(_T("At least one alias is not a full virtual path: ") + error);
		}
	}
}