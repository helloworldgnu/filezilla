#ifndef __DIALOGEX_H__
#define __DIALOGEX_H__

#include "wrapengine.h"

class wxDialogEx : public wxDialog, public CWrapEngine
{
public:
	bool Load(wxWindow *pParent, const wxString& name);

	bool SetChildLabel(int id, const wxString& label, unsigned long maxLength = 0);
	wxString GetChildLabel(int id);

	virtual int ShowModal();

	bool ReplaceControl(wxWindow* old, wxWindow* wnd);

	static int ShownDialogs() { return m_shown_dialogs; }

	static bool CanShowPopupDialog();
protected:
	virtual void InitDialog();

	DECLARE_EVENT_TABLE()
	virtual void OnChar(wxKeyEvent& event);
	void OnMenuEvent(wxCommandEvent& event);

#ifdef __WXMAC__
	virtual bool ProcessEvent(wxEvent& event);
#endif

	static int m_shown_dialogs;
};

#endif //__DIALOGEX_H__
