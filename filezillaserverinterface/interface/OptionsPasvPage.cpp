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

// OptionsPasvPage.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "OptionsDlg.h"
#include "OptionsPage.h"
#include "OptionsPasvPage.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsPasvPage


COptionsPasvPage::COptionsPasvPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsPasvPage::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptionsPasvPage)
	m_URL = _T("");
	m_IP = _T("");
	m_nIPType = -1;
	m_bUseCustomPort = FALSE;
	m_PortMin = _T("");
	m_PortMax = _T("");
	m_Text = CString((LPCTSTR)IDC_OPTIONS_PASV_TEXT);
	//}}AFX_DATA_INIT
}


void COptionsPasvPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsPasvPage)
	DDX_Control(pDX, IDC_OPTIONS_PASV_PORTMIN, m_cPortMin);
	DDX_Control(pDX, IDC_OPTIONS_PASV_PORTMAX, m_cPortMax);
	DDX_Control(pDX, IDC_OPTIONS_PASV_IP, m_cIP);
	DDX_Control(pDX, IDC_OPTIONS_PASV_URL, m_cURL);
	DDX_Text(pDX, IDC_OPTIONS_PASV_URL, m_URL);
	DDX_Text(pDX, IDC_OPTIONS_PASV_IP, m_IP);
	DDX_Radio(pDX, IDC_OPTIONS_PASV_IPTYPE1, m_nIPType);
	DDX_Check(pDX, IDD_OPTIONS_PASV_USEPORTRANGE, m_bUseCustomPort);
	DDX_Text(pDX, IDC_OPTIONS_PASV_PORTMIN, m_PortMin);
	DDV_MaxChars(pDX, m_PortMin, 5);
	DDX_Text(pDX, IDC_OPTIONS_PASV_PORTMAX, m_PortMax);
	DDV_MaxChars(pDX, m_PortMax, 5);
	DDX_Text(pDX, IDC_OPTIONS_PASV_TEXT, m_Text);
	DDX_Check(pDX, IDC_OPTIONS_PASV_NOLOCAL, m_NoExternalOnLocal);
	DDX_Control(pDX, IDC_FTPTEST, m_ftptest);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsPasvPage, COptionsPage)
	//{{AFX_MSG_MAP(COptionsPasvPage)
	ON_BN_CLICKED(IDC_OPTIONS_PASV_IPTYPE2, OnOptionsPasvIptype)
	ON_BN_CLICKED(IDD_OPTIONS_PASV_USEPORTRANGE, OnOptionsPasvUseportrange)
	ON_BN_CLICKED(IDC_OPTIONS_PASV_IPTYPE3, OnOptionsPasvIptype)
	ON_BN_CLICKED(IDC_OPTIONS_PASV_IPTYPE1, OnOptionsPasvIptype)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten COptionsPasvPage

void COptionsPasvPage::OnOptionsPasvIptype()
{
	UpdateData(TRUE);
	m_cIP.EnableWindow(m_nIPType == 1);
	m_cURL.EnableWindow(m_nIPType == 2);
}

void COptionsPasvPage::OnOptionsPasvUseportrange()
{
	UpdateData(TRUE);
	m_cPortMin.EnableWindow(m_bUseCustomPort);
	m_cPortMax.EnableWindow(m_bUseCustomPort);
}

BOOL COptionsPasvPage::OnInitDialog()
{
	COptionsPage::OnInitDialog();

	OnOptionsPasvIptype();
	OnOptionsPasvUseportrange();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

BOOL COptionsPasvPage::IsDataValid()
{
	if (m_nIPType == 1)
	{
		//Ensure a valid IP address has been entered
		if (m_IP == _T(""))
		{
			m_pOptionsDlg->ShowPage(this);
			GetDlgItem(IDC_OPTIONS_PASV_IP)->SetFocus();
			AfxMessageBox(_T("Please enter a valid IP address or hostname!"));
			return FALSE;
		}
	}
	else if (m_nIPType == 2)
	{
		CString address = m_URL;
		if (address.Left(7) == _T("http://"))
			address = address.Mid(7);
		int pos = address.Find('/');
		if (pos != -1)
			address = address.Left(pos);

		if (address == _T("") || address.Find(' ') != -1)
		{
			m_pOptionsDlg->ShowPage(this);
			GetDlgItem(IDC_OPTIONS_PASV_URL)->SetFocus();
			AfxMessageBox(_T("Please enter the complete URL of the IP autodetect server!"));
			return FALSE;
		}
	}

	if (m_bUseCustomPort)
	{
		int nPortMin = _ttoi(m_PortMin);
		int nPortMax = _ttoi(m_PortMax);
		if (nPortMin < 1 || nPortMin > 65535 || nPortMax < 1 || nPortMax > 65535 || nPortMin > nPortMax)
		{
			m_pOptionsDlg->ShowPage(this);
			GetDlgItem(IDC_OPTIONS_PASV_PORTMIN)->SetFocus();
			AfxMessageBox(_T("The port values have to be in the range from 1 to 65535. Also, the first value has to be lower than or equal to the second one."));
			return FALSE;
		}
	}

	return TRUE;
}

void COptionsPasvPage::LoadData()
{
	m_nIPType = (int)m_pOptionsDlg->GetOptionVal(OPTION_CUSTOMPASVIPTYPE);
	m_IP = m_pOptionsDlg->GetOption(OPTION_CUSTOMPASVIP);
	m_URL = m_pOptionsDlg->GetOption(OPTION_CUSTOMPASVIPSERVER);
	m_bUseCustomPort = m_pOptionsDlg->GetOptionVal(OPTION_USECUSTOMPASVPORT) != 0;
	m_PortMin.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_CUSTOMPASVMINPORT)));
	m_PortMax.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_CUSTOMPASVMAXPORT)));
	m_NoExternalOnLocal = m_pOptionsDlg->GetOptionVal(OPTION_NOEXTERNALIPONLOCAL) != 0;
}

void COptionsPasvPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_CUSTOMPASVIPTYPE, m_nIPType);
	m_pOptionsDlg->SetOption(OPTION_CUSTOMPASVIP, m_IP);
	m_pOptionsDlg->SetOption(OPTION_CUSTOMPASVIPSERVER, m_URL);
	m_pOptionsDlg->SetOption(OPTION_USECUSTOMPASVPORT, m_bUseCustomPort);
	m_pOptionsDlg->SetOption(OPTION_CUSTOMPASVMINPORT, _ttoi(m_PortMin));
	m_pOptionsDlg->SetOption(OPTION_CUSTOMPASVMAXPORT, _ttoi(m_PortMax));
	m_pOptionsDlg->SetOption(OPTION_NOEXTERNALIPONLOCAL, m_NoExternalOnLocal ? 1 : 0);
}