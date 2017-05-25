#include <filezilla.h>
#include "fileexistsdlg.h"
#include "Options.h"
#include "sizeformatting.h"
#include "timeformatting.h"
#include "themeprovider.h"
#include "xrc_helper.h"

#include <wx/display.h>

BEGIN_EVENT_TABLE(CFileExistsDlg, wxDialogEx)
EVT_BUTTON(XRCID("wxID_OK"), CFileExistsDlg::OnOK)
EVT_BUTTON(XRCID("wxID_CANCEL"), CFileExistsDlg::OnCancel)
EVT_CHECKBOX(wxID_ANY, CFileExistsDlg::OnCheck)
END_EVENT_TABLE()

CFileExistsDlg::CFileExistsDlg(CFileExistsNotification *pNotification)
{
	m_pNotification = pNotification;
	m_action = CFileExistsNotification::overwrite;
}

bool CFileExistsDlg::Create(wxWindow* parent)
{
	wxASSERT(parent);

	SetExtraStyle(wxWS_EX_BLOCK_EVENTS);
	SetParent(parent);
	if (!CreateControls()) {
		return false;
	}
	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);

	return true;
}

void CFileExistsDlg::DisplayFile(bool left, wxString name, int64_t size, CDateTime const& time, wxString const& iconFile)
{
	name = GetPathEllipsis(name, FindWindow(left ? XRCID("ID_FILE1_NAME") : XRCID("ID_FILE2_NAME")));
	name.Replace(_T("&"), _T("&&"));

	wxString sizeStr = _("Size unknown");
	if (size != -1) {
		bool const thousands_separator = COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0;
		sizeStr = CSizeFormat::Format(size, true, CSizeFormat::bytes, thousands_separator, 0);
	}

	wxString timeStr = _("Date/time unknown");
	if (time.IsValid())
		timeStr = CTimeFormat::Format(time);

	xrc_call(*this, left ? "ID_FILE1_NAME" : "ID_FILE2_NAME", &wxStaticText::SetLabel, name);
	xrc_call(*this, left ? "ID_FILE1_SIZE" : "ID_FILE2_SIZE", &wxStaticText::SetLabel, sizeStr);
	xrc_call(*this, left ? "ID_FILE1_TIME" : "ID_FILE2_TIME", &wxStaticText::SetLabel, timeStr);

	LoadIcon(left ? XRCID("ID_FILE1_ICON") : XRCID("ID_FILE2_ICON"), iconFile);
}

bool CFileExistsDlg::CreateControls()
{
	if (!Load(GetParent(), _T("ID_FILEEXISTSDLG"))) {
		return false;
	}

	wxString localFile = m_pNotification->localFile;
	wxString remoteFile = m_pNotification->remotePath.FormatFilename(m_pNotification->remoteFile);

	DisplayFile(m_pNotification->download, localFile, m_pNotification->localSize.GetValue(), m_pNotification->localTime, m_pNotification->localFile);
	DisplayFile(!m_pNotification->download, remoteFile, m_pNotification->remoteSize.GetValue(), m_pNotification->remoteTime, m_pNotification->remoteFile);

	xrc_call(*this, "ID_UPDOWNONLY", &wxCheckBox::SetLabel, m_pNotification->download ? _("A&pply only to downloads") : _("A&pply only to uploads"));

	return true;
}

void CFileExistsDlg::LoadIcon(int id, const wxString &file)
{
	wxStaticBitmap *pStatBmp = static_cast<wxStaticBitmap *>(FindWindow(id));
	if (!pStatBmp)
		return;

	wxSize size = CThemeProvider::GetIconSize(iconSizeNormal);
	pStatBmp->SetInitialSize(size);
	pStatBmp->InvalidateBestSize();
	pStatBmp->SetBitmap(CThemeProvider::GetBitmap(_T("ART_FILE"), wxART_OTHER, size));

#ifdef __WXMSW__
	SHFILEINFO fileinfo;
	memset(&fileinfo, 0, sizeof(fileinfo));
	if (SHGetFileInfo(file, FILE_ATTRIBUTE_NORMAL, &fileinfo, sizeof(fileinfo), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES)) {
		wxBitmap bmp;
		bmp.Create(size.x, size.y);

		wxMemoryDC *dc = new wxMemoryDC;

		wxPen pen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
		wxBrush brush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

		dc->SelectObject(bmp);

		dc->SetPen(pen);
		dc->SetBrush(brush);
		dc->DrawRectangle(0, 0, size.x, size.y);

		wxIcon icon;
		icon.SetHandle(fileinfo.hIcon);
		icon.SetSize(size.x, size.y);

		dc->DrawIcon(icon, 0, 0);
		delete dc;

		pStatBmp->SetBitmap(bmp);

		return;
	}

#endif //__WXMSW__

	wxFileName fn(file);
	wxString ext = fn.GetExt();
	if (ext.empty())
		return;

	wxFileType *pType = wxTheMimeTypesManager->GetFileTypeFromExtension(ext);
	if (pType) {
		wxIconLocation loc;
		if (pType->GetIcon(&loc) && loc.IsOk()) {
			wxLogNull *tmp = new wxLogNull;
			wxIcon icon(loc);
			delete tmp;
			if (!icon.Ok()) {
				delete pType;
				return;
			}

			int width = icon.GetWidth();
			int height = icon.GetHeight();
			if (width && height) {
				wxBitmap bmp;
				bmp.Create(icon.GetWidth(), icon.GetHeight());

				wxMemoryDC *dc = new wxMemoryDC;

				wxPen pen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
				wxBrush brush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

				dc->SelectObject(bmp);

				dc->SetPen(pen);
				dc->SetBrush(brush);
				dc->DrawRectangle(0, 0, width, height);

				dc->DrawIcon(icon, 0, 0);
				delete dc;

				pStatBmp->SetBitmap(bmp);

				return;
			}
		}
		delete pType;
	}
}

void CFileExistsDlg::OnOK(wxCommandEvent& event)
{
	if (xrc_call(*this, "ID_ACTION1", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::overwrite;
	else if (xrc_call(*this, "ID_ACTION2", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::overwriteNewer;
	else if (xrc_call(*this, "ID_ACTION3", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::resume;
	else if (xrc_call(*this, "ID_ACTION4", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::rename;
	else if (xrc_call(*this, "ID_ACTION5", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::skip;
	else if (xrc_call(*this, "ID_ACTION6", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::overwriteSizeOrNewer;
	else if (xrc_call(*this, "ID_ACTION7", &wxRadioButton::GetValue))
		m_action = CFileExistsNotification::overwriteSize;
	else
		m_action = CFileExistsNotification::overwrite;

	m_always = xrc_call(*this, "ID_ALWAYS", &wxCheckBox::GetValue);
	m_directionOnly = xrc_call(*this, "ID_UPDOWNONLY", &wxCheckBox::GetValue);
	m_queueOnly = xrc_call(*this, "ID_QUEUEONLY", &wxCheckBox::GetValue);
	EndModal(wxID_OK);
}

enum CFileExistsNotification::OverwriteAction CFileExistsDlg::GetAction() const
{
	return m_action;
}

void CFileExistsDlg::OnCancel(wxCommandEvent& event)
{
	m_action = CFileExistsNotification::skip;
	EndModal(wxID_CANCEL);
}

bool CFileExistsDlg::Always(bool &directionOnly, bool &queueOnly) const
{
	directionOnly = m_directionOnly;
	queueOnly = m_queueOnly;
	return m_always;
}

wxString CFileExistsDlg::GetPathEllipsis(wxString path, wxWindow *window)
{
	int dn = wxDisplay::GetFromWindow(GetParent()); // Use parent window as the dialog isn't realized yet.
	if (dn < 0) {
		return path;
	}

	int string_width; // width of the path string in pixels
	int y;			// dummy variable
	window->GetTextExtent(path, &string_width, &y);

	wxDisplay display(dn);
	wxRect rect = display.GetClientArea();
	const int DESKTOP_WIDTH = rect.GetWidth(); // width of the desktop in pixels
	const int maxWidth = (int)(DESKTOP_WIDTH * 0.75);

	// If the path is already short enough, don't change it
	if (string_width <= maxWidth || path.Length() < 20)
		return path;

	wxString fill = _T(" ");
	fill += 0x2026; //unicode ellipsis character
	fill += _T(" ");

	int fillWidth;
	window->GetTextExtent(fill, &fillWidth, &y);

	// Do initial split roughly in the middle of the string
	int middle = path.Length() / 2;
	wxString left = path.Left(middle);
	wxString right = path.Mid(middle);

	int leftWidth, rightWidth;
	window->GetTextExtent(left, &leftWidth, &y);
	window->GetTextExtent(right, &rightWidth, &y);

	// continue removing one character at a time around the fill until path string is small enough
	while ((leftWidth + fillWidth + rightWidth) > maxWidth) {
		if (leftWidth > rightWidth && left.Len() > 10) {
			left.RemoveLast();
			window->GetTextExtent(left, &leftWidth, &y);
		}
		else {
			if (right.Len() <= 10)
				break;

			right = right.Mid(1);
			window->GetTextExtent(right, &rightWidth, &y);
		}
	}

	return left + fill + right;
}

void CFileExistsDlg::OnCheck(wxCommandEvent& event)
{
	if (event.GetId() != XRCID("ID_UPDOWNONLY") && event.GetId() != XRCID("ID_QUEUEONLY"))
		return;

	if (event.IsChecked())
		XRCCTRL(*this, "ID_ALWAYS", wxCheckBox)->SetValue(true);
}
