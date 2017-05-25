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

// GroupsDlgIpFilter.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "FileZilla server.h"
#include "GroupsDlgIpFilter.h"
#include "GroupsDlg.h"
#include "../iputils.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CGroupsDlgIpFilter

CGroupsDlgIpFilter::CGroupsDlgIpFilter(CGroupsDlg* pOwner)
	: CSAPrefsSubDlg(IDD)
{
	m_pOwner = pOwner;

	m_pGroup = 0;
}

CGroupsDlgIpFilter::~CGroupsDlgIpFilter()
{
}


void CGroupsDlgIpFilter::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGroupsDlgIpFilter)
	DDX_Text(pDX, IDC_GROUPS_IPFILTER_ALLOWED, m_AllowedAddresses);
	DDX_Text(pDX, IDC_GROUPS_IPFILTER_DISALLOWED, m_DisallowedAddresses);
	DDV_MaxChars(pDX, m_AllowedAddresses, 32000);
	DDV_MaxChars(pDX, m_DisallowedAddresses, 32000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGroupsDlgIpFilter, CSAPrefsSubDlg)
	//{{AFX_MSG_MAP(CGroupsDlgIpFilter)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CGroupsDlgIpFilter

BOOL CGroupsDlgIpFilter::OnInitDialog()
{
	CSAPrefsSubDlg::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

CString CGroupsDlgIpFilter::Validate()
{
	UpdateData(TRUE);

	//if (!ParseIPFilter(m_DisallowedAddresses)) {
	//	GetDlgItem(IDC_GROUPS_IPFILTER_DISALLOWED)->SetFocus();
	//	return _T("Invalid IP address/range/mask");
	//}

	//if (!ParseIPFilter(m_AllowedAddresses)) {
	//	GetDlgItem(IDC_GROUPS_IPFILTER_ALLOWED)->SetFocus();
	//	return _T("Invalid IP address/range/mask");
	//}

	return _T("");
}

void CGroupsDlgIpFilter::SetCtrlState()
{
	if (!m_pOwner->GetCurrentGroup())
	{
		GetDlgItem(IDC_GROUPS_IPFILTER_ALLOWED)->EnableWindow(FALSE);
		GetDlgItem(IDC_GROUPS_IPFILTER_DISALLOWED)->EnableWindow(FALSE);
	}
	else
	{
		GetDlgItem(IDC_GROUPS_IPFILTER_ALLOWED)->EnableWindow(TRUE);
		GetDlgItem(IDC_GROUPS_IPFILTER_DISALLOWED)->EnableWindow(TRUE);
	}
}

BOOL CGroupsDlgIpFilter::DisplayGroup(t_group *pGroup)
{
	m_pGroup = pGroup;

	m_DisallowedAddresses = _T("");
	m_AllowedAddresses = _T("");

	if (!pGroup)
	{
		UpdateData(FALSE);

		return TRUE;
	}

	for (auto const& disallowedIP : pGroup->disallowedIPs) {
		m_DisallowedAddresses += disallowedIP + "\r\n";
	}
	for (auto const& allowedIP : pGroup->allowedIPs) {
		m_AllowedAddresses += allowedIP + "\r\n";
	}

	UpdateData(FALSE);

	return TRUE;
}

BOOL CGroupsDlgIpFilter::SaveGroup(t_group *pGroup)
{
	if (!pGroup)
		return FALSE;

	UpdateData(TRUE);

	pGroup->disallowedIPs.clear();
	pGroup->allowedIPs.clear();

	//ParseIPFilter(m_DisallowedAddresses, &pGroup->disallowedIPs);
	//ParseIPFilter(m_AllowedAddresses, &pGroup->allowedIPs);

	return TRUE;
}
