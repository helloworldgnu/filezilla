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

// OptionsSecurityPage.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "OptionsDlg.h"
#include "OptionsPage.h"
#include "OptionsSecurityPage.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsSecurityPage


COptionsSecurityPage::COptionsSecurityPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsSecurityPage::IDD, pParent)
{
}


void COptionsSecurityPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsSecurityPage)
	DDX_Check(pDX, IDC_DATAIPMATCH_EXACT, m_exact);
	DDX_Check(pDX, IDC_DATAIPMATCH_RELAXED, m_relaxed);
	DDX_Check(pDX, IDC_DATAIPMATCH_NONE, m_none);
	//}}AFX_DATA_MAP
}

BOOL COptionsSecurityPage::OnInitDialog()
{
	COptionsPage::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

void COptionsSecurityPage::LoadData()
{
	auto const level = m_pOptionsDlg->GetOptionVal(OPTION_CHECK_DATA_CONNECTION_IP);
	m_relaxed = level == 1;
	m_none = level == 0;
	m_exact = !m_relaxed && !m_none;
}

void COptionsSecurityPage::SaveData()
{
	int level;
	if (m_none) {
		level = 0;
	}
	else if (m_relaxed) {
		level = 1;
	}
	else {
		level = 2;
	}
	m_pOptionsDlg->SetOption(OPTION_CHECK_DATA_CONNECTION_IP, level);
}
