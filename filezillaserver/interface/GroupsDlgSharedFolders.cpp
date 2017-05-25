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

// GroupsDlg.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "FileZilla server.h"
#include "GroupsDlgSharedFolders.h"
#include "GroupsDlg.h"
#include "misc\sbdestination.h"
#include "entersomething.h"

#include <set>

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CGroupsDlgSharedFolders

CGroupsDlgSharedFolders::CGroupsDlgSharedFolders(CGroupsDlg *pOwner)
	: CSAPrefsSubDlg(CGroupsDlgSharedFolders::IDD)
{
	ASSERT(pOwner);
	m_pOwner = pOwner;

	//{{AFX_DATA_INIT(CGroupsDlgSharedFolders)
	m_bDirsCreate = FALSE;
	m_bDirsDelete = FALSE;
	m_bDirsList = FALSE;
	m_bDirsSubdirs = FALSE;
	m_bFilesAppend = FALSE;
	m_bFilesDelete = FALSE;
	m_bFilesRead = FALSE;
	m_bFilesWrite = FALSE;
	m_bAutoCreate = FALSE;
	//}}AFX_DATA_INIT
}

CGroupsDlgSharedFolders::~CGroupsDlgSharedFolders()
{
}


void CGroupsDlgSharedFolders::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGroupsDlgSharedFolders)
	DDX_Control(pDX, IDC_DIRS_CREATE, m_cDirsCreate);
	DDX_Control(pDX, IDC_DIRS_DELETE, m_cDirsDelete);
	DDX_Control(pDX, IDC_DIRS_LIST, m_cDirsList);
	DDX_Control(pDX, IDC_DIRS_SUBDIRS, m_cDirsSubdirs);
	DDX_Control(pDX, IDC_FILES_READ, m_cFilesRead);
	DDX_Control(pDX, IDC_FILES_WRITE, m_cFilesWrite);
	DDX_Control(pDX, IDC_FILES_DELETE, m_cFilesDelete);
	DDX_Control(pDX, IDC_FILES_APPEND, m_cFilesAppend);
	DDX_Control(pDX, IDC_GROUPS_AUTOCREATE, m_cAutoCreate);
	DDX_Control(pDX, IDC_DIRS, m_cDirs);
	DDX_Check(pDX, IDC_DIRS_CREATE, m_bDirsCreate);
	DDX_Check(pDX, IDC_DIRS_DELETE, m_bDirsDelete);
	DDX_Check(pDX, IDC_DIRS_LIST, m_bDirsList);
	DDX_Check(pDX, IDC_DIRS_SUBDIRS, m_bDirsSubdirs);
	DDX_Check(pDX, IDC_FILES_APPEND, m_bFilesAppend);
	DDX_Check(pDX, IDC_FILES_DELETE, m_bFilesDelete);
	DDX_Check(pDX, IDC_FILES_READ, m_bFilesRead);
	DDX_Check(pDX, IDC_FILES_WRITE, m_bFilesWrite);
	DDX_Check(pDX, IDC_GROUPS_AUTOCREATE, m_bAutoCreate);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGroupsDlgSharedFolders, CSAPrefsSubDlg)
	//{{AFX_MSG_MAP(CGroupsDlgSharedFolders)
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
// Behandlungsroutinen für Nachrichten CGroupsDlgSharedFolders

BOOL CGroupsDlgSharedFolders::OnInitDialog()
{
	CSAPrefsSubDlg::OnInitDialog();

	m_cDirs.InsertColumn(0, _T("Directories"), LVCFMT_LEFT, 120);
	m_cDirs.InsertColumn(1, _T("Aliases"), LVCFMT_LEFT, 200);
	UpdateData(FALSE);

	m_imagelist.Create(16, 16, ILC_MASK, 3, 3);
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
				  // EXCEPTION: OCX-Propertypages should return FALSE
}

CString CGroupsDlgSharedFolders::Validate()
{
	//TODO: check for homedir

	UpdateData(TRUE);
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return _T("");

	for (auto & permission : pGroup->permissions) {
		if (permission.dir == _T("") || permission.dir == _T("/") || permission.dir == _T("\\")) {
			m_cDirs.SetFocus();
			return _T("At least one shared directory is not a valid local path.");
		}

		if (permission.bIsHome) {
			permission.aliases.clear();
		}
	}

	return _T("");
}

void CGroupsDlgSharedFolders::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (pWnd == &m_cDirs)
	{
		CMenu menu;
		menu.LoadMenu(IDR_DIRCONTEXT);

		CMenu* pPopup = menu.GetSubMenu(0);
		ASSERT(pPopup != NULL);
		CWnd* pWndPopupOwner = this;

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

void CGroupsDlgSharedFolders::OnItemchangedDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;

	int nItem = pNMListView->iItem;
	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (selpos)
	{
		if (m_cDirs.GetNextSelectedItem(selpos)!=nItem)
			return;
	}
	int index = pNMListView->lParam;
	if (nItem != -1)
	{
		m_bFilesRead = pGroup->permissions[index].bFileRead;
		m_bFilesWrite = pGroup->permissions[index].bFileWrite;
		m_bFilesDelete = pGroup->permissions[index].bFileDelete;
		m_bFilesAppend = pGroup->permissions[index].bFileAppend;
		m_bDirsCreate = pGroup->permissions[index].bDirCreate;
		m_bDirsDelete = pGroup->permissions[index].bDirDelete;
		m_bDirsList = pGroup->permissions[index].bDirList;
		m_bDirsSubdirs = pGroup->permissions[index].bDirSubdirs;
		m_bAutoCreate = pGroup->permissions[index].bAutoCreate;
	}
	UpdateData(FALSE);

	SetCtrlState();
	*pResult = 0;
}

void CGroupsDlgSharedFolders::OnItemchangingDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	UpdateData(TRUE);

	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;


	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (selpos)
	{
		int item = m_cDirs.GetNextSelectedItem(selpos);
		int index = m_cDirs.GetItemData(item);
		pGroup->permissions[index].bFileRead = m_bFilesRead;
		pGroup->permissions[index].bFileWrite = m_bFilesWrite;
		pGroup->permissions[index].bFileDelete = m_bFilesDelete;
		pGroup->permissions[index].bFileAppend = m_bFilesAppend;
		pGroup->permissions[index].bDirCreate = m_bDirsCreate;
		pGroup->permissions[index].bDirDelete = m_bDirsDelete;
		pGroup->permissions[index].bDirList = m_bDirsList;
		pGroup->permissions[index].bDirSubdirs = m_bDirsSubdirs;
		pGroup->permissions[index].bAutoCreate = m_bAutoCreate;
	}
}

void CGroupsDlgSharedFolders::SetCtrlState()
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
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
		m_cAutoCreate.EnableWindow(FALSE);
		GetDlgItem(IDC_DIRADD)->EnableWindow(FALSE);
		GetDlgItem(IDC_DIRREMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_DIRRENAME)->EnableWindow(FALSE);
		GetDlgItem(IDC_DIRSETASHOME)->EnableWindow(FALSE);

		m_bFilesAppend = m_bFilesDelete = m_bFilesRead = m_bFilesWrite = FALSE;
		m_bDirsCreate = m_bDirsDelete = m_bDirsList = m_bDirsSubdirs = FALSE;
		m_bAutoCreate = FALSE;
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
			m_cAutoCreate.EnableWindow(TRUE);
			GetDlgItem(IDC_DIRREMOVE)->EnableWindow(TRUE);
			GetDlgItem(IDC_DIRRENAME)->EnableWindow(TRUE);
			GetDlgItem(IDC_DIRSETASHOME)->EnableWindow(TRUE);
		}
		else
		{
			m_bFilesAppend = m_bFilesDelete = m_bFilesRead = m_bFilesWrite = FALSE;
			m_bDirsCreate = m_bDirsDelete = m_bDirsList = m_bDirsSubdirs = FALSE;
			m_bAutoCreate = FALSE;
			UpdateData(FALSE);
			m_cFilesRead.EnableWindow(FALSE);
			m_cFilesWrite.EnableWindow(FALSE);
			m_cFilesDelete.EnableWindow(FALSE);
			m_cFilesAppend.EnableWindow(FALSE);
			m_cDirsCreate.EnableWindow(FALSE);
			m_cDirsDelete.EnableWindow(FALSE);
			m_cDirsList.EnableWindow(FALSE);
			m_cDirsSubdirs.EnableWindow(FALSE);
			m_cAutoCreate.EnableWindow(FALSE);
			GetDlgItem(IDC_DIRREMOVE)->EnableWindow(FALSE);
			GetDlgItem(IDC_DIRRENAME)->EnableWindow(FALSE);
			GetDlgItem(IDC_DIRSETASHOME)->EnableWindow(FALSE);
		}
	}
}

void CGroupsDlgSharedFolders::OnDirmenuAdd()
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;

	t_directory dir;
	dir.bFileRead = dir.bDirList = dir.bDirSubdirs = TRUE;
	dir.bDirCreate = dir.bDirDelete =
		dir.bFileAppend = dir.bFileDelete =
		dir.bFileWrite = dir.bIsHome =
		dir.bAutoCreate = FALSE;
	dir.dir = _T("");
	dir.bIsHome = m_cDirs.GetItemCount()?FALSE:TRUE;

	pGroup->permissions.push_back(dir);
	int nItem = m_cDirs.InsertItem(LVIF_TEXT |LVIF_PARAM|LVIF_IMAGE, 0, _T("<new directory>"), 0, 0, dir.bIsHome?1:0, pGroup->permissions.size()-1);
	m_cDirs.SetItemState(nItem, LVIS_SELECTED,LVIS_SELECTED);
	m_cDirs.SetItemState(nItem, LVIS_SELECTED,LVIS_SELECTED);
	OnDblclkDirs(0, 0);
}

void CGroupsDlgSharedFolders::OnDirmenuRemove()
{
t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;

	POSITION selpos;
	selpos=m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem=m_cDirs.GetNextSelectedItem(selpos);
	int index=m_cDirs.GetItemData(nItem);
	m_cDirs.DeleteItem(nItem);
	int i=0;
	for (std::vector<t_directory>::iterator iter=pGroup->permissions.begin(); iter != pGroup->permissions.end(); iter++, i++)
		if (i==index)
		{
			pGroup->permissions.erase(iter);
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

void CGroupsDlgSharedFolders::OnDirmenuRename()
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);

	m_cDirs.SetFocus();
	m_cDirs.EditLabel(nItem);
}

void CGroupsDlgSharedFolders::OnDirmenuSetashomedir()
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;

	POSITION selpos;
	selpos=m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem=m_cDirs.GetNextSelectedItem(selpos);

	for (unsigned int j=0; j<pGroup->permissions.size(); j++)
	{
		LVITEM item;
		memset(&item,0,sizeof(item));
		item.mask=LVIF_IMAGE|LVIF_PARAM;
		item.iItem=j;
		m_cDirs.GetItem(&item);
		item.iImage = (j==(unsigned int)nItem)?1:0;
		pGroup->permissions[item.lParam].bIsHome=0;
		m_cDirs.SetItem(&item);
	}
	int index = m_cDirs.GetItemData(nItem);
	pGroup->permissions[index].bIsHome = 1;
}

void CGroupsDlgSharedFolders::OnEndlabeleditDirs(NMHDR* pNMHDR, LRESULT* pResult)
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
			t_group *pGroup = m_pOwner->GetCurrentGroup();
			if (!pGroup)
				return;

			pGroup->permissions[pDispInfo->item.lParam].dir = pDispInfo->item.pszText;
			*pResult = TRUE;
		}

	}
	else
	{
		if (m_cDirs.GetItemText(pDispInfo->item.iItem,0) == _T(""))
		{
			t_group *pGroup = m_pOwner->GetCurrentGroup();
			if (!pGroup)
				return;

			m_cDirs.DeleteItem(pDispInfo->item.iItem);
			int i=0;
			for (std::vector<t_directory>::iterator iter=pGroup->permissions.begin(); iter!=pGroup->permissions.end(); iter++, i++)
				if (i==pDispInfo->item.lParam)
				{
					pGroup->permissions.erase(iter);
					break;
				}
		}
	}
}

void CGroupsDlgSharedFolders::OnDblclkDirs(NMHDR* pNMHDR, LRESULT* pResult)
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
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
			sb.SetInitialSelection(m_cDirs.GetItemText(nItem,0));
			if (sb.SelectFolder())
			{
				m_cDirs.SetItemText(nItem, 0, sb.GetSelectedFolder());
				pGroup->permissions[index].dir = sb.GetSelectedFolder();
			}
		}
		else
		{
			m_cDirs.SetFocus();
			m_cDirs.EditLabel(nItem);
		}
	}
	else
		OnDirmenuEditAliases();

	if (pResult)
		*pResult = 0;
}

void CGroupsDlgSharedFolders::OnFilesWrite()
{
	UpdateData(TRUE);
	SetCtrlState();
}

void CGroupsDlgSharedFolders::OnDiradd()
{
	OnDirmenuAdd();
}

void CGroupsDlgSharedFolders::OnDirremove()
{
	OnDirmenuRemove();
}

void CGroupsDlgSharedFolders::OnDirrename()
{
	OnDirmenuRename();
}

void CGroupsDlgSharedFolders::OnDirsetashome()
{
	OnDirmenuSetashomedir();
}

BOOL CGroupsDlgSharedFolders::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_F2)
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

BOOL CGroupsDlgSharedFolders::DisplayGroup(const t_group *pGroup)
{
	if (!pGroup)
	{
		m_cDirs.DeleteAllItems();
		m_bFilesRead = m_bFilesWrite = m_bFilesDelete = m_bFilesAppend = FALSE;
		m_bDirsCreate = m_bDirsList = m_bDirsDelete = m_bDirsSubdirs = FALSE;
		m_bAutoCreate = FALSE;
		return TRUE;
	}

	UpdateData(FALSE);

	//Fill the dirs list
	m_cDirs.DeleteAllItems();
	for (unsigned int j = 0; j < pGroup->permissions.size(); j++)
	{
		int nItem = m_cDirs.InsertItem(j, pGroup->permissions[j].dir);
		LVITEM item;
		memset(&item, 0, sizeof(item));
		item.mask=LVIF_IMAGE|LVIF_PARAM;
		item.iItem = nItem;
		m_cDirs.GetItem(&item);
		item.lParam=j;
		item.iImage = pGroup->permissions[j].bIsHome?1:0;
		m_cDirs.SetItem(&item);

		CString aliases;
		for (auto const& alias : pGroup->permissions[j].aliases) {
			aliases += alias;
			aliases += _T("|");
		}
		aliases.TrimRight('|');
		m_cDirs.SetItemText(nItem, 1, aliases);
	}

	return TRUE;
}

BOOL CGroupsDlgSharedFolders::SaveGroup(t_group *pGroup)
{
	if (!pGroup)
		return FALSE;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (selpos)
	{
		int item = m_cDirs.GetNextSelectedItem(selpos);
		int index = m_cDirs.GetItemData(item);
		pGroup->permissions[index].bFileRead = m_bFilesRead;
		pGroup->permissions[index].bFileWrite = m_bFilesWrite;
		pGroup->permissions[index].bFileDelete = m_bFilesDelete;
		pGroup->permissions[index].bFileAppend = m_bFilesAppend;
		pGroup->permissions[index].bDirCreate = m_bDirsCreate;
		pGroup->permissions[index].bDirDelete = m_bDirsDelete;
		pGroup->permissions[index].bDirList = m_bDirsList;
		pGroup->permissions[index].bDirSubdirs = m_bDirsSubdirs;
		pGroup->permissions[index].bAutoCreate = m_bAutoCreate;
	}
	return TRUE;
}

void CGroupsDlgSharedFolders::OnDirmenuEditAliases()
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
		return;

	POSITION selpos = m_cDirs.GetFirstSelectedItemPosition();
	if (!selpos)
		return;
	int nItem = m_cDirs.GetNextSelectedItem(selpos);
	int index = m_cDirs.GetItemData(nItem);

	if (pGroup->permissions[index].bIsHome)
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

			if (alias != _T("") && seen.insert(alias).second ) {

				aliasList.push_back(alias);

				if( alias.GetLength() < 2 || alias[0] != '/' ) {
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
			pGroup->permissions[index].aliases = std::move(aliasList);
			m_cDirs.SetItemText(nItem, 1, aliases);
		}
		else {
			AfxMessageBox(_T("At least one alias is not a full virtual path: ") + error);
		}
	}
}