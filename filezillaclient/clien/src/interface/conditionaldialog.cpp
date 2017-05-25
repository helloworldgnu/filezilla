#include <filezilla.h>
#include "conditionaldialog.h"
#include "filezillaapp.h"
#include "wrapengine.h"
#include "Options.h"

BEGIN_EVENT_TABLE(CConditionalDialog, wxDialog)
EVT_BUTTON(wxID_ANY, CConditionalDialog::OnButton)
END_EVENT_TABLE()

CConditionalDialog::CConditionalDialog(wxWindow* parent, enum DialogType type, enum Modes mode, bool checked /*=false*/)
	: wxDialog(parent, wxID_ANY, _T(""), wxDefaultPosition), m_type(type)
{
	wxSizer* pVertSizer = new wxBoxSizer(wxVERTICAL);

	wxSizer* pMainSizer = new wxBoxSizer(wxHORIZONTAL);
	pVertSizer->Add(pMainSizer);

	pMainSizer->AddSpacer(5);
	wxSizer* pSizer = new wxBoxSizer(wxVERTICAL);
	pMainSizer->Add(pSizer, 0, wxALL, 5);
	m_pTextSizer = new wxFlexGridSizer(1, 5, 6);
	pSizer->Add(m_pTextSizer, 0, wxTOP, 5);

	wxCheckBox *pCheckBox = new wxCheckBox(this, wxID_HIGHEST + 1, _("&Don't show this dialog again."));
	pCheckBox->SetValue(checked);
	pSizer->Add(pCheckBox, 0, wxTOP | wxBOTTOM, 5);

	wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
	pVertSizer->Add(buttons, 0, wxGROW | wxALL, 5);

	if (mode == ok) {
		pMainSizer->Prepend(new wxStaticBitmap(this, wxID_ANY, wxArtProvider::GetBitmap(wxART_INFORMATION)), 0, wxLEFT | wxBOTTOM | wxTOP, 10);

		wxButton* ok = new wxButton(this, wxID_OK);
		ok->SetDefault();
		buttons->AddButton(ok);
		SetEscapeId(wxID_OK);
	}
	else {
		pMainSizer->Prepend(new wxStaticBitmap(this, wxID_ANY, wxArtProvider::GetBitmap(wxART_QUESTION)), 0, wxLEFT | wxBOTTOM | wxTOP, 10);

		wxButton* yes = new wxButton(this, wxID_YES);
		yes->SetDefault();
		buttons->AddButton(yes);

		buttons->AddButton(new wxButton(this, wxID_NO));
		SetEscapeId(wxID_NO);
	}
	buttons->Realize();

	SetSizer(pVertSizer);
}

bool CConditionalDialog::Run()
{
	wxString dialogs = COptions::Get()->GetOption(OPTION_ONETIME_DIALOGS);
	if (dialogs.Len() > (unsigned int)m_type && dialogs[m_type] == '1')
		return true;

	Fit();
	wxGetApp().GetWrapEngine()->WrapRecursive(this, 3);

	CenterOnParent();

	int id = ShowModal();

	if (wxDynamicCast(FindWindow(wxID_HIGHEST + 1), wxCheckBox)->GetValue())
	{
		while (dialogs.Len() <= (unsigned int)m_type)
			dialogs += _T("0");
		dialogs[m_type] = '1';
		COptions::Get()->SetOption(OPTION_ONETIME_DIALOGS, dialogs);
	}

	if (id == wxID_OK || id == wxID_YES)
		return true;

	return false;
}

void CConditionalDialog::AddText(const wxString& text)
{
	m_pTextSizer->Add(new wxStaticText(this, wxID_ANY, text));
}

void CConditionalDialog::OnButton(wxCommandEvent& event)
{
	EndDialog(event.GetId());
}
