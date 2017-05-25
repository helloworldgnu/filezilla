#ifndef FILEZILLA_DROP_TARGET_EX_HEADER
#define FILEZILLA_DROP_TARGET_EX_HEADER

#include <wx/dnd.h>
#include <wx/timer.h>

template<class Control>
class CScrollableDropTarget : public wxEvtHandler, public wxDropTarget
{
public:
	CScrollableDropTarget(Control* pCtrl);

	virtual bool OnDrop(wxCoord x, wxCoord y);

	virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def);

	virtual void OnLeave();

	virtual wxDragResult OnEnter(wxCoord x, wxCoord y, wxDragResult def);

	virtual typename Control::Item DisplayDropHighlight(wxPoint) = 0;

protected:
	wxDragResult FixupDragResult(wxDragResult res);

	bool IsScroll(wxPoint p) const;
	bool IsTopScroll(wxPoint p) const;
	bool IsBottomScroll(wxPoint p) const;

	void OnTimer(wxTimerEvent& /*event*/);

protected:
	Control *m_pCtrl;

	wxTimer m_timer;
	int m_count;

	DECLARE_EVENT_TABLE()
};

#endif
