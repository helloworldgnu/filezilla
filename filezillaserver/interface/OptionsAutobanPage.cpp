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
#include "OptionsDlg.h"
#include "Options.h"
#include "OptionsPage.h"
#include "OptionsAutobanPage.h"
#include "../OptionLimits.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsMisc


COptionsAutobanPage::COptionsAutobanPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsAutobanPage::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptionsAutobanPage)
	m_enable = false;
	m_attempts = _T("5");
	m_time = _T("1");
	m_type = 0;
	//}}AFX_DATA_INIT
}


void COptionsAutobanPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsAutobanPage)
	DDX_Check(pDX, IDC_AUTOBAN, m_enable);
	DDX_Text(pDX, ID_ATTEMPTS, m_attempts);
	DDV_MaxChars(pDX, m_attempts, 3);
	DDX_Text(pDX, ID_BANTIME, m_time);
	DDV_MaxChars(pDX, m_time, 3);
	DDX_Radio(pDX, IDC_BANTYPE1, m_type);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsAutobanPage, COptionsPage)
	//{{AFX_MSG_MAP(COptionsAutobanPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten COptionsAutobanPage

BOOL COptionsAutobanPage::OnInitDialog()
{
	COptionsPage::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

void COptionsAutobanPage::LoadData()
{
	m_enable = m_pOptionsDlg->GetOptionVal(OPTION_AUTOBAN_ENABLE) ? true : false;
	m_attempts.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_AUTOBAN_ATTEMPTS)));
	m_time.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_AUTOBAN_BANTIME)));
	m_type = m_pOptionsDlg->GetOptionVal(OPTION_AUTOBAN_TYPE) ? 1 : 0;
}

void COptionsAutobanPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_AUTOBAN_ENABLE, m_enable ? 1 : 0);
	m_pOptionsDlg->SetOption(OPTION_AUTOBAN_ATTEMPTS, _ttoi(m_attempts));
	m_pOptionsDlg->SetOption(OPTION_AUTOBAN_BANTIME, _ttoi(m_time));
	m_pOptionsDlg->SetOption(OPTION_AUTOBAN_TYPE, (m_type == 0) ? 0 : 1);
}

BOOL COptionsAutobanPage::IsDataValid()
{
	if (!UpdateData(TRUE))
		return FALSE;

	if (!m_enable)
		return TRUE;

	int attempts = _ttoi(m_attempts);
	if (attempts < OPTION_AUTOBAN_ATTEMPTS_MIN || attempts > OPTION_AUTOBAN_ATTEMPTS_MAX)
	{
		CString s;
		s.Format(_T("\"Attempts\" has to be a number between %d and %d."), OPTION_AUTOBAN_ATTEMPTS_MIN, OPTION_AUTOBAN_ATTEMPTS_MAX);
		AfxMessageBox(s);
		return FALSE;
	}

	if (!m_type)
	{
		int time = _ttoi(m_time);
		if (time < 1 || time > 999)
		{
			AfxMessageBox(_T("Ban time has to be a number between 1 and 999"));
			return FALSE;
		}
	}

	return TRUE;
}
\