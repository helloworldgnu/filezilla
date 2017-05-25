// FileZilla Server - a Windows ftp server

// Copyright (C) 2004 - Tim Kosse <tim.kosse@filezilla-project.org>

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
#include "OptionsDlg.h"
#include "OptionsPage.h"
#include "OptionsGeneralIpbindingsPage.h"

COptionsGeneralIpbindingsPage::COptionsGeneralIpbindingsPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsGeneralIpbindingsPage::IDD, pParent)
{
}

COptionsGeneralIpbindingsPage::~COptionsGeneralIpbindingsPage()
{
}

void COptionsGeneralIpbindingsPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_OPTIONS_GENERAL_IPBINDUNGS_IPBINDINGS, m_bindings);
}

BEGIN_MESSAGE_MAP(COptionsGeneralIpbindingsPage, COptionsPage)
END_MESSAGE_MAP()

BOOL COptionsGeneralIpbindingsPage::IsDataValid()
{
	return true;
}
void COptionsGeneralIpbindingsPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_IPBINDINGS, m_bindings);
}

void COptionsGeneralIpbindingsPage::LoadData()
{
	m_bindings = m_pOptionsDlg->GetOption(OPTION_IPBINDINGS);
}
