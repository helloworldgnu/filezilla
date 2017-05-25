#ifndef FILEZILLA_MSGBOX_HEADER
#define FILEZILLA_MSGBOX_HEADER

#include <wx/msgdlg.h>

bool IsShowingMessageBox();

int wxMessageBoxEx(const wxString& message, const wxString& caption = wxMessageBoxCaptionStr
	, long style = wxOK | wxCENTRE, wxWindow *parent = NULL
	, int x = wxDefaultCoord, int y = wxDefaultCoord);

#endif