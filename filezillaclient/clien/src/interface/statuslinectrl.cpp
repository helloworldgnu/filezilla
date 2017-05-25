#include <filezilla.h>
#include "queue.h"
#include "statuslinectrl.h"
#include <wx/dcbuffer.h>
#include "Options.h"
#include "sizeformatting.h"

#include <algorithm>

BEGIN_EVENT_TABLE(CStatusLineCtrl, wxWindow)
EVT_PAINT(CStatusLineCtrl::OnPaint)
EVT_TIMER(wxID_ANY, CStatusLineCtrl::OnTimer)
EVT_ERASE_BACKGROUND(CStatusLineCtrl::OnEraseBackground)
END_EVENT_TABLE()

int CStatusLineCtrl::m_fieldOffsets[4];
wxCoord CStatusLineCtrl::m_textHeight;
bool CStatusLineCtrl::m_initialized = false;

#define PROGRESSBAR_WIDTH 102

CStatusLineCtrl::CStatusLineCtrl(CQueueView* pParent, const t_EngineData* const pEngineData, const wxRect& initialPosition)
	: m_pParent(pParent)
	, m_pEngineData(pEngineData)
	, m_gcLastTimeStamp(wxDateTime::Now())
{
	wxASSERT(pEngineData);

	Create(pParent->GetMainWindow(), wxID_ANY, initialPosition.GetPosition(), initialPosition.GetSize());

	SetOwnFont(pParent->GetFont());
	SetForegroundColour(pParent->GetForegroundColour());
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);
	SetBackgroundColour(pParent->GetBackgroundColour());

	m_transferStatusTimer.SetOwner(this);

	InitFieldOffsets();

	ClearTransferStatus();
}

void CStatusLineCtrl::InitFieldOffsets()
{
	if (m_initialized)
		return;
	m_initialized = true;

	// Calculate field widths so that the contents fit under every language.
	wxClientDC dc(this);
	dc.SetFont(GetFont());

	wxCoord w, h;
	wxTimeSpan elapsed(100, 0, 0);
	// @translator: This is a date/time formatting specifier. See https://wiki.filezilla-project.org/Date_and_Time_formatting
	dc.GetTextExtent(elapsed.Format(_("%H:%M:%S elapsed")), &w, &h);
	m_textHeight = h;
	m_fieldOffsets[0] = 50 + w;

	// @translator: This is a date/time formatting specifier. See https://wiki.filezilla-project.org/Date_and_Time_formatting
	dc.GetTextExtent(elapsed.Format(_("%H:%M:%S left")), &w, &h);
	m_fieldOffsets[1] = m_fieldOffsets[0] + 20 + w;

	m_fieldOffsets[2] = m_fieldOffsets[1] + 20;
	m_fieldOffsets[3] = m_fieldOffsets[2] + PROGRESSBAR_WIDTH + 20;
}

CStatusLineCtrl::~CStatusLineCtrl()
{
	if (!status_.empty() && status_.totalSize >= 0)
		m_pEngineData->pItem->SetSize(status_.totalSize);

	if (m_transferStatusTimer.IsRunning())
		m_transferStatusTimer.Stop();
}

void CStatusLineCtrl::OnPaint(wxPaintEvent&)
{
	wxPaintDC dc(this);

	wxRect rect = GetRect();

	int refresh = 0;
	if (!m_data.IsOk() || rect.GetWidth() != m_data.GetWidth() || rect.GetHeight() != m_data.GetHeight()) {
		m_mdc.reset();
		m_data = wxBitmap(rect.GetWidth(), rect.GetHeight());
		m_mdc = make_unique<wxMemoryDC>(m_data);
		// Use same layout direction as the DC which bitmap is drawn on.
		// This avoids problem with mirrored characters on RTL locales.
		m_mdc->SetLayoutDirection(dc.GetLayoutDirection());
		refresh = 31;
	}

	wxTimeSpan elapsed;
	int left = -1;
	wxFileOffset rate;
	wxString bytes_and_rate;
	int bar_split = -1;
	int permill = -1;

	if (status_.empty()) {
		if (m_previousStatusText != m_statusText) {
			// Clear background
			m_mdc->SetFont(GetFont());
			m_mdc->SetPen(GetBackgroundColour());
			m_mdc->SetBrush(GetBackgroundColour());
			m_mdc->SetTextForeground(GetForegroundColour());
			m_mdc->DrawRectangle(0, 0, rect.GetWidth(), rect.GetHeight());
			wxCoord h = (rect.GetHeight() - m_textHeight) / 2;
			m_mdc->DrawText(m_statusText, 50, h);
			m_previousStatusText = m_statusText;
			refresh = 0;
		}
	}
	else {
		if (!m_previousStatusText.empty()) {
			m_previousStatusText.clear();
			refresh = 31;
		}

		int elapsed_milli_seconds = 0;
		if (status_.started.IsValid()) {
			elapsed = wxDateTime::UNow().Subtract(status_.started);
			elapsed_milli_seconds = elapsed.GetMilliseconds().GetLo(); // Assume GetHi is always 0
		}

		if (elapsed_milli_seconds / 1000 != m_last_elapsed_seconds) {
			refresh |= 1;
			m_last_elapsed_seconds = elapsed_milli_seconds / 1000;
		}

		if (COptions::Get()->GetOptionVal(OPTION_SPEED_DISPLAY))
			rate = GetCurrentSpeed();
		else
			rate = GetSpeed(elapsed_milli_seconds);

		if (status_.totalSize > 0 && elapsed_milli_seconds >= 1000 && rate > 0) {
			wxFileOffset r = status_.totalSize - status_.currentOffset;
			left = r / rate + 1;
			if (r)
				++left;

			if (left < 0)
				left = 0;
		}

		if (m_last_left != left) {
			refresh |= 2;
			m_last_left = left;
		}

		const wxString bytestr = CSizeFormat::Format(status_.currentOffset, true, CSizeFormat::bytes, COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0, 0);
		if (elapsed_milli_seconds >= 1000 && rate > -1) {
			CSizeFormat::_format format = static_cast<CSizeFormat::_format>(COptions::Get()->GetOptionVal(OPTION_SIZE_FORMAT));
			if (format == CSizeFormat::bytes)
				format = CSizeFormat::iec;
			const wxString ratestr = CSizeFormat::Format(rate, true,
														 format,
														 COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0,
														 COptions::Get()->GetOptionVal(OPTION_SIZE_DECIMALPLACES));
			bytes_and_rate.Printf(_("%s (%s/s)"), bytestr, ratestr );
		}
		else
			bytes_and_rate.Printf(_("%s (? B/s)"), bytestr);

		if (m_last_bytes_and_rate != bytes_and_rate) {
			refresh |= 8;
			m_last_bytes_and_rate = bytes_and_rate;
		}

		if (status_.totalSize > 0) {
			bar_split = static_cast<int>(status_.currentOffset * (PROGRESSBAR_WIDTH - 2) / status_.totalSize);
			if (bar_split > (PROGRESSBAR_WIDTH - 2))
				bar_split = PROGRESSBAR_WIDTH - 2;

			if (status_.currentOffset > status_.totalSize)
				permill = 1001;
			else
				permill = static_cast<int>(status_.currentOffset * 1000 / status_.totalSize);
		}

		if (m_last_bar_split != bar_split || m_last_permill != permill) {
			refresh |= 4;
			m_last_bar_split = bar_split;
			m_last_permill = permill;
		}
	}

	if (refresh) {
		m_mdc->SetFont(GetFont());
		m_mdc->SetPen(GetBackgroundColour());
		m_mdc->SetBrush(GetBackgroundColour());
		m_mdc->SetTextForeground(GetForegroundColour());

		// Get character height so that we can center the text vertically.
		wxCoord h = (rect.GetHeight() - m_textHeight) / 2;

		if (refresh & 1) {
			m_mdc->DrawRectangle(0, 0, m_fieldOffsets[0], rect.GetHeight());
			DrawRightAlignedText(*m_mdc, elapsed.Format(_("%H:%M:%S elapsed")), m_fieldOffsets[0], h);
		}
		if (refresh & 2) {
			m_mdc->DrawRectangle(m_fieldOffsets[0], 0, m_fieldOffsets[1] - m_fieldOffsets[0], rect.GetHeight());
			if (left != -1) {
				wxTimeSpan timeLeft(0, 0, left);
				DrawRightAlignedText(*m_mdc, timeLeft.Format(_("%H:%M:%S left")), m_fieldOffsets[1], h);
			}
			else
				DrawRightAlignedText(*m_mdc, _("--:--:-- left"), m_fieldOffsets[1], h);
		}
		if (refresh & 8) {
			m_mdc->DrawRectangle(m_fieldOffsets[3], 0, rect.GetWidth() - m_fieldOffsets[3], rect.GetHeight());
			m_mdc->DrawText(bytes_and_rate, m_fieldOffsets[3], h);
		}
		if (refresh & 16) {
			m_mdc->DrawRectangle(m_fieldOffsets[1], 0, m_fieldOffsets[2] - m_fieldOffsets[1], rect.GetHeight());
		}
		if (refresh & 4) {
			m_mdc->DrawRectangle(m_fieldOffsets[2], 0, m_fieldOffsets[3] - m_fieldOffsets[2], rect.GetHeight());
			if (bar_split != -1)
				DrawProgressBar(*m_mdc, m_fieldOffsets[2], 1, rect.GetHeight() - 2, bar_split, permill);
		}
	}
	dc.Blit(0, 0, rect.GetWidth(), rect.GetHeight(), m_mdc.get(), 0, 0);
}

void CStatusLineCtrl::ClearTransferStatus()
{
	if (!status_.empty() && status_.totalSize >= 0) {
		m_pParent->UpdateItemSize(m_pEngineData->pItem, status_.totalSize);
	}
	status_.clear();

	switch (m_pEngineData->state)
	{
	case t_EngineData::disconnect:
		m_statusText = _("Disconnecting from previous server");
		break;
	case t_EngineData::cancel:
		m_statusText = _("Waiting for transfer to be cancelled");
		break;
	case t_EngineData::connect:
		m_statusText = wxString::Format(_("Connecting to %s"), m_pEngineData->lastServer.FormatServer());
		break;
	default:
		m_statusText = _("Transferring");
		break;
	}

	if (m_transferStatusTimer.IsRunning())
		m_transferStatusTimer.Stop();

	m_past_data_count = 0;
	m_gcLastOffset = -1;
	m_gcLastSpeed = -1;
	Refresh(false);
}

void CStatusLineCtrl::SetTransferStatus(CTransferStatus const& status)
{
	if (!status) {
		ClearTransferStatus();
	}
	else {
		status_ = status;

		m_lastOffset = status.currentOffset;

		if (!m_transferStatusTimer.IsRunning())
			m_transferStatusTimer.Start(100);
		Refresh(false);
	}
}

void CStatusLineCtrl::OnTimer(wxTimerEvent&)
{
	if (!m_pEngineData || !m_pEngineData->pEngine) {
		m_transferStatusTimer.Stop();
		return;
	}

	bool changed;
	CTransferStatus status = m_pEngineData->pEngine->GetTransferStatus(changed);

	if (status.empty())
		ClearTransferStatus();
	else if (changed) {
		if (status.madeProgress && !status.list &&
			m_pEngineData->pItem->GetType() == QueueItemType::File)
		{
			CFileItem* pItem = (CFileItem*)m_pEngineData->pItem;
			pItem->set_made_progress(true);
		}
		SetTransferStatus(status);
	}
	else
		m_transferStatusTimer.Stop();
}

void CStatusLineCtrl::DrawRightAlignedText(wxDC& dc, wxString text, int x, int y)
{
	wxCoord w, h;
	dc.GetTextExtent(text, &w, &h);
	x -= w;

	dc.DrawText(text, x, y);
}

void CStatusLineCtrl::OnEraseBackground(wxEraseEvent&)
{
	// Don't erase background, only causes status line to flicker.
}

void CStatusLineCtrl::DrawProgressBar(wxDC& dc, int x, int y, int height, int bar_split, int permill)
{
	wxASSERT(bar_split != -1);
	wxASSERT(permill != -1);

	// Draw right part
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	dc.DrawRectangle(x + 1 + bar_split, y + 1, PROGRESSBAR_WIDTH - bar_split - 1, height - 2);

	if (bar_split && height > 2) {
		// Draw pretty gradient

		int greenmin = 128;
		int greenmax = 255;
		int colourCount = ((height + 1) / 2);

		for (int i = 0; i < colourCount; ++i) {
			int curGreen = greenmax - ((greenmax - greenmin) * i / (colourCount - 1));
			dc.SetPen(wxPen(wxColour(0, curGreen, 0)));
			dc.DrawLine(x + 1, y + colourCount - i, x + 1 + bar_split, y + colourCount - i);
			dc.DrawLine(x + 1, y + height - colourCount + i - 1, x + 1 + bar_split, y + height - colourCount + i - 1);
		}
	}

	dc.SetPen(*wxBLACK_PEN);
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	dc.DrawRectangle(x, y, PROGRESSBAR_WIDTH, height);

	// Draw percentage-done text
	wxString text;
	if (permill > 1000) {
		text = _T("> 100.0%%");
	}
	else {
		text = wxString::Format(_T("%d.%d%%"), permill / 10, permill % 10);
	}

	wxCoord w, h;
	dc.GetTextExtent(text, &w, &h);
	dc.DrawText(text, x + PROGRESSBAR_WIDTH / 2 - w / 2, y + height / 2 - h / 2);
}

wxFileOffset CStatusLineCtrl::GetSpeed(int elapsed_milli_seconds)
{
	if (status_.empty())
		return -1;

	if (elapsed_milli_seconds <= 0) {
		return -1;
	}

	int elapsed_seconds = elapsed_milli_seconds / 1000;
	while (m_past_data_count < 10 && elapsed_seconds > m_past_data_count) {
		m_past_data[m_past_data_count].elapsed = elapsed_milli_seconds;
		m_past_data[m_past_data_count].offset = status_.currentOffset - status_.startOffset;
		++m_past_data_count;
	}

	_past_data forget;

	int offset = (elapsed_seconds - 1) / 2;
	if (offset > 0) {
		forget = m_past_data[std::min(offset, m_past_data_count - 1)];
	}

	if (elapsed_milli_seconds <= forget.elapsed) {
		return -1;
	}

	return ((status_.currentOffset - status_.startOffset - forget.offset) * 1000) / (elapsed_milli_seconds - forget.elapsed);
}

wxFileOffset CStatusLineCtrl::GetCurrentSpeed()
{
	if (status_.empty())
		return -1;

	const wxTimeSpan timeDiff( wxDateTime::UNow().Subtract(m_gcLastTimeStamp) );

	if (timeDiff.GetMilliseconds().GetLo() <= 2000)
		return m_gcLastSpeed;

	m_gcLastTimeStamp = wxDateTime::UNow();

	if (m_gcLastOffset < 0)
		m_gcLastOffset = status_.startOffset;

	const wxFileOffset fileOffsetDiff = status_.currentOffset - m_gcLastOffset;
	m_gcLastOffset = status_.currentOffset;
	if( fileOffsetDiff >= 0 ) {
		m_gcLastSpeed = fileOffsetDiff * 1000 / timeDiff.GetMilliseconds().GetLo();
	}

	return m_gcLastSpeed;
}

bool CStatusLineCtrl::Show(bool show /*=true*/)
{
	if (show) {
		if (!m_transferStatusTimer.IsRunning())
			m_transferStatusTimer.Start(100);
	}
	else
		m_transferStatusTimer.Stop();

	return wxWindow::Show(show);
}

void CStatusLineCtrl::SetEngineData(const t_EngineData* const pEngineData)
{
	wxASSERT(pEngineData);
	m_pEngineData = pEngineData;
}
