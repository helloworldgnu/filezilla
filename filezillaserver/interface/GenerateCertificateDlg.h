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

class CGenerateCertificateDlg : public CDialog
{
	DECLARE_DYNAMIC(CGenerateCertificateDlg)

public:
	CGenerateCertificateDlg(CWnd* pParent = NULL);
	virtual ~CGenerateCertificateDlg();

	enum { IDD = IDD_CERTGEN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
	CString m_city;
	CString m_cname;
	CString m_country;
	CString m_email;
public:
	CString m_file;
protected:
	CString m_organization;
	CString m_state;
	CString m_unit;
public:
	int m_keysize;
	afx_msg void OnOK();
	afx_msg void OnBrowse();
};
