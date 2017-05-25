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

// UsersDlgGeneral.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "FileZilla server.h"
#include "UsersDlgGeneral.h"
#include "entersomething.h"
#include "UsersDlg.h"
#include "UsersDlgSpeedLimit.h"
#include "../AsyncSslSocketLayer.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld CUsersDlgGeneral

CUsersDlgGeneral::CUsersDlgGeneral(CUsersDlg* pOwner)
	: CSAPrefsSubDlg(IDD)
	, m_Comments(_T(""))
{
	m_pOwner = pOwner;

	//{{AFX_DATA_INIT(CUsersDlgGeneral)
	m_bNeedpass = FALSE;
	m_Pass = _T("");
	m_nMaxUsersBypass = FALSE;
	m_MaxConnCount = _T("");
	m_IpLimit = _T("");
	m_nEnabled = 0;
	//}}AFX_DATA_INIT

	m_pUser = 0;
}

CUsersDlgGeneral::~CUsersDlgGeneral()
{
}


void CUsersDlgGeneral::DoDataExchange(CDataExchange* pDX)
{
	CSAPrefsSubDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUsersDlgGeneral)
	DDX_Control(pDX, IDC_MAXCONNCOUNT, m_cMaxConnCount);
	DDX_Control(pDX, IDC_MAXUSERBYPASS, m_cMaxUsersBypass);
	DDX_Control(pDX, IDC_NEEDPASS, m_cNeedpass);
	DDX_Control(pDX, IDC_GROUP, m_cGroup);
	DDX_Control(pDX, IDC_PASS, m_cPass);
	DDX_Control(pDX, IDC_IPLIMIT, m_cIpLimit);
	DDX_Check(pDX, IDC_NEEDPASS, m_bNeedpass);
	DDX_Text(pDX, IDC_PASS, m_Pass);
	DDX_Check(pDX, IDC_MAXUSERBYPASS, m_nMaxUsersBypass);
	DDX_Text(pDX, IDC_MAXCONNCOUNT, m_MaxConnCount);
	DDV_MaxChars(pDX, m_MaxConnCount, 9);
	DDX_Text(pDX, IDC_IPLIMIT, m_IpLimit);
	DDV_MaxChars(pDX, m_IpLimit, 9);
	DDX_Control(pDX, IDC_USERS_GENERAL_ENABLE, m_cEnabled);
	DDX_Check(pDX, IDC_USERS_GENERAL_ENABLE, m_nEnabled);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_USERS_GENERAL_COMMENTS, m_cComments);
	DDX_Text(pDX, IDC_USERS_GENERAL_COMMENTS, m_Comments);
	DDV_MaxChars(pDX, m_Comments, 20000);
	DDX_Control(pDX, IDC_FORCESSL, m_cForceSsl);
	DDX_Check(pDX, IDC_FORCESSL, m_nForceSsl);
}


BEGIN_MESSAGE_MAP(CUsersDlgGeneral, CSAPrefsSubDlg)
	//{{AFX_MSG_MAP(CUsersDlgGeneral)
	ON_BN_CLICKED(IDC_NEEDPASS, OnNeedpass)
	ON_CBN_SELCHANGE(IDC_GROUP, OnSelchangeGroup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CUsersDlgGeneral

BOOL CUsersDlgGeneral::OnInitDialog()
{
	CSAPrefsSubDlg::OnInitDialog();

	m_bNeedpass = FALSE;
	m_Pass = _T("");
	UpdateData(FALSE);

	m_cGroup.AddString(_T("<none>"));
	for (CUsersDlg::t_GroupsList::iterator iter = m_pOwner->m_GroupsList.begin(); iter != m_pOwner->m_GroupsList.end(); ++iter)
		m_cGroup.AddString(iter->group);

	SetCtrlState();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zurückgeben
}

void CUsersDlgGeneral::OnNeedpass()
{
	UpdateData(TRUE);
	m_cPass.EnableWindow(m_bNeedpass);
}

CString CUsersDlgGeneral::Validate()
{
	UpdateData(TRUE);
	if (m_bNeedpass && m_Pass == _T(""))
	{
		m_cPass.SetFocus();
		return _T("Empty passwords are not allowed. Please enter a password!");
	}
	if (_ttoi(m_MaxConnCount) < 0 || _ttoi(m_MaxConnCount) > 999999999)
	{
		m_cMaxConnCount.SetFocus();
		return _T("The maximum user count has to be between 0 and 999999999!");
	}
	if (_ttoi(m_MaxConnCount) < 0 || _ttoi(m_MaxConnCount) > 999999999)
	{
		m_cIpLimit.SetFocus();
		return _T("The maximum user limit per IP has to be between 0 and 999999999!");
	}
	return _T("");
}

void CUsersDlgGeneral::SetCtrlState()
{
	if (!m_pOwner->GetCurrentUser())
	{
		m_cEnabled.EnableWindow(FALSE);
		m_cNeedpass.EnableWindow(FALSE);
		m_cPass.EnableWindow(FALSE);
		m_cGroup.EnableWindow(FALSE);
		m_cMaxUsersBypass.EnableWindow(FALSE);
		m_cMaxConnCount.EnableWindow(FALSE);
		m_cIpLimit.EnableWindow(FALSE);
		m_cComments.EnableWindow(FALSE);
		m_cForceSsl.EnableWindow(FALSE);

		m_cGroup.SetCurSel(CB_ERR);

		UpdateData(FALSE);
	}
	else
	{
		m_cEnabled.EnableWindow(TRUE);
		m_cNeedpass.EnableWindow(TRUE);
		m_cPass.EnableWindow(TRUE);
		m_cGroup.EnableWindow(TRUE);
		m_cMaxUsersBypass.EnableWindow(TRUE);
		m_cMaxConnCount.EnableWindow(TRUE);
		m_cIpLimit.EnableWindow(TRUE);
		m_cComments.EnableWindow(TRUE);
		m_cForceSsl.EnableWindow(TRUE);

		OnNeedpass();
	}
}

void CUsersDlgGeneral::OnSelchangeGroup()
{
	if (m_cGroup.GetCurSel() <= 0)
	{
		m_pUser->group = _T("");
		UpdateData(TRUE);
		m_pOwner->SetCtrlState();
		if (m_nMaxUsersBypass == 2)
			m_nMaxUsersBypass = 0;
		if (m_nEnabled == 2)
			m_nEnabled = 1;
		UpdateData(FALSE);
		m_cMaxUsersBypass.SetButtonStyle(BS_AUTOCHECKBOX);
		m_cEnabled.SetButtonStyle(BS_AUTOCHECKBOX);

		m_pOwner->m_pSpeedLimitPage->UpdateData(TRUE);
		CButton *pButton = reinterpret_cast<CButton *>(m_pOwner->m_pSpeedLimitPage->GetDlgItem(IDC_USERS_SPEEDLIMIT_SERVERBYPASS_DOWNLOAD));
		if (pButton->GetCheck() == 2)
			pButton->SetCheck(0);
		pButton->SetButtonStyle(BS_AUTOCHECKBOX);

		pButton = reinterpret_cast<CButton *>(m_pOwner->m_pSpeedLimitPage->GetDlgItem(IDC_USERS_SPEEDLIMIT_SERVERBYPASS_DOWNLOAD));
		if (pButton->GetCheck() == 2)
			pButton->SetCheck(0);
		pButton->SetButtonStyle(BS_AUTOCHECKBOX);
		m_pOwner->m_pSpeedLimitPage->UpdateData(FALSE);
	}
	else
	{
		m_cGroup.GetLBText(m_cGroup.GetCurSel(), m_pUser->group);
		m_cMaxUsersBypass.SetButtonStyle(BS_AUTO3STATE);
		m_cEnabled.SetButtonStyle(BS_AUTO3STATE);

		((CButton *)m_pOwner->m_pSpeedLimitPage->GetDlgItem(IDC_USERS_SPEEDLIMIT_SERVERBYPASS_DOWNLOAD))->SetButtonStyle(BS_AUTO3STATE);
		((CButton *)m_pOwner->m_pSpeedLimitPage->GetDlgItem(IDC_USERS_SPEEDLIMIT_SERVERBYPASS_UPLOAD))->SetButtonStyle(BS_AUTO3STATE);
	}
}

BOOL CUsersDlgGeneral::DisplayUser(t_user *pUser)
{
	m_pUser = pUser;

	if (!pUser) {
		m_bNeedpass = FALSE;
		m_Pass = _T("");
		m_nMaxUsersBypass = 0;
		m_IpLimit = _T("");
		m_MaxConnCount = _T("");
		m_Comments = _T("");
		m_nForceSsl = 0;

		UpdateData(FALSE);

		return TRUE;
	}

	m_Pass = pUser->password;
	m_cPass.SetModify(FALSE);
	m_bNeedpass = pUser->password != _T("");

	if (pUser->group == _T("") || m_cGroup.SelectString(-1, pUser->group) == CB_ERR) {
		m_cMaxUsersBypass.SetButtonStyle(BS_AUTOCHECKBOX);
		m_cEnabled.SetButtonStyle(BS_AUTOCHECKBOX);
		m_cGroup.SetCurSel(0);
		m_cForceSsl.SetButtonStyle(BS_AUTOCHECKBOX);
	}
	else {
		m_cMaxUsersBypass.SetButtonStyle(BS_AUTO3STATE);
		m_cEnabled.SetButtonStyle(BS_AUTO3STATE);
		m_cForceSsl.SetButtonStyle(BS_AUTO3STATE);
	}
	m_nEnabled = pUser->nEnabled;
	m_nMaxUsersBypass = pUser->nBypassUserLimit;
	CString str;
	str.Format(_T("%d"), pUser->nUserLimit);
	m_MaxConnCount = str;
	str.Format(_T("%d"), pUser->nIpLimit);
	m_IpLimit = str;
	m_Comments = pUser->comment;
	m_nForceSsl = pUser->forceSsl;

	UpdateData(FALSE);

	return TRUE;
}

BOOL CUsersDlgGeneral::SaveUser(t_user & user)
{
	user.nEnabled = m_nEnabled;
	if (!m_bNeedpass)
		user.password = _T("");
	else if (m_cPass.GetModify() && m_Pass != _T("")) {
		user.generateSalt();
		
		std::string saltedPassword;
		//auto saltedPassword = ConvToNetwork(m_Pass + user.salt);

		CAsyncSslSocketLayer ssl(0);
		//user.password = ConvFromNetwork(ssl.SHA512(reinterpret_cast<unsigned char const*>(saltedPassword.c_str()), saltedPassword.size()).c_str());
		if (user.password.IsEmpty()) {
			// Something went very wrong, disable user.
			user.nEnabled = false;
		}
	}

	user.nBypassUserLimit = m_nMaxUsersBypass;
	user.nUserLimit = _ttoi(m_MaxConnCount);
	user.nIpLimit = _ttoi(m_IpLimit);
	if (m_cGroup.GetCurSel()<=0)
		user.group = _T("");
	else
		m_cGroup.GetLBText(m_cGroup.GetCurSel(), user.group);

	user.comment = m_Comments;

	user.forceSsl = m_nForceSsl;

	return TRUE;
}
