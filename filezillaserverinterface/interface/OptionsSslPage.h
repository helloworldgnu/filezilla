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

#pragma once
#include "afxwin.h"

class COptionsSslPage : public COptionsPage
{
public:
	COptionsSslPage(COptionsDlg *pOptionsDlg, CWnd* pParent = NULL);
	virtual ~COptionsSslPage();

	enum { IDD = IDD_OPTIONS_SSL };

	virtual BOOL IsDataValid();
	virtual void SaveData();
	virtual void LoadData();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
protected:
	BOOL m_enabled{};
	BOOL m_allowExplicit{};
	BOOL m_forceExplicit{};
	BOOL m_forceProtP{};
	CString m_certificate;
	CString m_key;
	CString m_pass;
	CString m_sslports;
	CButton m_cAllowExplicit;
	CButton m_cForceExplicit;
	CButton m_cForceProtP;
	CEdit m_cCertificate;
	CButton m_cCertificateBrowse;
	BOOL m_require_resumption{};
	CButton m_cRequireResumption;

	CEdit m_cKey;
	CEdit m_cPass;
	CButton m_cKeyBrowse;
	CEdit m_cSslports;

	afx_msg void OnGenerate();
	afx_msg void OnKeyBrowse();
	afx_msg void OnCertificateBrowse();
	afx_msg void OnEnableSsl();
	virtual BOOL OnInitDialog();
};
