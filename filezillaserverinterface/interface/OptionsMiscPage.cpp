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

// OptionsMisc.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "OptionsDlg.h"
#include "Options.h"	
#include "OptionsPage.h"
#include "OptionsMiscPage.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsMisc


COptionsMiscPage::COptionsMiscPage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsMiscPage::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptionsMiscPage)
	m_bDontShowPass = FALSE;
	m_bStartMinimized = FALSE;
	m_TransferBufferSize = _T("");
	m_bActiveIgnoreLocal = FALSE;
	//}}AFX_DATA_INIT
}


void COptionsMiscPage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsMiscPage)
	DDX_Check(pDX, IDC_OPTIONS_MISC_DONTSHOWPASS, m_bDontShowPass);
	DDX_Check(pDX, IDC_OPTIONS_MISC_STARTMINIMIZED, m_bStartMinimized);
	DDX_Text(pDX, IDC_OPTIONS_TRANSFERBUFFERSIZE, m_TransferBufferSize);
	DDV_MaxChars(pDX, m_TransferBufferSize, 6);
	DDX_Text(pDX, IDC_OPTIONS_TRANSFERBUFFERSIZE2, m_TransferBufferSize2);
	DDV_MaxChars(pDX, m_TransferBufferSize2, 8);
	DDX_Check(pDX, IDC_SHAREWRITE, m_bSharedWrite);
	DDX_Check(pDX, IDC_ACTIVE_IGNORELOCAL, m_bActiveIgnoreLocal);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsMiscPage, COptionsPage)
	//{{AFX_MSG_MAP(COptionsMiscPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten COptionsMiscPage

BOOL COptionsMiscPage::OnInitDialog()
{
	COptionsPage::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

void COptionsMiscPage::LoadData()
{
	m_bDontShowPass = m_pOptionsDlg->GetOptionVal(OPTION_LOGSHOWPASS) == 0;
	m_bStartMinimized = m_pOptionsDlg->m_pInterfaceOptions->GetOptionVal(IOPTION_STARTMINIMIZED) != 0;
	m_TransferBufferSize.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_BUFFERSIZE)));
	m_TransferBufferSize2.Format(_T("%d"), static_cast<int>(m_pOptionsDlg->GetOptionVal(OPTION_BUFFERSIZE2)));
	m_bSharedWrite = m_pOptionsDlg->GetOptionVal(OPTION_SHAREDWRITE) != 0;
	m_bActiveIgnoreLocal = m_pOptionsDlg->GetOptionVal(OPTION_ACTIVE_IGNORELOCAL) != 0;
}

void COptionsMiscPage::SaveData()
{
	m_pOptionsDlg->SetOption(OPTION_LOGSHOWPASS, m_bDontShowPass?0:1);
	m_pOptionsDlg->m_pInterfaceOptions->SetOption(IOPTION_STARTMINIMIZED, m_bStartMinimized);
	m_pOptionsDlg->SetOption(OPTION_BUFFERSIZE, _ttoi(m_TransferBufferSize));
	m_pOptionsDlg->SetOption(OPTION_BUFFERSIZE2, _ttoi(m_TransferBufferSize2));
	m_pOptionsDlg->SetOption(OPTION_SHAREDWRITE, m_bSharedWrite ? 1 : 0);
	m_pOptionsDlg->SetOption(OPTION_ACTIVE_IGNORELOCAL, m_bActiveIgnoreLocal ? 1 : 0);
}
