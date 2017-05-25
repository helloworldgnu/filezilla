#include <filezilla.h>
#include "verifyhostkeydialog.h"
#include <wx/tokenzr.h>
#include "dialogex.h"
#include "ipcmutex.h"

std::list<CVerifyHostkeyDialog::t_keyData> CVerifyHostkeyDialog::m_sessionTrustedKeys;

void CVerifyHostkeyDialog::ShowVerificationDialog(wxWindow* parent, CHostKeyNotification& notification)
{
	wxDialogEx dlg;
	bool loaded;
	if (notification.GetRequestID() == reqId_hostkey)
		loaded = dlg.Load(parent, _T("ID_HOSTKEY"));
	else
		loaded = dlg.Load(parent, _T("ID_HOSTKEYCHANGED"));
	if (!loaded) {
		notification.m_trust = false;
		notification.m_alwaysTrust = false;
		wxBell();
		return;
	}

	dlg.WrapText(&dlg, XRCID("ID_DESC"), 400);

	const wxString host = wxString::Format(_T("%s:%d"), notification.GetHost(), notification.GetPort());
	dlg.SetChildLabel(XRCID("ID_HOST"), host);
	dlg.SetChildLabel(XRCID("ID_FINGERPRINT"), notification.GetFingerprint());

	dlg.GetSizer()->Fit(&dlg);
	dlg.GetSizer()->SetSizeHints(&dlg);

	int res = dlg.ShowModal();

	if (res == wxID_OK) {
		notification.m_trust = true;
		notification.m_alwaysTrust = XRCCTRL(dlg, "ID_ALWAYS", wxCheckBox)->GetValue();

		struct t_keyData data;
		data.host = host;
		data.fingerprint = notification.GetFingerprint();
		m_sessionTrustedKeys.push_back(data);
		return;
	}

	notification.m_trust = false;
	notification.m_alwaysTrust = false;
}

bool CVerifyHostkeyDialog::IsTrusted(CHostKeyNotification const& notification)
{
	const wxString host = wxString::Format(_T("%s:%d"), notification.GetHost(), notification.GetPort());

	for(auto const& trusted : m_sessionTrustedKeys ) {
		if (trusted.host == host && trusted.fingerprint == notification.GetFingerprint())
			return true;
	}

	return false;
}
