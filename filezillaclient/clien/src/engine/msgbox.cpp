#include <filezilla.h>

#include "msgbox.h"

namespace {
int openMessageBoxes = 0;
}

bool IsShowingMessageBox()
{
	return openMessageBoxes != 0;
}

int wxMessageBoxEx(const wxString& message, const wxString& caption
	, long style, wxWindow *parent
	, int x, int y)
{
	++openMessageBoxes;
	int ret = wxMessageBox(message, caption, style, parent, x, y);
	--openMessageBoxes;

	return ret;
}
