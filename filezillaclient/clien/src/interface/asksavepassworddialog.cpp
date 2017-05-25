#include "filezilla.h"
#include "asksavepassworddialog.h"
#include "Options.h"
#include "filezillaapp.h"
#include "xrc_helper.h"
#include "sitemanager.h"

BEGIN_EVENT_TABLE(CAskSavePasswordDialog, wxDialogEx)
EVT_RADIOBUTTON(XRCID("ID_REMEMBER_YES"), CAskSavePasswordDialog::OnRadioButtonChanged)
EVT_RADIOBUTTON(XRCID("ID_REMEMBER_NO"), CAskSavePasswordDialog::OnRadioButtonChanged)
END_EVENT_TABLE()

bool CAskSavePasswordDialog::Create(wxWindow*)
{
	if (!Load(0, _T("ID_ASK_SAVE_PASSWORD"))) {
		return false;
	}

	wxButton* ok = XRCCTRL(*this, "wxID_OK", wxButton);
	if (ok)
		ok->Enable(false);

	wxGetApp().GetWrapEngine()->WrapRecursive(this, 2, "");

	return true;
}


bool CAskSavePasswordDialog::Run(wxWindow* parent)
{
	bool ret = true;

	if (COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) == 0 && COptions::Get()->GetOptionVal(OPTION_PROMPTPASSWORDSAVE) != 0 && !CSiteManager::HasSites()) {
		CAskSavePasswordDialog dlg;
		if (dlg.Create(parent)) {
			ret = dlg.ShowModal() == wxID_OK;
			if (ret) {
				if (xrc_call(dlg, "ID_REMEMBER_NO", &wxRadioButton::GetValue)) {
					COptions::Get()->SetOption(OPTION_DEFAULT_KIOSKMODE, 1);
				}
				COptions::Get()->SetOption(OPTION_PROMPTPASSWORDSAVE, 0);
			}
		}
	}
	else
		COptions::Get()->SetOption(OPTION_PROMPTPASSWORDSAVE, 0);

	return ret;
}

void CAskSavePasswordDialog::OnRadioButtonChanged(wxCommandEvent&)
{
	wxRadioButton* yes = XRCCTRL(*this, "ID_REMEMBER_NO", wxRadioButton);
	wxRadioButton* no = XRCCTRL(*this, "ID_REMEMBER_YES", wxRadioButton);
	wxButton* ok = XRCCTRL(*this, "wxID_OK", wxButton);
	if (yes && no && ok)
		ok->Enable(yes->GetValue() || no->GetValue());
}
