#include <filezilla.h>
#include "Options.h"
#include "sftp_crypt_info_dlg.h"
#include "sizeformatting.h"
#include "speedlimits_dialog.h"
#include "statusbar.h"
#include "themeprovider.h"
#include "verifycertdialog.h"

#include <wx/dcclient.h>

static const int statbarWidths[3] = {
	-3, 0, 25
};
#define FIELD_QUEUESIZE 1

BEGIN_EVENT_TABLE(wxStatusBarEx, wxStatusBar)
EVT_SIZE(wxStatusBarEx::OnSize)
END_EVENT_TABLE()

wxStatusBarEx::wxStatusBarEx(wxTopLevelWindow* pParent)
{
	m_pParent = pParent;
	m_columnWidths = 0;
	Create(pParent, wxID_ANY);

#ifdef __WXMSW__
	m_parentWasMaximized = false;

	if (GetLayoutDirection() != wxLayout_RightToLeft)
		SetDoubleBuffered(true);
#endif
}

wxStatusBarEx::~wxStatusBarEx()
{
	delete [] m_columnWidths;
}

void wxStatusBarEx::SetFieldsCount(int number /*=1*/, const int* widths /*=NULL*/)
{
	wxASSERT(number > 0);

	int oldCount = GetFieldsCount();
	int* oldWidths = m_columnWidths;

	m_columnWidths = new int[number];
	if (!widths)
	{
		if (oldWidths)
		{
			const int min = wxMin(oldCount, number);
			for (int i = 0; i < min; i++)
				m_columnWidths[i] = oldWidths[i];
			for (int i = min; i < number; i++)
				m_columnWidths[i] = -1;
			delete [] oldWidths;
		}
		else
		{
			for (int i = 0; i < number; i++)
				m_columnWidths[i] = -1;
		}
	}
	else
	{
		delete [] oldWidths;
		for (int i = 0; i < number; i++)
			m_columnWidths[i] = widths[i];

		FixupFieldWidth(number - 1);
	}

	wxStatusBar::SetFieldsCount(number, m_columnWidths);
}

void wxStatusBarEx::SetStatusWidths(int n, const int *widths)
{
	wxASSERT(n == GetFieldsCount());
	wxASSERT(widths);
	for (int i = 0; i < n; i++)
		m_columnWidths[i] = widths[i];

	FixupFieldWidth(n - 1);

	wxStatusBar::SetStatusWidths(n, m_columnWidths);
}

void wxStatusBarEx::FixupFieldWidth(int field)
{
	if (field != GetFieldsCount() - 1)
		return;

#if __WXGTK20__
	// Gripper overlaps last all the time
	if (m_columnWidths[field] > 0)
		m_columnWidths[field] += 15;
#endif
}

void wxStatusBarEx::SetFieldWidth(int field, int width)
{
	field = GetFieldIndex(field);
	if( field < 0 ) {
		return;
	}

	m_columnWidths[field] = width;
	FixupFieldWidth(field);
	wxStatusBar::SetStatusWidths(GetFieldsCount(), m_columnWidths);
}

int wxStatusBarEx::GetFieldIndex(int field)
{
	if (field >= 0) {
		wxCHECK(field <= GetFieldsCount(), -1);
	}
	else {
		field = GetFieldsCount() + field;
		wxCHECK(field >= 0, -1);
	}

	return field;
}

void wxStatusBarEx::OnSize(wxSizeEvent&)
{
#ifdef __WXMSW__
	const int count = GetFieldsCount();
	if (count && m_columnWidths && m_columnWidths[count - 1] > 0)
	{
		// No sizegrip on maximized windows
		bool isMaximized = m_pParent->IsMaximized();
		if (isMaximized != m_parentWasMaximized)
		{
			m_parentWasMaximized = isMaximized;

			if (isMaximized)
				m_columnWidths[count - 1] -= 16;
			else
				m_columnWidths[count - 1] += 16;

			wxStatusBar::SetStatusWidths(count, m_columnWidths);
			Refresh();
		}
	}
#endif
}

#ifdef __WXGTK__
void wxStatusBarEx::SetStatusText(const wxString& text, int number /*=0*/)
{
	wxString oldText = GetStatusText(number);
	if (oldText != text) {
		wxStatusBar::SetStatusText(text, number);
	}
}
#endif

int wxStatusBarEx::GetGripperWidth()
{
#if defined(__WXMSW__)
	return m_pParent->IsMaximized() ? 0 : 6;
#elif defined(__WXGTK__)
	return 15;
#else
	return 0;
#endif
}



BEGIN_EVENT_TABLE(CWidgetsStatusBar, wxStatusBarEx)
EVT_SIZE(CWidgetsStatusBar::OnSize)
END_EVENT_TABLE()

CWidgetsStatusBar::CWidgetsStatusBar(wxTopLevelWindow* parent)
	: wxStatusBarEx(parent)
{
}

CWidgetsStatusBar::~CWidgetsStatusBar()
{
}

void CWidgetsStatusBar::OnSize(wxSizeEvent& event)
{
	wxStatusBarEx::OnSize(event);

	for (int i = 0; i < GetFieldsCount(); i++)
		PositionChildren(i);

#ifdef __WXMSW__
	if (GetLayoutDirection() != wxLayout_RightToLeft)
		Update();
#endif
}

bool CWidgetsStatusBar::AddField(int field, int idx, wxWindow* pChild)
{
	field = GetFieldIndex(field);
	if( field < 0 ) {
		return false;
	}

	t_statbar_child data;
	data.field = field;
	data.pChild = pChild;

	m_children[idx] = data;

	PositionChildren(field);

	return true;
}

void CWidgetsStatusBar::RemoveField(int idx)
{
	auto iter = m_children.find(idx);
	if (iter != m_children.end()) {
		int field = iter->second.field;
		m_children.erase(iter);
		PositionChildren(field);
	}
}

void CWidgetsStatusBar::PositionChildren(int field)
{
	wxRect rect;
	GetFieldRect(field, rect);

	int offset = 2;

#ifndef __WXMSW__
	if (field + 1 == GetFieldsCount())
	{
		rect.SetWidth(m_columnWidths[field]);
		offset += 5 + GetGripperWidth();
	}
#endif

	for (auto iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		if (iter->second.field != field)
			continue;

		const wxSize size = iter->second.pChild->GetSize();
		int position = rect.GetRight() - size.x - offset;

		iter->second.pChild->SetSize(position, rect.GetTop() + (rect.GetHeight() - size.y + 1) / 2, -1, -1);

		offset += size.x + 3;
	}
}

void CWidgetsStatusBar::SetFieldWidth(int field, int width)
{
	wxStatusBarEx::SetFieldWidth(field, width);
	for (int i = 0; i < GetFieldsCount(); i++)
		PositionChildren(i);
}

class CIndicator : public wxStaticBitmap
{
public:
	CIndicator(CStatusBar* pStatusBar, const wxBitmap& bmp)
		: wxStaticBitmap(pStatusBar, wxID_ANY, bmp)
	{
		m_pStatusBar = pStatusBar;
	}

protected:
	CStatusBar* m_pStatusBar;

	DECLARE_EVENT_TABLE()
	void OnLeftMouseUp(wxMouseEvent&)
	{
		m_pStatusBar->OnHandleLeftClick(this);
	}
	void OnRightMouseUp(wxMouseEvent&)
	{
		m_pStatusBar->OnHandleRightClick(this);
	}
};

BEGIN_EVENT_TABLE(CIndicator, wxStaticBitmap)
EVT_LEFT_UP(CIndicator::OnLeftMouseUp)
EVT_RIGHT_UP(CIndicator::OnRightMouseUp)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(CStatusBar, CWidgetsStatusBar)
EVT_MENU(XRCID("ID_SPEEDLIMITCONTEXT_ENABLE"), CStatusBar::OnSpeedLimitsEnable)
EVT_MENU(XRCID("ID_SPEEDLIMITCONTEXT_CONFIGURE"), CStatusBar::OnSpeedLimitsConfigure)
END_EVENT_TABLE()

CStatusBar::CStatusBar(wxTopLevelWindow* pParent)
	: CWidgetsStatusBar(pParent), CStateEventHandler(0)
{
	// Speedlimits
	RegisterOption(OPTION_SPEEDLIMIT_ENABLE);
	RegisterOption(OPTION_SPEEDLIMIT_INBOUND);
	RegisterOption(OPTION_SPEEDLIMIT_OUTBOUND);

	// Size format
	RegisterOption(OPTION_SIZE_FORMAT);
	RegisterOption(OPTION_SIZE_USETHOUSANDSEP);
	RegisterOption(OPTION_SIZE_DECIMALPLACES);

	RegisterOption(OPTION_ASCIIBINARY);

	// Reload icons
	RegisterOption(OPTION_THEME);

	CContextManager::Get()->RegisterHandler(this, STATECHANGE_SERVER, true, false);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_CHANGEDCONTEXT, false, false);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_ENCRYPTION, true, false);

	m_pDataTypeIndicator = 0;
	m_pEncryptionIndicator = 0;
	m_pSpeedLimitsIndicator = 0;

	m_size = 0;
	m_hasUnknownFiles = false;

	const int count = 3;
	SetFieldsCount(count);
	int array[count];
	array[0] = wxSB_FLAT;
	array[1] = wxSB_NORMAL;
	array[2] = wxSB_FLAT;
	SetStatusStyles(count, array);

	SetStatusWidths(count, statbarWidths);

	UpdateSizeFormat();

	UpdateSpeedLimitsIcon();
	DisplayDataType();
	DisplayEncrypted();
}

CStatusBar::~CStatusBar()
{
}

void CStatusBar::DisplayQueueSize(int64_t totalSize, bool hasUnknown)
{
	m_size = totalSize;
	m_hasUnknownFiles = hasUnknown;

	if (totalSize == 0 && !hasUnknown)
	{
		SetStatusText(_("Queue: empty"), FIELD_QUEUESIZE);
		return;
	}

	wxString queueSize = wxString::Format(_("Queue: %s%s"), hasUnknown ? _T(">") : _T(""),
		CSizeFormat::Format(totalSize, true, m_sizeFormat, m_sizeFormatThousandsSep, m_sizeFormatDecimalPlaces));

	SetStatusText(queueSize, FIELD_QUEUESIZE);
}

void CStatusBar::DisplayDataType()
{
	const CServer* pServer = 0;
	const CState* pState = CContextManager::Get()->GetCurrentContext();
	if (pState)
		pServer = pState->GetServer();

	if (!pServer || !CServer::ProtocolHasDataTypeConcept(pServer->GetProtocol()))
	{
		if (m_pDataTypeIndicator)
		{
			RemoveField(widget_datatype);
			m_pDataTypeIndicator->Destroy();
			m_pDataTypeIndicator = 0;
		}
	}
	else
	{
		wxString name;
		wxString desc;

		const int type = COptions::Get()->GetOptionVal(OPTION_ASCIIBINARY);
		if (type == 1)
		{
			name = _T("ART_ASCII");
			desc = _("Current transfer type is set to ASCII.");
		}
		else if (type == 2)
		{
			name = _T("ART_BINARY");
			desc = _("Current transfer type is set to binary.");
		}
		else
		{
			name = _T("ART_AUTO");
			desc = _("Current transfer type is set to automatic detection.");
		}

		wxBitmap bmp = wxArtProvider::GetBitmap(name, wxART_OTHER, CThemeProvider::GetIconSize(iconSizeSmall));
		if (!m_pDataTypeIndicator)
		{
			m_pDataTypeIndicator = new CIndicator(this, bmp);
			AddField(0, widget_datatype, m_pDataTypeIndicator);
		}
		else
			m_pDataTypeIndicator->SetBitmap(bmp);
		m_pDataTypeIndicator->SetToolTip(desc);
	}
}

void CStatusBar::MeasureQueueSizeWidth()
{
	wxClientDC dc(this);
	dc.SetFont(GetFont());

	wxSize s = dc.GetTextExtent(_("Queue: empty"));

	wxString tmp = _T(">8888");
	if (m_sizeFormatDecimalPlaces)
	{
		tmp += _T(".");
		for (int i = 0; i < m_sizeFormatDecimalPlaces; i++)
			tmp += _T("8");
	}
	s.IncTo(dc.GetTextExtent(wxString::Format(_("Queue: %s MiB"), tmp)));

	SetFieldWidth(FIELD_QUEUESIZE, s.x + 10);
}

void CStatusBar::DisplayEncrypted()
{
	const CServer* pServer = 0;
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (pState)
		pServer = pState->GetServer();

	bool encrypted = false;
	if (pServer) {
		CCertificateNotification* info;
		if (pServer->GetProtocol() == FTPS || pServer->GetProtocol() == FTPES || pServer->GetProtocol() == SFTP) {
			encrypted = true;
		}
		else if (pServer->GetProtocol() == FTP && pState->GetSecurityInfo(info)) {
			encrypted = true;
		}
	}

	if (!encrypted) {
		if (m_pEncryptionIndicator) {
			RemoveField(widget_encryption);
			m_pEncryptionIndicator->Destroy();
			m_pEncryptionIndicator = 0;
		}
	}
	else {
		wxBitmap bmp = wxArtProvider::GetBitmap(_T("ART_LOCK"), wxART_OTHER,  CThemeProvider::GetIconSize(iconSizeSmall));
		if (!m_pEncryptionIndicator) {
			m_pEncryptionIndicator = new CIndicator(this, bmp);
			AddField(0, widget_encryption, m_pEncryptionIndicator);
			m_pEncryptionIndicator->SetToolTip(_("The connection is encrypted. Click icon for details."));
		}
		else
			m_pEncryptionIndicator->SetBitmap(bmp);
	}
}

void CStatusBar::UpdateSizeFormat()
{
	// 0 equals bytes, however just use IEC binary prefixes instead,
	// exact byte counts for queue make no sense.
	m_sizeFormat = CSizeFormat::_format(COptions::Get()->GetOptionVal(OPTION_SIZE_FORMAT));
	if (!m_sizeFormat)
		m_sizeFormat = CSizeFormat::iec;

	m_sizeFormatThousandsSep = COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0;
	m_sizeFormatDecimalPlaces = COptions::Get()->GetOptionVal(OPTION_SIZE_DECIMALPLACES);

	MeasureQueueSizeWidth();

	DisplayQueueSize(m_size, m_hasUnknownFiles);
}

void CStatusBar::OnHandleLeftClick(wxWindow* pWnd)
{
	if (pWnd == m_pEncryptionIndicator) {
		CState* pState = CContextManager::Get()->GetCurrentContext();
		CCertificateNotification *pCertificateNotification = 0;
		CSftpEncryptionNotification *pSftpEncryptionNotification = 0;
		if (pState->GetSecurityInfo(pCertificateNotification)) {
			CVerifyCertDialog dlg;
			dlg.ShowVerificationDialog(*pCertificateNotification, true);
		}
		else if (pState->GetSecurityInfo(pSftpEncryptionNotification)) {
			CSftpEncryptioInfoDialog dlg;
			dlg.ShowDialog(pSftpEncryptionNotification);
		}
		else
			wxMessageBoxEx(_("Certificate and session data are not available yet."), _("Security information"));
	}
	else if (pWnd == m_pSpeedLimitsIndicator) {
		CSpeedLimitsDialog dlg;
		dlg.Run(m_pParent);
	}
	else if (pWnd == m_pDataTypeIndicator) {
		ShowDataTypeMenu();
	}
}

void CStatusBar::OnHandleRightClick(wxWindow* pWnd)
{
	if (pWnd == m_pDataTypeIndicator) {
		ShowDataTypeMenu();
	}
	else if (pWnd == m_pSpeedLimitsIndicator) {
		wxMenu* pMenu = wxXmlResource::Get()->LoadMenu(_T("ID_MENU_SPEEDLIMITCONTEXT"));
		if (!pMenu)
			return;

		int downloadlimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_INBOUND);
		int uploadlimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_OUTBOUND);
		bool enable = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_ENABLE) != 0;
		if (!downloadlimit && !uploadlimit)
			enable = false;
		pMenu->Check(XRCID("ID_SPEEDLIMITCONTEXT_ENABLE"), enable);

		PopupMenu(pMenu);
		delete pMenu;
	}
}

void CStatusBar::ShowDataTypeMenu()
{
	wxMenu* pMenu = wxXmlResource::Get()->LoadMenu(_T("ID_MENU_TRANSFER_TYPE_CONTEXT"));
	if (!pMenu)
		return;

	const int type = COptions::Get()->GetOptionVal(OPTION_ASCIIBINARY);
	switch (type)
	{
	case 1:
		pMenu->Check(XRCID("ID_MENU_TRANSFER_TYPE_ASCII"), true);
		break;
	case 2:
		pMenu->Check(XRCID("ID_MENU_TRANSFER_TYPE_BINARY"), true);
		break;
	default:
		pMenu->Check(XRCID("ID_MENU_TRANSFER_TYPE_AUTO"), true);
		break;
	}

	PopupMenu(pMenu);
	delete pMenu;
}

void CStatusBar::UpdateSpeedLimitsIcon()
{
	bool enable = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_ENABLE) != 0;

	wxBitmap bmp = wxArtProvider::GetBitmap(_T("ART_SPEEDLIMITS"), wxART_OTHER, CThemeProvider::GetIconSize(iconSizeSmall));
	if (!bmp.Ok()) {
		return;
	}
	wxString tooltip;

	int downloadLimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_INBOUND);
	int uploadLimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_OUTBOUND);
	if (!enable || (!downloadLimit && !uploadLimit)) {
		wxImage img = bmp.ConvertToImage();
		img = img.ConvertToGreyscale();
		bmp = wxBitmap(img);
		tooltip = _("Speed limits are disabled, click to change.");
	}
	else {
		tooltip = _("Speed limits are enabled, click to change.");
		tooltip += _T("\n");
		if (downloadLimit)
			tooltip += wxString::Format(_("Download limit: %s/s"), CSizeFormat::FormatUnit(downloadLimit, CSizeFormat::kilo));
		else
			tooltip += _("Download limit: none");
		tooltip += _T("\n");
		if (uploadLimit)
			tooltip += wxString::Format(_("Upload limit: %s/s"), CSizeFormat::FormatUnit(uploadLimit, CSizeFormat::kilo));
		else
			tooltip += _("Upload limit: none");
	}

	if (!m_pSpeedLimitsIndicator) {
		m_pSpeedLimitsIndicator = new CIndicator(this, bmp);
		AddField(0, widget_speedlimit, m_pSpeedLimitsIndicator);
	}
	else
		m_pSpeedLimitsIndicator->SetBitmap(bmp);
	m_pSpeedLimitsIndicator->SetToolTip(tooltip);
}

void CStatusBar::OnOptionsChanged(changed_options_t const& options)
{
	if (options.test(OPTION_SPEEDLIMIT_ENABLE) || options.test(OPTION_SPEEDLIMIT_INBOUND) || options.test(OPTION_SPEEDLIMIT_OUTBOUND)) {
		UpdateSpeedLimitsIcon();
	}
	if (options.test(OPTION_SIZE_FORMAT) || options.test(OPTION_SIZE_USETHOUSANDSEP) || options.test(OPTION_SIZE_DECIMALPLACES)) {
		UpdateSizeFormat();
	}
	if (options.test(OPTION_ASCIIBINARY)) {
		DisplayDataType();
	}
	if (options.test(OPTION_THEME)) {
		DisplayDataType();
		UpdateSpeedLimitsIcon();
		DisplayEncrypted();
	}
}

void CStatusBar::OnStateChange(CState*, enum t_statechange_notifications notification, const wxString&, const void*)
{
	if (notification == STATECHANGE_SERVER || notification == STATECHANGE_CHANGEDCONTEXT) {
		DisplayDataType();
		DisplayEncrypted();
	}
	else if (notification == STATECHANGE_ENCRYPTION) {
		DisplayEncrypted();
	}
}

void CStatusBar::OnSpeedLimitsEnable(wxCommandEvent&)
{
	int downloadlimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_INBOUND);
	int uploadlimit = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_OUTBOUND);
	bool enable = COptions::Get()->GetOptionVal(OPTION_SPEEDLIMIT_ENABLE) == 0;
	if (enable)
	{
		if (!downloadlimit && !uploadlimit)
		{
			CSpeedLimitsDialog dlg;
			dlg.Run(m_pParent);
		}
		else
			COptions::Get()->SetOption(OPTION_SPEEDLIMIT_ENABLE, 1);
	}
	else
		COptions::Get()->SetOption(OPTION_SPEEDLIMIT_ENABLE, 0);
}

void CStatusBar::OnSpeedLimitsConfigure(wxCommandEvent&)
{
	CSpeedLimitsDialog dlg;
	dlg.Run(m_pParent);
}
