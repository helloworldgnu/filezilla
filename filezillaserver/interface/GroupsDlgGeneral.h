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

#if !defined(AFX_GROUPSDLGGENERAL_H__3548112C_F36E_42D1_B073_78272B897DDA__INCLUDED_)
#define AFX_GROUPSDLGGENERAL_H__3548112C_F36E_42D1_B073_78272B897DDA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Accounts.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CGroupsDlgGeneral
class CGroupsDlg;
class CGroupsDlgGeneral : public CSAPrefsSubDlg
{
// Konstruktion
public:
	CGroupsDlgGeneral(CGroupsDlg *pOwner);   // Standardkonstruktor
	~CGroupsDlgGeneral();

	BOOL DisplayGroup(const t_group *pGroup);
	BOOL SaveGroup(t_group *pGroup);
	void SetCtrlState();
	CString Validate();

protected:
// Dialogfelddaten
	//{{AFX_DATA(CGroupsDlgGeneral)
	enum { IDD = IDD_GROUPS_GENERAL };
	CEdit	m_cMaxConnCount;
	CEdit	m_cIpLimit;
	CButton	m_cMaxUsersBypass;
	int		m_nMaxUsersBypass;
	CString	m_MaxConnCount;
	CString	m_IpLimit;
	CButton	m_cEnabled;
	int		m_nEnabled;
	CButton m_cForceSsl;
	int		m_nForceSsl;
	//}}AFX_DATA


// Überschreibungen
	// Vom Klassen-Assistenten generierte virtuelle Funktionsüberschreibungen
	//{{AFX_VIRTUAL(CGroupsDlgGeneral)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV-Unterstützung
	//}}AFX_VIRTUAL

// Implementierung
protected:
	CGroupsDlg *m_pOwner;
	CImageList m_imagelist;

	// Generierte Nachrichtenzuordnungsfunktionen
	//{{AFX_MSG(CGroupsDlgGeneral)
	virtual BOOL OnInitDialog();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CEdit m_cComments;
	CString m_Comments;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_GROUPSDLGGENERAL_H__3548112C_F36E_42D1_B073_78272B897DDA__INCLUDED_
