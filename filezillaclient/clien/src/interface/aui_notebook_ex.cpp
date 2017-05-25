#include <filezilla.h>
#include <wx/aui/aui.h>
#include "aui_notebook_ex.h"
#include <wx/dcmirror.h>

#include <memory>

#ifdef __WXMSW__
#define TABCOLOUR wxSYS_COLOUR_3DFACE
#else
#define TABCOLOUR wxSYS_COLOUR_WINDOWFRAME
#endif

struct wxAuiTabArtExData
{
	std::map<wxString, int> maxSizes;
};

class wxAuiTabArtEx : public wxAuiDefaultTabArt
{
public:
	wxAuiTabArtEx(wxAuiNotebookEx* pNotebook, std::shared_ptr<wxAuiTabArtExData> const& data)
		: m_pNotebook(pNotebook)
		, m_data(data)
	{
	}

	virtual wxAuiTabArt* Clone()
	{
		wxAuiTabArtEx *art = new wxAuiTabArtEx(m_pNotebook, m_data);
		art->SetNormalFont(m_normalFont);
		art->SetSelectedFont(m_selectedFont);
		art->SetMeasuringFont(m_measuringFont);
		return art;
	}

	virtual wxSize GetTabSize(wxDC& dc, wxWindow* wnd, const wxString& caption, const wxBitmap& bitmap, bool active, int close_button_state, int* x_extent)
	{
		wxSize size = wxAuiDefaultTabArt::GetTabSize(dc, wnd, caption, bitmap, active, close_button_state, x_extent);

		wxString text = caption;
		int pos;
		if ((pos = caption.Find(_T(" ("))) != -1)
			text = text.Left(pos);
		auto iter = m_data->maxSizes.find(text);
		if (iter == m_data->maxSizes.end())
			m_data->maxSizes[text] = size.x;
		else
		{
			if (iter->second > size.x)
			{
				size.x = iter->second;
				*x_extent = size.x;
			}
			else
				iter->second = size.x;
		}

		return size;
	}

protected:
	wxAuiNotebookEx* m_pNotebook;

	std::shared_ptr<wxAuiTabArtExData> m_data;
};

BEGIN_EVENT_TABLE(wxAuiNotebookEx, wxAuiNotebook)
EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, wxAuiNotebookEx::OnPageChanged)
END_EVENT_TABLE()

wxAuiNotebookEx::wxAuiNotebookEx()
{
}

wxAuiNotebookEx::~wxAuiNotebookEx()
{
}

void wxAuiNotebookEx::RemoveExtraBorders()
{
	wxAuiPaneInfoArray& panes = m_mgr.GetAllPanes();
	for (size_t i = 0; i < panes.Count(); i++)
	{
		panes[i].PaneBorder(false);
	}
	m_mgr.Update();
}

void wxAuiNotebookEx::SetExArtProvider()
{
	SetArtProvider(new wxAuiTabArtEx(this, std::make_shared<wxAuiTabArtExData>()));
}

bool wxAuiNotebookEx::SetPageText(size_t page_idx, const wxString& text)
{
	// Basically identical to the AUI one, but not calling Update
	if (page_idx >= m_tabs.GetPageCount())
		return false;

	// update our own tab catalog
	wxAuiNotebookPage& page_info = m_tabs.GetPage(page_idx);
	page_info.caption = text;

	// update what's on screen
	wxAuiTabCtrl* ctrl;
	int ctrl_idx;
	if (FindTab(page_info.window, &ctrl, &ctrl_idx)) {
		wxAuiNotebookPage& info = ctrl->GetPage(ctrl_idx);
		info.caption = text;
		ctrl->Refresh();
	}

	return true;
}

void wxAuiNotebookEx::Highlight(size_t page, bool highlight /*=true*/)
{
	if (GetSelection() == (int)page)
		return;

	wxASSERT(page < m_tabs.GetPageCount());
	if (page >= m_tabs.GetPageCount())
		return;

	if (page >= m_highlighted.size())
		m_highlighted.resize(page + 1, false);

	if (highlight == m_highlighted[page])
		return;

	m_highlighted[page] = highlight;

	GetActiveTabCtrl()->Refresh();
}

bool wxAuiNotebookEx::Highlighted(size_t page) const
{
	wxASSERT(page < m_tabs.GetPageCount());
	if (page >= m_highlighted.size())
		return false;

	return m_highlighted[page];
}

void wxAuiNotebookEx::OnPageChanged(wxAuiNotebookEvent&)
{
	size_t page = (size_t)GetSelection();
	if (page >= m_highlighted.size())
		return;

	m_highlighted[page] = false;
}

void wxAuiNotebookEx::AdvanceTab(bool forward)
{
	int page = GetSelection();
	if (forward)
		++page;
	else
		--page;
	if (page >= (int)GetPageCount())
		page = 0;
	else if (page < 0)
		page = GetPageCount() - 1;

	SetSelection(page);
}

bool wxAuiNotebookEx::AddPage(wxWindow *page, const wxString &text, bool select, int imageId)
{
	bool const res = wxAuiNotebook::AddPage(page, text, select, imageId);
	size_t const count = GetPageCount();

	if (count > 1) {
		GetPage(count - 1)->MoveAfterInTabOrder(GetPage(count - 2));
	}

	if (GetWindowStyle() & wxAUI_NB_BOTTOM) {
		GetActiveTabCtrl()->MoveAfterInTabOrder(GetPage(count - 1));
	}
	else {
		GetActiveTabCtrl()->MoveBeforeInTabOrder(GetPage(0));
	}

	return res;
}
