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

#if !defined(AFX_OPTIONSPASVPAGE_H__CF8A61D7_93AB_449F_AC3F_8EE3A0F44B87__INCLUDED_)
#define AFX_OPTIONSPASVPAGE_H__CF8A61D7_93AB_449F_AC3F_8EE3A0F44B87__INCLUDED_

#include "misc/hyperlink.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OptionsPasvPage.h : Header-Datei
//

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsPasvPage

class COptionsDlg;
class COptionsPasvPage : public COptionsPage
{
// Konstruktion
public:
	COptionsPasvPage(COptionsDlg *pOptionsDlg, CWnd* pParent = NULL);   // Standardkonstruktor

	virtual BOOL IsDataValid();
	virtual void SaveData();
	virtual void LoadData();

// Dialogfelddaten
	//{{AFX_DATA(COptionsPasvPage)
	enum { IDD = IDD_OPTIONS_PASV };
	CEdit	m_cPortMin;
	CEdit	m_cPortMax;
	CEdit	m_cIP;
	CEdit	m_cURL;
	CString	m_URL;
	CString	m_IP;
	int		m_nIPType;
	BOOL	m_bUseCustomPort;
	CString	m_PortMin;
	CString	m_PortMax;
	CString	m_Text;
	BOOL	m_NoExternalOnLocal;
	CHyperLink m_ftptest;
	//}}AFX_DATA


// Überschreibungen
	// Vom Klassen-Assistenten generierte virtuelle Funktionsüberschreibungen
	//{{AFX_VIRTUAL(COptionsPasvPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV-Unterstützung
	//}}AFX_VIRTUAL

// Implementierung
protected:

	// Generierte Nachrichtenzuordnungsfunktionen
	//{{AFX_MSG(COptionsPasvPage)
	afx_msg void OnOptionsPasvIptype();
	afx_msg void OnOptionsPasvUseportrange();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_OPTIONSPASVPAGE_H__CF8A61D7_93AB_449F_AC3F_8EE3A0F44B87__INCLUDED_
