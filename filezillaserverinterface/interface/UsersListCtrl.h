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

#if !defined(AFX_USERSLISTCTRL_H__C939FF91_7A57_4E36_927B_00B917F6ECED__INCLUDED_)
#define AFX_USERSLISTCTRL_H__C939FF91_7A57_4E36_927B_00B917F6ECED__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// UsersListCtrl.h : Header-Datei
//

/////////////////////////////////////////////////////////////////////////////
// Fenster CUsersListCtrl

class CMainFrame;
class CConnectionData;
class CUsersListCtrl : public CListCtrl
{
// Konstruktion
public:
	CUsersListCtrl(CMainFrame *pOwner);
	virtual ~CUsersListCtrl();

// Attribute
public:

// Operationen
public:
	BOOL ParseUserControlCommand(unsigned char *pData, DWORD dwDataLength);
	void SetDisplayPhysicalNames(bool showPhysical);
	bool GetDisplayPhysicalNames() const { return m_showPhysical; }
	void SetSortColumn(int sortColumn = -1, int sortDir = -1);
	int GetSortColumn() const { return m_sortColumn; }
	int GetSortDirection() const { return m_sortDir; }

// Überschreibungen
	// Vom Klassen-Assistenten generierte virtuelle Funktionsüberschreibungen
	//{{AFX_VIRTUAL(CUsersListCtrl)
	//}}AFX_VIRTUAL

// Implementierung
protected:
	bool ProcessConnOp(unsigned char *pData, DWORD dwDataLength);
	void QSortList(const unsigned int dir, int anf, int ende, int (*comp)(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData));
	static int CmpUserid(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData);
	static int CmpUser(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData);
	static int CmpIP(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData);

	CImageList m_SortImg;
	CImageList m_ImageList;
	CMainFrame *m_pOwner;
	bool m_showPhysical;
	UINT_PTR m_nSpeedinfoTimer;
	int m_sortColumn;
	int m_sortDir;
	std::map<int, CConnectionData*> m_connectionDataMap;
	std::vector<CConnectionData*> m_connectionDataArray;

	// Generierte Nachrichtenzuordnungsfunktionen
protected:
	//{{AFX_MSG(CUsersListCtrl)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnContextmenuKick();
	afx_msg void OnContextmenuBan();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclick(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_USERSLISTCTRL_H__C939FF91_7A57_4E36_927B_00B917F6ECED__INCLUDED_
