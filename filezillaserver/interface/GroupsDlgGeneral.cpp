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

// GroupsDlg.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "FileZilla server.h"
#include "GroupsDlgGeneral.h"
#include "GroupsDlg.h"
#include "entersomething.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CGroupsDlgGeneral

CGroupsDlgGeneral::CGroupsDlgGeneral(CGroupsDlg *pOwner)
	: CSAPrefsSubDlg(CGroupsDlgGeneral::IDD)
	, m_Comments(_T(""))
{
	ASSERT(pOwner);
	m_pOwner = pOwner;

	//{{AFX_DATA_INIT(CGroupsDlgGeneral)
	m_nMaxUsersBypass = FALSE;
	m_MaxConnCount = _T("");
	m_IpLimit = _T("");
	//}}AFX_DATA_INIT
}

CGroupsDlgGeneral::~CGroupsDlgGeneral()
{
}


void CGroupsDlgGeneral::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGroupsDlgGeneral)
	DDX_Control(pDX, IDC_GROUPS_MAXCONNCOUNT, m_cMaxConnCount);
	DDX_Control(pDX, IDC_GROUPS_MAXUSERBYPASS, m_cMaxUsersBypass);
	DDX_Control(pDX, IDC_GROUPS_IPLIMIT, m_cIpLimit);
	DDX_Check(pDX, IDC_GROUPS_MAXUSERBYPASS, m_nMaxUsersBypass);
	DDX_Text(pDX, IDC_GROUPS_MAXCONNCOUNT, m_MaxConnCount);
	DDV_MaxChars(pDX, m_MaxConnCount, 9);
	DDX_Text(pDX, IDC_GROUPS_IPLIMIT, m_IpLimit);
	DDV_MaxChars(pDX, m_IpLimit, 9);
	DDX_Control(pDX, IDC_GROUPS_GENERAL_ENABLE, m_cEnabled);
	DDX_Check(pDX, IDC_GROUPS_GENERAL_ENABLE, m_nEnabled);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_GROUPS_GENERAL_COMMENTS, m_cComments);
	DDX_Text(pDX, IDC_GROUPS_GENERAL_COMMENTS, m_Comments);
	DDV_MaxChars(pDX, m_Comments, 20000);
	DDX_Control(pDX, IDC_FORCESSL, m_cForceSsl);
	DDX_Check(pDX, IDC_FORCESSL, m_nForceSsl);
}


BEGIN_MESSAGE_MAP(CGroupsDlgGeneral, CSAPrefsSubDlg)
	//{{AFX_MSG_MAP(CGroupsDlgGeneral)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CGroupsDlgGeneral

BOOL CGroupsDlgGeneral::OnInitDialog()
{
	CSAPrefsSubDlg::OnInitDialog();

	UpdateData(FALSE);

	SetCtrlState();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Propertypages should return FALSE
}

CString CGroupsDlgGeneral::Validate()
{
	UpdateData(TRUE);

	if (_ttoi(m_MaxConnCount)<0 || _ttoi(m_MaxConnCount)>999999999)
	{
		m_cMaxConnCount.SetFocus();
		return _T("The maximum user count has to be between 0 and 999999999!");
	}
	if (_ttoi(m_MaxConnCount)<0 || _ttoi(m_MaxConnCount)>999999999)
	{
		m_cIpLimit.SetFocus();
		return _T("The maximum user limit per IP has to be between 0 and 999999999!");
	}
	return _T("");
}

void CGroupsDlgGeneral::SetCtrlState()
{
	t_group *pGroup = m_pOwner->GetCurrentGroup();
	if (!pGroup)
	{
		m_cEnabled.EnableWindow(FALSE);
		m_cMaxUsersBypass.EnableWindow(FALSE);
		m_cMaxConnCount.EnableWindow(FALSE);
		m_cIpLimit.EnableWindow(FALSE);
		m_cComments.EnableWindow(FALSE);
		m_cForceSsl.EnableWindow(FALSE);

		UpdateData(FALSE);
	}
	else
	{
		m_cEnabled.EnableWindow(TRUE);
		m_cMaxUsersBypass.EnableWindow(TRUE);
		m_cMaxConnCount.EnableWindow(TRUE);
		m_cIpLimit.EnableWindow(TRUE);
		m_cComments.EnableWindow(TRUE);
		m_cForceSsl.EnableWindow(TRUE);
	}
}

BOOL CGroupsDlgGeneral::DisplayGroup(const t_group *pGroup)
{
	if (!pGroup)
	{
		m_nEnabled = 0;
		m_nMaxUsersBypass = 0;
		m_IpLimit = _T("");
		m_MaxConnCount = _T("");
		m_Comments = _T("");
		m_nForceSsl = 0;

		UpdateData(FALSE);

		return TRUE;
	}

	m_nEnabled = pGroup->nEnabled;
	m_nMaxUsersBypass = pGroup->nBypassUserLimit;
	CString str;
	str.Format(_T("%d"), pGroup->nUserLimit);
	m_MaxConnCount = str;
	str.Format(_T("%d"), pGroup->nIpLimit);
	m_IpLimit = str;
	m_Comments = pGroup->comment;
	m_nForceSsl = pGroup->forceSsl;

	UpdateData(FALSE);

	return TRUE;
}

BOOL CGroupsDlgGeneral::SaveGroup(t_group *pGroup)
{
	if (!pGroup)
		return FALSE;

	pGroup->nEnabled = m_nEnabled;
	pGroup->nBypassUserLimit = m_nMaxUsersBypass;
	pGroup->nUserLimit = _ttoi(m_MaxConnCount);
	pGroup->nIpLimit = _ttoi(m_IpLimit);
	pGroup->comment = m_Comments;
	pGroup->forceSsl = m_nForceSsl;

	return TRUE;
}
