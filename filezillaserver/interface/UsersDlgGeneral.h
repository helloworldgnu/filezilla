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

#if !defined(AFX_USERSDLGGENERAL_H__5348112C_F36E_42D1_B073_78272B897DDA__INCLUDED_)
#define AFX_USERSDLGGENERAL_H__5348112C_F36E_42D1_B073_78272B897DDA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// UsersDlg.h : Header-Datei
//

#include "UsersDlg.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CUsersDlgGeneral
class CUsersDlgGeneral : public CSAPrefsSubDlg
{
	friend CUsersDlg;
// Konstruktion
public:
	CUsersDlgGeneral(CUsersDlg* pOwner = NULL);   // Standardkonstruktor
	~CUsersDlgGeneral();

	BOOL DisplayUser(t_user *pUser);
	BOOL SaveUser(t_user & user);

protected:
	t_user *m_pUser;

// Dialogfelddaten
	//{{AFX_DATA(CUsersDlgGeneral)
	enum { IDD = IDD_USERS_GENERAL };
	CEdit	m_cMaxConnCount;
	CEdit	m_cIpLimit;
	CButton	m_cMaxUsersBypass;
	CButton	m_cNeedpass;
	CComboBox	m_cGroup;
	CEdit	m_cPass;
	BOOL	m_bNeedpass;
	CString	m_Pass;
	int		m_nMaxUsersBypass;
	CString	m_MaxConnCount;
	CString	m_IpLimit;
	CButton m_cEnabled;
	int		m_nEnabled;
	CButton m_cForceSsl;
	int		m_nForceSsl;
	//}}AFX_DATA


// Überschreibungen
	// Vom Klassen-Assistenten generierte virtuelle Funktionsüberschreibungen
	//{{AFX_VIRTUAL(CUsersDlgGeneral)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV-Unterstützung
	//}}AFX_VIRTUAL

// Implementierung
protected:
	CUsersDlg *m_pOwner;
	CImageList m_imagelist;
	void SetCtrlState();
	CString Validate();

	// Generierte Nachrichtenzuordnungsfunktionen
	//{{AFX_MSG(CUsersDlgGeneral)
	virtual BOOL OnInitDialog();
	afx_msg void OnNeedpass();
	afx_msg void OnSelchangeGroup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CEdit m_cComments;
	CString m_Comments;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_USERSDLGGENERAL_H__5348112C_F36E_42D1_B073_78272B897DDA__INCLUDED_
