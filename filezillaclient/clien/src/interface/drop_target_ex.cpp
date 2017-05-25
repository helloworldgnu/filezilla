#include <filezilla.h>
#include "drop_target_ex.h"

#include "listctrlex.h"
#include "treectrlex.h"

template<class Control>
CScrollableDropTarget<Control>::CScrollableDropTarget(Control* pCtrl)
	: m_pCtrl(pCtrl)
	, m_count()
{
	m_timer.SetOwner(this);
}


template<class Control>
bool CScrollableDropTarget<Control>::OnDrop(wxCoord, wxCoord)
{
	m_timer.Stop();
	return true;
}

template<class Control>
wxDragResult CScrollableDropTarget<Control>::OnDragOver(wxCoord x, wxCoord y, wxDragResult def)
{
	def = FixupDragResult(def);
	if (!m_timer.IsRunning() && IsScroll(wxPoint(x, y))) {
		m_timer.Start(100, true);
		m_count = 0;
	}
	return def;
}

template<class Control>
void CScrollableDropTarget<Control>::OnLeave()
{
	m_timer.Stop();
}

template<class Control>
wxDragResult CScrollableDropTarget<Control>::OnEnter(wxCoord x, wxCoord y, wxDragResult def)
{
	def = FixupDragResult(def);
	if (!m_timer.IsRunning() && IsScroll(wxPoint(x, y))) {
		m_timer.Start(100, true);
		m_count = 0;
	}
	return def;
}

template<class Control>
bool CScrollableDropTarget<Control>::IsScroll(wxPoint p) const
{
	return IsTopScroll(p) || IsBottomScroll(p);
}

template<class Control>
bool CScrollableDropTarget<Control>::IsTopScroll(wxPoint p) const
{
	if (!m_pCtrl->GetItemCount()) {
		return false;
	}

	wxRect itemRect;
	if (!m_pCtrl->GetItemRect(m_pCtrl->GetTopItem(), itemRect))
		return false;

	wxRect windowRect = m_pCtrl->GetActualClientRect();

	if (itemRect.GetTop() < 0) {
		itemRect.SetTop(0);
	}
	if (itemRect.GetHeight() > windowRect.GetHeight() / 4) {
		itemRect.SetHeight(wxMax(windowRect.GetHeight() / 4, 8));
	}

	if (p.y < 0 || p.y >= itemRect.GetBottom())
		return false;

	if (p.x < 0 || p.x > windowRect.GetWidth())
		return false;

	auto top = m_pCtrl->GetTopItem();
	if (!m_pCtrl->Valid(top) || top == m_pCtrl->GetFirstItem()) {
		return false;
	}

	wxASSERT(m_pCtrl->GetTopItem() != m_pCtrl->GetFirstItem());

	return true;
}

template<class Control>
bool CScrollableDropTarget<Control>::IsBottomScroll(wxPoint p) const
{
	if (!m_pCtrl->GetItemCount()) {
		return false;
	}

	wxRect itemRect;
	if (!m_pCtrl->GetItemRect(m_pCtrl->GetFirstItem(), itemRect))
		return false;

	wxRect const windowRect = m_pCtrl->GetActualClientRect();

	int scrollHeight = itemRect.GetHeight();
	if (scrollHeight > windowRect.GetHeight() / 4) {
		scrollHeight = wxMax(windowRect.GetHeight() / 4, 8);
	}

	if (p.y > windowRect.GetBottom() || p.y < windowRect.GetBottom() - scrollHeight)
		return false;

	if (p.x < 0 || p.x > windowRect.GetWidth())
		return false;

	auto bottom = m_pCtrl->GetBottomItem();
	if (!m_pCtrl->Valid(bottom) || bottom == m_pCtrl->GetLastItem()) {
		return false;
	}

	wxASSERT(m_pCtrl->GetLastItem() != m_pCtrl->GetBottomItem());

	return true;
}

template<class Control>
void CScrollableDropTarget<Control>::OnTimer(wxTimerEvent& /*event*/)
{
	if (!m_pCtrl->GetItemCount()) {
		return;
	}

	wxPoint p = wxGetMousePosition();
	wxWindow* ctrl = m_pCtrl->GetMainWindow();
	p = ctrl->ScreenToClient(p);

	if (IsTopScroll(p)) {
		auto top = m_pCtrl->GetTopItem();
		wxASSERT(m_pCtrl->Valid(top));
		wxASSERT(top != m_pCtrl->GetFirstItem());
		m_pCtrl->EnsureVisible(m_pCtrl->GetPrevItemSimple(top));
	}
	else if (IsBottomScroll(p)) {
		auto bottom = m_pCtrl->GetBottomItem();
		wxASSERT(m_pCtrl->Valid(bottom));
		wxASSERT(bottom != m_pCtrl->GetLastItem());
		m_pCtrl->EnsureVisible(m_pCtrl->GetNextItemSimple(bottom));
	}
	else {
		return;
	}

	DisplayDropHighlight(p);

	if (m_count < 90)
		++m_count;
	m_timer.Start(100 - m_count, true);
}

template<class Control>
wxDragResult CScrollableDropTarget<Control>::FixupDragResult(wxDragResult res)
{
#ifdef __WXMAC__
	if (res == wxDragNone && wxGetKeyState(WXK_CONTROL)) {
		res = wxDragCopy;
	}
#endif

	if (res == wxDragLink) {
		res = wxGetKeyState(WXK_CONTROL) ? wxDragCopy : wxDragMove;
	}

	return res;
}

BEGIN_EVENT_TABLE_TEMPLATE1(CScrollableDropTarget, wxEvtHandler, Control)
EVT_TIMER(wxID_ANY, CScrollableDropTarget::OnTimer)
END_EVENT_TABLE()

template class CScrollableDropTarget<wxTreeCtrlEx>;
template class CScrollableDropTarget<wxListCtrlEx>;
