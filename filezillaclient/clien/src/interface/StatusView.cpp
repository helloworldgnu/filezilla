#include <filezilla.h>
#include "StatusView.h"
#include "Options.h"

#include <wx/dcclient.h>

#define MAX_LINECOUNT 1000
#define LINECOUNT_REMOVAL 10

BEGIN_EVENT_TABLE(CStatusView, wxNavigationEnabled<wxWindow>)
EVT_SIZE(CStatusView::OnSize)
EVT_MENU(XRCID("ID_CLEARALL"), CStatusView::OnClear)
EVT_MENU(XRCID("ID_COPYTOCLIPBOARD"), CStatusView::OnCopy)
END_EVENT_TABLE()

class CFastTextCtrl : public wxNavigationEnabled<wxTextCtrl>
{
public:
	CFastTextCtrl(wxWindow* parent)
	{
		Create(parent, -1, wxString(), wxDefaultPosition, wxDefaultSize,
			wxNO_BORDER | wxVSCROLL | wxTE_MULTILINE |
			wxTE_READONLY | wxTE_RICH | wxTE_RICH2 | wxTE_NOHIDESEL |
			wxTAB_TRAVERSAL);
	}
#ifdef __WXMSW__
	// wxTextCtrl::Remove is somewhat slow, this is a faster version
	virtual void Remove(long from, long to)
	{
		DoSetSelection(from, to, false);

		m_updatesCount = -2; // suppress any update event
		::SendMessage((HWND)GetHandle(), EM_REPLACESEL, 0, (LPARAM)_T(""));
	}

	void AppendText(const wxString& text, int lineCount, const CHARFORMAT2& cf)
	{
		HWND hwnd = (HWND)GetHWND();

		CHARRANGE range;
		range.cpMin = GetLastPosition();
		range.cpMax = range.cpMin;
		::SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&range);
		::SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		m_updatesCount = -2; // suppress any update event
		::SendMessage(hwnd, EM_REPLACESEL, 0, reinterpret_cast<LPARAM>(static_cast<wxChar const*>(text.c_str())));
		::SendMessage(hwnd, EM_LINESCROLL, (WPARAM)0, (LPARAM)lineCount);
	}
#endif

#ifndef __WXMAC__
	void SetDefaultColor(const wxColour& color)
	{
		m_defaultStyle.SetTextColour(color);
	}
#endif

	DECLARE_EVENT_TABLE()

	void OnText(wxCommandEvent&)
	{
		// Do nothing here.
		// Having this event handler prevents the event from propagating up the
		// window hierarchy which saves a few CPU cycles.
	}

#ifdef __WXMAC__
	void OnChar(wxKeyEvent& event)
	{
		if (event.GetKeyCode() != WXK_TAB) {
			event.Skip();
			return;
		}

		HandleAsNavigationKey(event);
	}
#endif
};

BEGIN_EVENT_TABLE(CFastTextCtrl, wxNavigationEnabled<wxTextCtrl>)
	EVT_TEXT(wxID_ANY, CFastTextCtrl::OnText)
#ifdef __WXMAC__
	EVT_CHAR_HOOK(CFastTextCtrl::OnChar)
#endif
END_EVENT_TABLE()


CStatusView::CStatusView(wxWindow* parent, wxWindowID id)
{
	Create(parent, id, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER);
	m_pTextCtrl = new CFastTextCtrl(this);

#ifdef __WXMAC__
	m_pTextCtrl->SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
#else
	m_pTextCtrl->SetFont(GetFont());
#endif

	m_pTextCtrl->Connect(wxID_ANY, wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(CStatusView::OnContextMenu), 0, this);
#ifdef __WXMSW__
	::SendMessage((HWND)m_pTextCtrl->GetHandle(), EM_SETOLECALLBACK, 0, 0);
#endif

	InitDefAttr();

	m_shown = IsShown();
}

CStatusView::~CStatusView()
{
}

void CStatusView::OnSize(wxSizeEvent &)
{
	if (m_pTextCtrl) {
		wxSize s = GetClientSize();
		m_pTextCtrl->SetSize(0, 0, s.GetWidth(), s.GetHeight());
	}
}

void CStatusView::AddToLog(CLogmsgNotification const& notification)
{
	AddToLog(notification.msgType, notification.msg, wxDateTime::Now());
}

void CStatusView::AddToLog(MessageType messagetype, const wxString& message, const wxDateTime& time)
{
	if (!m_shown) {
		if (m_hiddenLines.size() >= MAX_LINECOUNT) {
			auto it = m_hiddenLines.begin();
			it->messagetype = messagetype;
			it->message = message;
			it->time = time;
			m_hiddenLines.splice(m_hiddenLines.end(), m_hiddenLines, it );
		}
		else {
			t_line line;
			line.messagetype = messagetype;
			line.message = message;
			line.time = time;
			m_hiddenLines.push_back(line);
		}
		return;
	}

	const int messageLength = message.Length();

	wxString prefix;
	prefix.Alloc(25 + messageLength);

	if (m_nLineCount)
#ifdef __WXMSW__
		prefix = _T("\r\n");
#else
		prefix = _T("\n");
#endif

	if (m_nLineCount >= MAX_LINECOUNT) {
#ifndef __WXGTK__
		m_pTextCtrl->Freeze();
#endif //__WXGTK__
		int oldLength = 0;
		auto it = m_lineLengths.begin();
		for (int i = 0; i < LINECOUNT_REMOVAL; ++i) {
			oldLength += *(it++) + 1;
		}
		m_unusedLineLengths.splice(m_unusedLineLengths.end(), m_lineLengths, m_lineLengths.begin(), it);
		m_pTextCtrl->Remove(0, oldLength);
	}
#ifdef __WXMAC__
	if (m_pTextCtrl->GetInsertionPoint() != m_pTextCtrl->GetLastPosition()) {
		m_pTextCtrl->SetInsertionPointEnd();
	}
#endif

	int lineLength = m_attributeCache[static_cast<int>(messagetype)].len + messageLength;

	if (m_showTimestamps) {
		if (time != m_lastTime) {
			m_lastTime = time;
#ifndef __WXMAC__
			m_lastTimeString = time.Format(_T("%H:%M:%S\t"));
#else
			// Tabs on OS X cannot be freely positioned
			m_lastTimeString = time.Format(_T("%H:%M:%S "));
#endif
		}
		prefix += m_lastTimeString;
		lineLength += m_lastTimeString.Len();
	}

#ifdef __WXMAC__
	m_pTextCtrl->SetDefaultStyle(m_attributeCache[static_cast<int>(messagetype)].attr);
#elif __WXGTK__
	m_pTextCtrl->SetDefaultColor(m_attributeCache[static_cast<int>(messagetype)].attr.GetTextColour());
#endif

	prefix += m_attributeCache[static_cast<int>(messagetype)].prefix;

	if (m_rtl) {
		// Unicode control characters that control reading direction
		const wxChar LTR_MARK = 0x200e;
		//const wxChar RTL_MARK = 0x200f;
		const wxChar LTR_EMBED = 0x202A;
		//const wxChar RTL_EMBED = 0x202B;
		//const wxChar POP = 0x202c;
		//const wxChar LTR_OVERRIDE = 0x202D;
		//const wxChar RTL_OVERRIDE = 0x202E;

		if (messagetype == MessageType::Command || messagetype == MessageType::Response || messagetype >= MessageType::Debug_Warning) {
			// Commands, responses and debug message contain English text,
			// set LTR reading order for them.
			prefix += LTR_MARK;
			prefix += LTR_EMBED;
			lineLength += 2;
		}
	}

	prefix += message;
#if defined(__WXGTK__)
	// AppendText always calls SetInsertionPointEnd, which is very expensive.
	// This check however is negligible.
	if (m_pTextCtrl->GetInsertionPoint() != m_pTextCtrl->GetLastPosition())
		m_pTextCtrl->AppendText(prefix);
	else
		m_pTextCtrl->WriteText(prefix);
#elif defined(__WXMAC__)
	m_pTextCtrl->WriteText(prefix);
#else
	m_pTextCtrl->AppendText(prefix, m_nLineCount, m_attributeCache[static_cast<int>(messagetype)].cf);
#endif

	if (m_nLineCount >= MAX_LINECOUNT) {
		m_nLineCount -= LINECOUNT_REMOVAL - 1;
#ifndef __WXGTK__
		m_pTextCtrl->Thaw();
#endif
	}
	else {
		m_nLineCount++;
	}
	if (m_unusedLineLengths.empty()) {
		m_lineLengths.push_back(lineLength);
	}
	else {
		m_unusedLineLengths.front() = lineLength;
		m_lineLengths.splice(m_lineLengths.end(), m_unusedLineLengths, m_unusedLineLengths.begin());
	}
}

void CStatusView::InitDefAttr()
{
	m_showTimestamps = COptions::Get()->GetOptionVal(OPTION_MESSAGELOG_TIMESTAMP) != 0;
	m_lastTime = wxDateTime::Now();
	m_lastTimeString = m_lastTime.Format(_T("%H:%M:%S\t"));

	// Measure withs of all types
	wxClientDC dc(this);

	int timestampWidth = 0;
	if (m_showTimestamps) {
		wxCoord width = 0;
		wxCoord height = 0;
#ifndef __WXMAC__
		dc.GetTextExtent(_T("88:88:88 "), &width, &height);
#else
		dc.GetTextExtent(_T("88:88:88 "), &width, &height);
#endif
		timestampWidth = width;
	}

	wxCoord width = 0;
	wxCoord height = 0;
	dc.GetTextExtent(_("Error:") + _T(" "), &width, &height);
	int maxPrefixWidth = width;
	dc.GetTextExtent(_("Command:") + _T(" "), &width, &height);
	if (width > maxPrefixWidth)
		maxPrefixWidth = width;
	dc.GetTextExtent(_("Response:") + _T(" "), &width, &height);
	if (width > maxPrefixWidth)
		maxPrefixWidth = width;
	dc.GetTextExtent(_("Trace:") + _T(" "), &width, &height);
	if (width > maxPrefixWidth)
		maxPrefixWidth = width;
	dc.GetTextExtent(_("Listing:") + _T(" "), &width, &height);
	if (width > maxPrefixWidth)
		maxPrefixWidth = width;
	dc.GetTextExtent(_("Status:") + _T(" "), &width, &height);
	if (width > maxPrefixWidth)
		maxPrefixWidth = width;

#ifdef __WXMAC__
	wxCoord spaceWidth;
	dc.GetTextExtent(_T(" "), &spaceWidth, &height);
#endif

	dc.SetMapMode(wxMM_LOMETRIC);

	int maxWidth = dc.DeviceToLogicalX(maxPrefixWidth) + 20;
	if (timestampWidth != 0) {
		timestampWidth = dc.DeviceToLogicalX(timestampWidth) + 20;
		maxWidth += timestampWidth;
	}
	wxArrayInt array;
#ifndef __WXMAC__
	if (timestampWidth != 0)
		array.Add(timestampWidth);
#endif
	array.Add(maxWidth);
	wxTextAttr defAttr;
	defAttr.SetTabs(array);
	defAttr.SetLeftIndent(0, maxWidth);
	m_pTextCtrl->SetDefaultStyle(defAttr);
#ifdef __WXMSW__
	m_pTextCtrl->SetStyle(0, 0, defAttr);
#endif

	const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX);
	const bool is_dark = background.Red() + background.Green() + background.Blue() < 384;

	for (int i = 0; i < static_cast<int>(MessageType::count); i++) {
		t_attributeCache& entry = m_attributeCache[i];
#ifndef __WXMAC__
		entry.attr = defAttr;
#endif
		switch (static_cast<MessageType>(i)) {
		case MessageType::Error:
			entry.prefix = _("Error:");
			entry.attr.SetTextColour(wxColour(255, 0, 0));
			break;
		case MessageType::Command:
			entry.prefix = _("Command:");
			if (is_dark)
				entry.attr.SetTextColour(wxColour(128, 128, 255));
			else
				entry.attr.SetTextColour(wxColour(0, 0, 128));
			break;
		case MessageType::Response:
			entry.prefix = _("Response:");
			if (is_dark)
				entry.attr.SetTextColour(wxColour(128, 255, 128));
			else
				entry.attr.SetTextColour(wxColour(0, 128, 0));
			break;
		case MessageType::Debug_Warning:
		case MessageType::Debug_Info:
		case MessageType::Debug_Verbose:
		case MessageType::Debug_Debug:
			entry.prefix = _("Trace:");
			if (is_dark)
				entry.attr.SetTextColour(wxColour(255, 128, 255));
			else
				entry.attr.SetTextColour(wxColour(128, 0, 128));
			break;
		case MessageType::RawList:
			entry.prefix = _("Listing:");
			if (is_dark)
				entry.attr.SetTextColour(wxColour(128, 255, 255));
			else
				entry.attr.SetTextColour(wxColour(0, 128, 128));
			break;
		default:
			entry.prefix = _("Status:");
			entry.attr.SetTextColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
			break;
		}

#ifdef __WXMAC__
		// Fill with blanks to approach best size
		dc.GetTextExtent(entry.prefix, &width, &height);
		wxASSERT(width <= maxPrefixWidth);
		wxCoord spaces = (maxPrefixWidth - width) / spaceWidth;
		entry.prefix += wxString(spaces, ' ');
#endif
		entry.prefix += _T("\t");
		entry.len = entry.prefix.Length();

#ifdef __WXMSW__
		m_pTextCtrl->SetStyle(0, 0, entry.attr);
		entry.cf.cbSize = sizeof(CHARFORMAT2);
		::SendMessage((HWND)m_pTextCtrl->GetHWND(), EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&entry.cf);
#endif
	}

	m_rtl = wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft;
}

void CStatusView::OnContextMenu(wxContextMenuEvent&)
{
	wxMenu* pMenu = wxXmlResource::Get()->LoadMenu(_T("ID_MENU_LOG"));
	if (!pMenu)
		return;

	pMenu->Check(XRCID("ID_SHOW_DETAILED_LOG"), COptions::Get()->GetOptionVal(OPTION_LOGGING_SHOW_DETAILED_LOGS) != 0);

	PopupMenu(pMenu);

	COptions::Get()->SetOption(OPTION_LOGGING_SHOW_DETAILED_LOGS, pMenu->IsChecked(XRCID("ID_SHOW_DETAILED_LOG")) ? 1 : 0);
	delete pMenu;
}

void CStatusView::OnClear(wxCommandEvent&)
{
	if (m_pTextCtrl)
		m_pTextCtrl->Clear();
	m_nLineCount = 0;
	m_lineLengths.clear();
}

void CStatusView::OnCopy(wxCommandEvent&)
{
	if (!m_pTextCtrl)
		return;

	long from, to;
	m_pTextCtrl->GetSelection(&from, &to);
	if (from != to)
		m_pTextCtrl->Copy();
	else {
		m_pTextCtrl->Freeze();
		m_pTextCtrl->SetSelection(-1, -1);
		m_pTextCtrl->Copy();
		m_pTextCtrl->SetSelection(from, to);
		m_pTextCtrl->Thaw();
	}
}

void CStatusView::SetFocus()
{
	m_pTextCtrl->SetFocus();
}

bool CStatusView::Show(bool show /*=true*/)
{
	m_shown = show;

	if (show && m_pTextCtrl) {
		if (m_hiddenLines.size() >= MAX_LINECOUNT) {
			m_pTextCtrl->Clear();
			m_nLineCount = 0;
			m_unusedLineLengths.splice(m_unusedLineLengths.end(), m_lineLengths, m_lineLengths.begin(), m_lineLengths.end());
		}

		for (auto const& line : m_hiddenLines) {
			AddToLog(line.messagetype, line.message, line.time);
		}
		m_hiddenLines.clear();
	}

	return wxWindow::Show(show);
}
