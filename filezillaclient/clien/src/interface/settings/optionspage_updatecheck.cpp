#include <filezilla.h>

#if FZ_MANUALUPDATECHECK && FZ_AUTOUPDATECHECK

#include "../Options.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_updatecheck.h"
#include "updater.h"
#include "update_dialog.h"

BEGIN_EVENT_TABLE(COptionsPageUpdateCheck, COptionsPage)
EVT_BUTTON(XRCID("ID_RUNUPDATECHECK"), COptionsPageUpdateCheck::OnRunUpdateCheck)
END_EVENT_TABLE()

bool COptionsPageUpdateCheck::LoadPage()
{
	bool failure = false;
	int sel;
	if (!m_pOptions->GetOptionVal(OPTION_UPDATECHECK))
		sel = 0;
	else
	{
		int days = m_pOptions->GetOptionVal(OPTION_UPDATECHECK_INTERVAL);
		if (days < 7)
			sel = 1;
		else
			sel = 2;
	}
	SetChoice(XRCID("ID_UPDATECHECK"), sel, failure);

	int type = m_pOptions->GetOptionVal(OPTION_UPDATECHECK_CHECKBETA);
	if( type < 0 || type > 2 ) {
		type = 1;
	}
	SetChoice(XRCID("ID_UPDATETYPE"), type, failure);

	return !failure;
}

bool COptionsPageUpdateCheck::Validate()
{
	int type = GetChoice(XRCID("ID_UPDATETYPE"));
	if( type == 2 && m_pOptions->GetOptionVal(OPTION_UPDATECHECK_CHECKBETA) != 2 ) {
		if (wxMessageBoxEx(_("Warning, use nightly builds at your own risk.\nNo support is given for nightly builds.\nNightly builds may not work as expected and might even damage your system.\n\nDo you really want to check for nightly builds?"), _("Updates"), wxICON_EXCLAMATION | wxYES_NO, this) != wxYES) {
			bool tmp;
			SetChoice(XRCID("ID_UPDATETYPE"), m_pOptions->GetOptionVal(OPTION_UPDATECHECK_CHECKBETA), tmp);
		}
	}
	return true;
}

bool COptionsPageUpdateCheck::SavePage()
{
	int sel = GetChoice(XRCID("ID_UPDATECHECK"));
	m_pOptions->SetOption(OPTION_UPDATECHECK, (sel > 0) ? 1 : 0);
	int days = 0;
	switch (sel)
	{
	case 1:
		days = 1;
		break;
	case 2:
		days = 7;
		break;
	default:
		days = 0;
		break;
	}
	m_pOptions->SetOption(OPTION_UPDATECHECK_INTERVAL, days);

	int type = GetChoice(XRCID("ID_UPDATETYPE"));
	if( type < 0 || type > 2 ) {
		type = 1;
	}
	m_pOptions->SetOption(OPTION_UPDATECHECK_CHECKBETA, type);

	return true;
}

void COptionsPageUpdateCheck::OnRunUpdateCheck(wxCommandEvent &)
{
	if( !Validate() || !SavePage() ) {
		return;
	}

	CUpdater* updater = CUpdater::GetInstance();
	if( updater ) {
		updater->Init();
		CUpdateDialog dlg( this, *updater );
		dlg.ShowModal();
	}
}

#endif
