#include <filezilla.h>
#include "../Options.h"
#include "../sizeformatting.h"
#include "settingsdialog.h"
#include "optionspage.h"
#include "optionspage_transfer.h"

BEGIN_EVENT_TABLE(COptionsPageTransfer, COptionsPage)
EVT_CHECKBOX(XRCID("ID_ENABLE_SPEEDLIMITS"), COptionsPageTransfer::OnToggleSpeedLimitEnable)
END_EVENT_TABLE()

void COptionsPageTransfer::OnToggleSpeedLimitEnable(wxCommandEvent& event)
{
	bool enable_speedlimits = GetCheck(XRCID("ID_ENABLE_SPEEDLIMITS"));
	XRCCTRL(*this, "ID_DOWNLOADLIMIT", wxTextCtrl)->Enable(enable_speedlimits);
	XRCCTRL(*this, "ID_UPLOADLIMIT", wxTextCtrl)->Enable(enable_speedlimits);
	XRCCTRL(*this, "ID_BURSTTOLERANCE", wxChoice)->Enable(enable_speedlimits);
}

bool COptionsPageTransfer::LoadPage()
{
	bool failure = false;

	bool enable_speedlimits = m_pOptions->GetOptionVal(OPTION_SPEEDLIMIT_ENABLE) != 0;
	SetCheck(XRCID("ID_ENABLE_SPEEDLIMITS"), enable_speedlimits, failure);

	wxTextCtrl* pTextCtrl = XRCCTRL(*this, "ID_DOWNLOADLIMIT", wxTextCtrl);
	if (!pTextCtrl)
		return false;
	pTextCtrl->SetMaxLength(9);
	pTextCtrl->ChangeValue(m_pOptions->GetOption(OPTION_SPEEDLIMIT_INBOUND));
	pTextCtrl->Enable(enable_speedlimits);

	pTextCtrl = XRCCTRL(*this, "ID_UPLOADLIMIT", wxTextCtrl);
	if (!pTextCtrl)
		return false;
	pTextCtrl->SetMaxLength(9);
	pTextCtrl->ChangeValue(m_pOptions->GetOption(OPTION_SPEEDLIMIT_OUTBOUND));
	pTextCtrl->Enable(enable_speedlimits);

	XRCCTRL(*this, "ID_NUMTRANSFERS", wxSpinCtrl)->SetValue(m_pOptions->GetOptionVal(OPTION_NUMTRANSFERS));
	XRCCTRL(*this, "ID_NUMDOWNLOADS", wxSpinCtrl)->SetValue(m_pOptions->GetOptionVal(OPTION_CONCURRENTDOWNLOADLIMIT));
	XRCCTRL(*this, "ID_NUMUPLOADS", wxSpinCtrl)->SetValue(m_pOptions->GetOptionVal(OPTION_CONCURRENTUPLOADLIMIT));

	SetChoice(XRCID("ID_BURSTTOLERANCE"), m_pOptions->GetOptionVal(OPTION_SPEEDLIMIT_BURSTTOLERANCE), failure);
	XRCCTRL(*this, "ID_BURSTTOLERANCE", wxChoice)->Enable(enable_speedlimits);

	wxStaticText* pUnit = XRCCTRL(*this, "ID_DOWNLOADLIMIT_UNIT", wxStaticText);
	if (!pUnit)
		return false;
	pUnit->SetLabel(wxString::Format(pUnit->GetLabel(), CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024)));

	pUnit = XRCCTRL(*this, "ID_UPLOADLIMIT_UNIT", wxStaticText);
	if (!pUnit)
		return false;
	pUnit->SetLabel(wxString::Format(pUnit->GetLabel(), CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024)));

	pTextCtrl = XRCCTRL(*this, "ID_REPLACE", wxTextCtrl);
	pTextCtrl->SetMaxLength(1);
	pTextCtrl->ChangeValue(m_pOptions->GetOption(OPTION_INVALID_CHAR_REPLACE));

	SetCheckFromOption(XRCID("ID_ENABLE_REPLACE"), OPTION_INVALID_CHAR_REPLACE_ENABLE, failure);

#ifdef __WXMSW__
	wxString invalid = _T("\\ / : * ? \" < > |");
	wxString filtered = wxString::Format(_("The following characters will be replaced: %s"), invalid);
#else
	wxString invalid = _T("/");
	wxString filtered = wxString::Format(_("The following character will be replaced: %s"), invalid);
#endif
	XRCCTRL(*this, "ID_REPLACED", wxStaticText)->SetLabel(filtered);

	SetCheckFromOption(XRCID("ID_ENABLE_PREALLOCATION"), OPTION_PREALLOCATE_SPACE, failure);

	return !failure;
}

bool COptionsPageTransfer::SavePage()
{
	SetOptionFromCheck(XRCID("ID_ENABLE_SPEEDLIMITS"), OPTION_SPEEDLIMIT_ENABLE);

	m_pOptions->SetOption(OPTION_NUMTRANSFERS,				XRCCTRL(*this, "ID_NUMTRANSFERS", wxSpinCtrl)->GetValue());
	m_pOptions->SetOption(OPTION_CONCURRENTDOWNLOADLIMIT,	XRCCTRL(*this, "ID_NUMDOWNLOADS", wxSpinCtrl)->GetValue());
	m_pOptions->SetOption(OPTION_CONCURRENTUPLOADLIMIT,		XRCCTRL(*this, "ID_NUMUPLOADS", wxSpinCtrl)->GetValue());

	SetOptionFromText(XRCID("ID_DOWNLOADLIMIT"), OPTION_SPEEDLIMIT_INBOUND);
	SetOptionFromText(XRCID("ID_UPLOADLIMIT"), OPTION_SPEEDLIMIT_OUTBOUND);
	m_pOptions->SetOption(OPTION_SPEEDLIMIT_BURSTTOLERANCE, GetChoice(XRCID("ID_BURSTTOLERANCE")));
	SetOptionFromText(XRCID("ID_REPLACE"), OPTION_INVALID_CHAR_REPLACE);
	SetOptionFromCheck(XRCID("ID_ENABLE_REPLACE"), OPTION_INVALID_CHAR_REPLACE_ENABLE);

	SetOptionFromCheck(XRCID("ID_ENABLE_PREALLOCATION"), OPTION_PREALLOCATE_SPACE);

	return true;
}

bool COptionsPageTransfer::Validate()
{
	long tmp;
	wxTextCtrl* pCtrl;
	wxSpinCtrl* pSpinCtrl;
	int spinValue;

	pSpinCtrl = XRCCTRL(*this, "ID_NUMTRANSFERS", wxSpinCtrl);
	spinValue = pSpinCtrl->GetValue();
	if (spinValue < 1 || spinValue > 10)
		return DisplayError(pSpinCtrl, _("Please enter a number between 1 and 10 for the number of concurrent transfers."));

	pSpinCtrl = XRCCTRL(*this, "ID_NUMDOWNLOADS", wxSpinCtrl);
	spinValue = pSpinCtrl->GetValue();
	if (spinValue < 0 || spinValue > 10)
		return DisplayError(pSpinCtrl, _("Please enter a number between 0 and 10 for the number of concurrent downloads."));

	pSpinCtrl = XRCCTRL(*this, "ID_NUMUPLOADS", wxSpinCtrl);
	spinValue = pSpinCtrl->GetValue();
	if (spinValue < 0 || spinValue > 10)
		return DisplayError(pSpinCtrl, _("Please enter a number between 0 and 10 for the number of concurrent uploads."));

	pCtrl = XRCCTRL(*this, "ID_DOWNLOADLIMIT", wxTextCtrl);
	if (!pCtrl->GetValue().ToLong(&tmp) || (tmp < 0))
	{
		const wxString unit = CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024);
		return DisplayError(pCtrl, wxString::Format(_("Please enter a download speed limit greater or equal to 0 %s/s."), unit));
	}

	pCtrl = XRCCTRL(*this, "ID_UPLOADLIMIT", wxTextCtrl);
	if (!pCtrl->GetValue().ToLong(&tmp) || (tmp < 0))
	{
		const wxString unit = CSizeFormat::GetUnitWithBase(CSizeFormat::kilo, 1024);
		return DisplayError(pCtrl, wxString::Format(_("Please enter an upload speed limit greater or equal to 0 %s/s."), unit));
	}

	pCtrl = XRCCTRL(*this, "ID_REPLACE", wxTextCtrl);
	wxString replace = pCtrl->GetValue();
#ifdef __WXMSW__
	if (replace == _T("\\") ||
		replace == _T("/") ||
		replace == _T(":") ||
		replace == _T("*") ||
		replace == _T("?") ||
		replace == _T("\"") ||
		replace == _T("<") ||
		replace == _T(">") ||
		replace == _T("|"))
#else
	if (replace == _T("/"))
#endif
	{
		return DisplayError(pCtrl, _("You cannot replace an invalid character with another invalid character. Please enter a character that is allowed in filenames."));
	}

	return true;
}
