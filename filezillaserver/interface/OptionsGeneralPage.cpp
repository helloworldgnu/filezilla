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

// OptionsGeneralPage.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "OptionsDlg.h"
#include "OptionsPage.h"
#include "OptionsGeneralPage.h"
#include <set>

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsGeneralPage


COptionsGeneralPage::COptionsGeneralPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsGeneralPage::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptionsGeneralPage)
	m_MaxUsers = _T("");
	m_Port = _T("");
	m_Threadnum = _T("");
	m_Timeout = _T("");
	m_NoTransferTimeout = _T("");
	m_LoginTimeout = _T("");
	//}}AFX_DATA_INIT
}


void COptionsGeneralPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsGeneralPage)
	DDX_Text(pDX, IDC_MAXUSERS, m_MaxUsers);
	DDV_MaxChars(pDX, m_MaxUsers, 9);
	DDX_Text(pDX, IDC_PORT, m_Port);
	DDX_Text(pDX, IDC_THREADNUM, m_Threadnum);
	DDV_MaxChars(pDX, m_Threadnum, 2);
	DDX_Text(pDX, IDC_TIMEOUT, m_Timeout);
	DDV_MaxChars(pDX, m_Timeout, 4);
	DDX_Text(pDX, IDC_TRANSFERTIMEOUT, m_NoTransferTimeout);
	DDV_MaxChars(pDX, m_NoTransferTimeout, 4);
	DDX_Text(pDX, IDC_LOGINTIMEOUT, m_LoginTimeout);
	DDV_MaxChars(pDX, m_LoginTimeout, 4);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsGeneralPage, COptionsPage)
	//{{AFX_MSG_MAP(COptionsGeneralPage)
		// HINWEIS: Der Klassen-Assistent fügt hier Zuordnungsmakros für Nachrichten ein
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten COptionsGeneralPage

BOOL COptionsGeneralPage::IsDataValid()
{
	if (!UpdateData(TRUE))
		return FALSE;

	std::set<int> portSet;
	bool valid = true;

	CString ports = m_Port;
	ports.TrimLeft(_T(" ,"));

	int pos = ports.FindOneOf(_T(" ,"));
	while (pos != -1 && valid)
	{
		int port = _ttoi(ports.Left(pos));
		if (port < 1 || port > 65535)
		{
			valid = false;
			break;
		}
		else
			portSet.insert(port);
		ports = ports.Mid(pos + 1);
		ports.TrimLeft(_T(" ,"));
		pos = ports.FindOneOf(_T(" ,"));
	}
	if (valid && ports != _T(""))
	{
		int port = _ttoi(ports);
		if (port < 1 || port > 65535)
			valid = false;
		else
			portSet.insert(port);
	}

	if (!valid)
	{
		m_pOptionsDlg->ShowPage(this);
		GetDlgItem(IDC_PORT)->SetFocus();
		AfxMessageBox(_T("Invalid port found, please only enter ports in the range from 1 to 65535."));
		return FALSE;
	}
	int threadnum = _ttoi(m_Threadnum);
	if (threadnum < 1 || threadnum > 50)
	{
		m_pOptionsDlg->ShowPage(this);
		GetDlgItem(IDC_THREADNUM)->SetFocus();
		AfxMessageBox(_T("Please enter a value between 1 and 50 for the number of Threads!"));
		return FALSE;
	}

	m_Port = _T("");
	for (std::set<int>::const_iterator iter = portSet.begin(); iter != portSet.end(); ++iter)
	{
		CString tmp;
		tmp.Format(_T("%d "), *iter);
		m_Port += tmp;
	}
	m_Port.TrimRight(' ');
	UpdateData(false);

	return TRUE;
}

void COptionsGeneralPage::LoadData()
{
	m_Port = m_pOptionsDlg->GetOption(OPTION_SERVERPORT);
	m_Threadnum.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_THREADNUM)));
	m_MaxUsers.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_MAXUSERS)));
	m_Timeout.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_TIMEOUT)));
	m_NoTransferTimeout.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_NOTRANSFERTIMEOUT)));
	m_LoginTimeout.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_LOGINTIMEOUT)));
}

void COptionsGeneralPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_SERVERPORT, m_Port);
	m_pOptionsDlg->SetOption(OPTION_THREADNUM, _ttoi(m_Threadnum));
	m_pOptionsDlg->SetOption(OPTION_MAXUSERS, _ttoi(m_MaxUsers));
	m_pOptionsDlg->SetOption(OPTION_TIMEOUT, _ttoi(m_Timeout));
	m_pOptionsDlg->SetOption(OPTION_NOTRANSFERTIMEOUT, _ttoi(m_NoTransferTimeout));
	m_pOptionsDlg->SetOption(OPTION_LOGINTIMEOUT, _ttoi(m_LoginTimeout));
}