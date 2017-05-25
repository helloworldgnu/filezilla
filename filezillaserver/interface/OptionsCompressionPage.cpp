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
#include "OptionsCompressionPage.h"
#include "../iputils.h"

COptionsCompressionPage::COptionsCompressionPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsCompressionPage::IDD, pParent)
{
}

COptionsCompressionPage::~COptionsCompressionPage()
{
}

void COptionsCompressionPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_OPTIONS_COMPRESSION_DISALLOWED_IPS, m_disallowedIPs);
	DDX_Check(pDX, IDC_OPTIONS_COMPRESSION_USE, m_UseModeZ);
	DDX_Text(pDX, IDC_OPTIONS_COMPRESSION_LEVELMAX, m_LevelMax);
	DDV_MaxChars(pDX, m_LevelMax, 1);
	DDX_Text(pDX, IDC_OPTIONS_COMPRESSION_LEVELMIN, m_LevelMin);
	DDV_MaxChars(pDX, m_LevelMin, 1);
	DDX_Check(pDX, IDC_OPTIONS_EXCLUDELOCAL, m_DisallowLocal);
}


BEGIN_MESSAGE_MAP(COptionsCompressionPage, COptionsPage)
END_MESSAGE_MAP()

BOOL COptionsCompressionPage::IsDataValid()
{
	UpdateData();

	if (_ttoi(m_LevelMin) < 1 || _ttoi(m_LevelMin) > 8)
	{
		m_pOptionsDlg->ShowPage(this);
		GetDlgItem(IDC_OPTIONS_COMPRESSION_LEVELMIN)->SetFocus();
		AfxMessageBox(_T("Minimum compression level must be between 1 and 8"));
		return false;
	}

	if (_ttoi(m_LevelMax) < 8 || _ttoi(m_LevelMax) > 9)
	{
		m_pOptionsDlg->ShowPage(this);
		GetDlgItem(IDC_OPTIONS_COMPRESSION_LEVELMAX)->SetFocus();
		AfxMessageBox(_T("Maximum compression level must be between 8 and 9"));
		return false;
	}



	//if (!ParseIPFilter(m_disallowedIPs))
	//{
	//	m_pOptionsDlg->ShowPage(this);
	//	GetDlgItem(IDC_OPTIONS_COMPRESSION_DISALLOWED_IPS)->SetFocus();
	//	AfxMessageBox(_T("Invalid IP address (range/mask) enterd."));
	//	return false;
	//}

	return TRUE;
}

void COptionsCompressionPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_MODEZ_USE, m_UseModeZ);
	m_pOptionsDlg->SetOption(OPTION_MODEZ_LEVELMIN, _ttoi(m_LevelMin));
	m_pOptionsDlg->SetOption(OPTION_MODEZ_LEVELMAX, _ttoi(m_LevelMax));
	m_pOptionsDlg->SetOption(OPTION_MODEZ_ALLOWLOCAL, !m_DisallowLocal);
	m_pOptionsDlg->SetOption(OPTION_MODEZ_DISALLOWED_IPS, m_disallowedIPs);
}

void COptionsCompressionPage::LoadData()
{
	m_LevelMin.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_MODEZ_LEVELMIN)));
	m_LevelMax.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_MODEZ_LEVELMAX)));
	m_UseModeZ = m_pOptionsDlg->GetOptionVal(OPTION_MODEZ_USE) != 0;
	m_DisallowLocal = !m_pOptionsDlg->GetOptionVal(OPTION_MODEZ_ALLOWLOCAL);
	m_disallowedIPs =  m_pOptionsDlg->GetOption(OPTION_MODEZ_DISALLOWED_IPS);
}
