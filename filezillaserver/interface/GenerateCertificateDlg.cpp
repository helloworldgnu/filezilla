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
#include "filezilla server.h"
#include "GenerateCertificateDlg.h"
#include "../AsyncSslSocketLayer.h"
#include ".\generatecertificatedlg.h"

IMPLEMENT_DYNAMIC(CGenerateCertificateDlg, CDialog)
CGenerateCertificateDlg::CGenerateCertificateDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CGenerateCertificateDlg::IDD, pParent)
	, m_city(_T(""))
	, m_cname(_T(""))
	, m_country(_T(""))
	, m_email(_T(""))
	, m_file(_T(""))
	, m_organization(_T(""))
	, m_state(_T(""))
	, m_unit(_T(""))
	, m_keysize(0)
{
}

CGenerateCertificateDlg::~CGenerateCertificateDlg()
{
}

void CGenerateCertificateDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_CITY, m_city);
	DDX_Text(pDX, IDC_CNAME, m_cname);
	DDX_Text(pDX, IDC_COUNTRY, m_country);
	DDV_MaxChars(pDX, m_country, 2);
	DDX_Text(pDX, IDC_EMAIL, m_email);
	DDX_Text(pDX, IDC_FILE, m_file);
	DDX_Text(pDX, IDC_ORGANIZATION, m_organization);
	DDX_Text(pDX, IDC_STATE, m_state);
	DDX_Text(pDX, IDC_UNIT, m_unit);
	DDX_Radio(pDX, IDC_KEYSIZE1, m_keysize);
}


BEGIN_MESSAGE_MAP(CGenerateCertificateDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
END_MESSAGE_MAP()

void CGenerateCertificateDlg::OnOK()
{
	UpdateData(TRUE);
	if (m_country.GetLength() != 2)
	{
		AfxMessageBox(_T("Please enter the 2 digit country code"));
		return;
	}

	if (m_file == _T(""))
	{
		AfxMessageBox(_T("Please enter a filename"));
		return;
	}

	m_country.MakeUpper();

	int bits = 1280;
	if (m_keysize == 1)
		bits = 2048;
	else if (m_keysize == 2)
		bits = 4096;

	//CString error;
	//if (CAsyncSslSocketLayer::CreateSslCertificate(LPCTSTR(m_file), bits,
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_country).c_str()),
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_state).c_str()),
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_city).c_str()),
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_organization).c_str()),
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_unit).c_str()),
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_cname).c_str()),
	//	reinterpret_cast<const unsigned char*>(ConvToNetwork(m_email).c_str()),
	//	error))
	//{
	//	AfxMessageBox(_T("Certificate generated successfully."));
	//	EndDialog(IDOK);
	//}
	//else
	//{
	//	if (error != _T(""))
	//		AfxMessageBox(_T("Certificate could not be generated.\nReason: ") + error);
	//	else
	//		AfxMessageBox(_T("Certificate could not be generated."));
	//}
}

void CGenerateCertificateDlg::OnBrowse()
{
	UpdateData();
	CFileDialog dlg(FALSE, 0, _T("certificate.crt"));
	if (dlg.DoModal() == IDOK)
	{
		m_file = dlg.GetPathName();
		UpdateData(FALSE);
	}
}
