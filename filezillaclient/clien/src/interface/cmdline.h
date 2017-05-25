#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include <wx/cmdline.h>

class CCommandLine
{
public:
	enum t_switches
	{
		sitemanager,
		close,
		version,
		debug_startup
	};

	enum t_option
	{
		logontype,
		site,
		local
	};

	CCommandLine(int argc, wxChar** argv);
	bool Parse();
	void DisplayUsage();

	bool HasSwitch(enum t_switches s) const;
	wxString GetOption(enum t_option option) const;
	wxString GetParameter() const;

protected:
	wxCmdLineParser m_parser;
};

#endif //__CMDLINE_H__
