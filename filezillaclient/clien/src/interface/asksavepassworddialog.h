#ifndef __ASKSAVEPASSWORDDIALOG_H__
#define __ASKSAVEPASSWORDDIALOG_H__

#include "dialogex.h"

class CAskSavePasswordDialog : public wxDialogEx
{
public:
	static bool Run(wxWindow* parent);
private:
	bool Create(wxWindow* parent);

	DECLARE_EVENT_TABLE()
	void OnRadioButtonChanged(wxCommandEvent& event);
};

#endif
