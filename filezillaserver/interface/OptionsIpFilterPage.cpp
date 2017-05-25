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

// OptionsIpFilterPage.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "OptionsDlg.h"
#include "OptionsPage.h"
#include "OptionsIpFilterPage.h"
#include "../iputils.h"

#if defined _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsIpFilterPage


COptionsIpFilterPage::COptionsIpFilterPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsIpFilterPage::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptionsIpFilterPage)
	//}}AFX_DATA_INIT
}


void COptionsIpFilterPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsIpFilterPage)
	DDX_Text(pDX, IDC_OPTIONS_IPFILTER_ALLOWED, m_AllowedAddresses);
	DDX_Text(pDX, IDC_OPTIONS_IPFILTER_DISALLOWED, m_DisallowedAddresses);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsIpFilterPage, COptionsPage)
	//{{AFX_MSG_MAP(COptionsIpFilterPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten COptionsIpFilterPage

BOOL COptionsIpFilterPage::OnInitDialog()
{
	COptionsPage::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

BOOL COptionsIpFilterPage::IsDataValid()
{
	if (!UpdateData(TRUE))
		return FALSE;

	//if (!ParseIPFilter(m_DisallowedAddresses))
	//{
	//	GetDlgItem(IDC_OPTIONS_IPFILTER_DISALLOWED)->SetFocus();
	//	AfxMessageBox(_T("Invalid IP address/range/mask"));
	//	return false;
	//}

	//if (!ParseIPFilter(m_AllowedAddresses))
	//{
	//	GetDlgItem(IDC_OPTIONS_IPFILTER_ALLOWED)->SetFocus();
	//	AfxMessageBox(_T("Invalid IP address/range/mask"));
	//	return false;
	//}

	return TRUE;
}

void COptionsIpFilterPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_IPFILTER_ALLOWED, m_AllowedAddresses);
	m_pOptionsDlg->SetOption(OPTION_IPFILTER_DISALLOWED, m_DisallowedAddresses);
}

void COptionsIpFilterPage::LoadData()
{
	m_AllowedAddresses = m_pOptionsDlg->GetOption(OPTION_IPFILTER_ALLOWED);
	m_DisallowedAddresses = m_pOptionsDlg->GetOption(OPTION_IPFILTER_DISALLOWED);
}
