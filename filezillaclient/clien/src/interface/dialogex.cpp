#include <filezilla.h>
#include "dialogex.h"
#include <msgbox.h>

BEGIN_EVENT_TABLE(wxDialogEx, wxDialog)
EVT_CHAR_HOOK(wxDialogEx::OnChar)
END_EVENT_TABLE()

int wxDialogEx::m_shown_dialogs = 0;

#ifdef __WXMAC__
static int const pasteId = wxNewId();
static int const selectAllId = wxNewId();

extern wxTextEntry* GetSpecialTextEntry(wxWindow*, wxChar);

bool wxDialogEx::ProcessEvent(wxEvent& event)
{
	if(event.GetEventType() != wxEVT_MENU) {
		return wxDialog::ProcessEvent(event);
	}

	wxTextEntry* e = GetSpecialTextEntry(FindFocus(), 'V');
	if( e && event.GetId() == pasteId ) {
		e->Paste();
		return true;
	}
	else if( e && event.GetId() == selectAllId ) {
		e->SelectAll();
		return true;
	}
	else {
		return wxDialog::ProcessEvent(event);
	}
}
#endif

void wxDialogEx::OnChar(wxKeyEvent& event)
{
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, wxID_CANCEL);
		ProcessEvent(event);
	}
	else
		event.Skip();
}

bool wxDialogEx::Load(wxWindow* pParent, const wxString& name)
{
	SetParent(pParent);
	if (!wxXmlResource::Get()->LoadDialog(this, pParent, name))
		return false;

	//GetSizer()->Fit(this);
	//GetSizer()->SetSizeHints(this);

#ifdef __WXMAC__
	wxAcceleratorEntry entries[2];
	entries[0].Set(wxACCEL_CMD, 'V', pasteId);
	entries[1].Set(wxACCEL_CMD, 'A', selectAllId);
	wxAcceleratorTable accel(sizeof(entries) / sizeof(wxAcceleratorEntry), entries);
	SetAcceleratorTable(accel);
#endif

	return true;
}

bool wxDialogEx::SetChildLabel(int id, const wxString& label, unsigned long maxLength /*=0*/)
{
	wxStaticText* pText = wxDynamicCast(FindWindow(id), wxStaticText);
	if (!pText)
		return false;

	if (!maxLength)
		pText->SetLabel(label);
	else {
		wxString wrapped = label;
		WrapText(this, wrapped, maxLength);
		pText->SetLabel(wrapped);
	}

	return true;
}

wxString wxDialogEx::GetChildLabel(int id)
{
	wxStaticText* pText = wxDynamicCast(FindWindow(id), wxStaticText);
	if (!pText)
		return wxString();

	return pText->GetLabel();
}

int wxDialogEx::ShowModal()
{
	CenterOnParent();

#ifdef __WXMSW__
	// All open menus need to be closed or app will become unresponsive.
	::EndMenu();

	// For same reason release mouse capture.
	// Could happen during drag&drop with notification dialogs.
	::ReleaseCapture();
#endif

	m_shown_dialogs++;

	int ret = wxDialog::ShowModal();

	m_shown_dialogs--;

	return ret;
}

bool wxDialogEx::ReplaceControl(wxWindow* old, wxWindow* wnd)
{
	if( !GetSizer()->Replace(old, wnd, true) ) {
		return false;
	}
	old->Destroy();

	return true;
}

bool wxDialogEx::CanShowPopupDialog()
{
	if( ShownDialogs() || IsShowingMessageBox() ) {
		return false;
	}

	wxMouseState mouseState = wxGetMouseState();
	if( mouseState.LeftIsDown() || mouseState.MiddleIsDown() || mouseState.RightIsDown() ) {
		return false;
	}
#ifdef __WXMSW__
	// Don't check for changes if mouse is captured,
	// e.g. if user is dragging a file
	if (GetCapture()) {
		return false;
	}
#endif

	return true;
}

void wxDialogEx::InitDialog()
{
#ifdef __WXGTK__
	// Some controls only report proper size after the call to Show(), e.g.
	// wxStaticBox::GetBordersForSizer is affected by this.
	// Re-fit window to compensate.
	wxSizer* s = GetSizer();
	if( s ) {
		wxSize min = GetMinClientSize();
		wxSize smin = s->GetMinSize();
		if( min.x < smin.x || min.y < smin.y ) {
			s->Fit(this);
			SetMinSize(GetSize());
		}
	}
#endif

	wxDialog::InitDialog();
}
