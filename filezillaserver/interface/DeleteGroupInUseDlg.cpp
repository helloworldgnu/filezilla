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
#include "FileZilla server.h"
#include "DeleteGroupInUseDlg.h"
#include ".\deletegroupinusedlg.h"

IMPLEMENT_DYNAMIC(CDeleteGroupInUseDlg, CDialog)
CDeleteGroupInUseDlg::CDeleteGroupInUseDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDeleteGroupInUseDlg::IDD, pParent)
	, m_action(0)
{
}

CDeleteGroupInUseDlg::~CDeleteGroupInUseDlg()
{
}

void CDeleteGroupInUseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DESC, m_Desc);
	DDX_Control(pDX, IDC_NEWGROUPCOMBO, m_NewGroup);
	DDX_Radio(pDX, IDC_RADIO1, m_action);
}


BEGIN_MESSAGE_MAP(CDeleteGroupInUseDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnOK)
END_MESSAGE_MAP()

BOOL CDeleteGroupInUseDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString str;
	m_Desc.GetWindowText(str);
	CString str2;
	str2.Format(str, m_groupName);
	m_Desc.SetWindowText(str2);

	m_NewGroup.AddString(_T("-- None --"));
	for (unsigned int i = 0; i < m_GroupsList->size(); i++)
	{
		CString name = (*m_GroupsList)[i].group;
		if (name == m_groupName)
			continue;
		m_NewGroup.AddString(name);
	}
	m_NewGroup.SetCurSel(0);

	m_action = 0;

	UpdateData(false);

	return true;
}

void CDeleteGroupInUseDlg::OnOK()
{
	UpdateData(true);
	if (m_NewGroup.GetCurSel() > 0)
		m_NewGroup.GetLBText(m_NewGroup.GetCurSel(), m_groupName);
	else
		m_groupName = _T("");
	CDialog::OnOK();
}
