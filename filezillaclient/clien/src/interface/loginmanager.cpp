#include <filezilla.h>
#include "loginmanager.h"

#include "dialogex.h"
#include "filezillaapp.h"

CLoginManager CLoginManager::m_theLoginManager;

bool CLoginManager::GetPassword(CServer &server, bool silent, wxString const& name, wxString const& challenge)
{
	wxASSERT(!silent || server.GetLogonType() == ASK || server.GetLogonType() == INTERACTIVE);
	wxASSERT(challenge.empty() || server.GetLogonType() == INTERACTIVE);

	if (server.GetLogonType() == ASK) {
		for (auto const& it : m_passwordCache) {
			if (it.host != server.GetHost())
				continue;
			if (it.port != server.GetPort())
				continue;
			if (it.user != server.GetUser())
				continue;

			server.SetUser(server.GetUser(), it.password);
			return true;
		}
	}
	if (silent)
		return false;

	return DisplayDialog(server, name, challenge);
}

bool CLoginManager::DisplayDialog(CServer &server, wxString const& name, wxString challenge)
{
	wxDialogEx pwdDlg;
	if (!pwdDlg.Load(wxGetApp().GetTopWindow(), _T("ID_ENTERPASSWORD"))) {
		return false;
	}

	if (name.empty()) {
		pwdDlg.GetSizer()->Show(XRCCTRL(pwdDlg, "ID_NAMELABEL", wxStaticText), false, true);
		pwdDlg.GetSizer()->Show(XRCCTRL(pwdDlg, "ID_NAME", wxStaticText), false, true);
	}
	else
		XRCCTRL(pwdDlg, "ID_NAME", wxStaticText)->SetLabel(name);
	if (challenge.empty()) {
		pwdDlg.GetSizer()->Show(XRCCTRL(pwdDlg, "ID_CHALLENGELABEL", wxStaticText), false, true);
		pwdDlg.GetSizer()->Show(XRCCTRL(pwdDlg, "ID_CHALLENGE", wxTextCtrl), false, true);
	}
	else {
#ifdef __WXMSW__
		challenge.Replace(_T("\n"), _T("\r\n"));
#endif
		XRCCTRL(pwdDlg, "ID_CHALLENGE", wxTextCtrl)->ChangeValue(challenge);
		pwdDlg.GetSizer()->Show(XRCCTRL(pwdDlg, "ID_REMEMBER", wxCheckBox), false, true);
		XRCCTRL(pwdDlg, "ID_CHALLENGE", wxTextCtrl)->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	}
	XRCCTRL(pwdDlg, "ID_HOST", wxStaticText)->SetLabel(server.FormatHost());

	if (server.GetUser().empty()) {
		pwdDlg.SetTitle(_("Enter username and password"));
		XRCCTRL(pwdDlg, "ID_OLD_USER_LABEL", wxStaticText)->Hide();
		XRCCTRL(pwdDlg, "ID_OLD_USER", wxStaticText)->Hide();

		XRCCTRL(pwdDlg, "ID_HEADER_PASS", wxStaticText)->Hide();
		if (server.GetLogonType() == INTERACTIVE) {
			XRCCTRL(pwdDlg, "ID_PASSWORD_LABEL", wxStaticText)->Hide();
			XRCCTRL(pwdDlg, "ID_PASSWORD", wxTextCtrl)->Hide();
			XRCCTRL(pwdDlg, "ID_REMEMBER", wxCheckBox)->Hide();
			XRCCTRL(pwdDlg, "ID_HEADER_BOTH", wxStaticText)->Hide();
		}
		else
			XRCCTRL(pwdDlg, "ID_HEADER_USER", wxStaticText)->Hide();

		XRCCTRL(pwdDlg, "ID_NEW_USER", wxTextCtrl)->SetFocus();
	}
	else {
		XRCCTRL(pwdDlg, "ID_OLD_USER", wxStaticText)->SetLabel(server.GetUser());
		XRCCTRL(pwdDlg, "ID_NEW_USER_LABEL", wxStaticText)->Hide();
		XRCCTRL(pwdDlg, "ID_NEW_USER", wxTextCtrl)->Hide();
		XRCCTRL(pwdDlg, "ID_HEADER_USER", wxStaticText)->Hide();
		XRCCTRL(pwdDlg, "ID_HEADER_BOTH", wxStaticText)->Hide();
	}
	XRCCTRL(pwdDlg, "wxID_OK", wxButton)->SetId(wxID_OK);
	XRCCTRL(pwdDlg, "wxID_CANCEL", wxButton)->SetId(wxID_CANCEL);
	pwdDlg.GetSizer()->Fit(&pwdDlg);
	pwdDlg.GetSizer()->SetSizeHints(&pwdDlg);

	wxString user;
	while (user.empty()) {
		if (pwdDlg.ShowModal() != wxID_OK)
			return false;

		if (server.GetUser().empty()) {
			user = XRCCTRL(pwdDlg, "ID_NEW_USER", wxTextCtrl)->GetValue();
			if (user.empty()) {
				wxMessageBoxEx(_("No username given."), _("Invalid input"), wxICON_EXCLAMATION);
				continue;
			}
		}
		else
			user = server.GetUser();
	}

	server.SetUser(user, XRCCTRL(pwdDlg, "ID_PASSWORD", wxTextCtrl)->GetValue());

	if (server.GetLogonType() == ASK && XRCCTRL(pwdDlg, "ID_REMEMBER", wxCheckBox)->GetValue()) {
		t_passwordcache entry;
		entry.host = server.GetHost();
		entry.port = server.GetPort();
		entry.user = server.GetUser();
		entry.password = server.GetPass();
		m_passwordCache.push_back(entry);
	}

	return true;
}

void CLoginManager::CachedPasswordFailed(const CServer& server)
{
	if (server.GetLogonType() != ASK)
		return;

	for (auto iter = m_passwordCache.begin(); iter != m_passwordCache.end(); ++iter) {
		if (iter->host != server.GetHost())
			continue;
		if (iter->port != server.GetPort())
			continue;
		if (iter->user != server.GetUser())
			continue;

		m_passwordCache.erase(iter);
		return;
	}
}
