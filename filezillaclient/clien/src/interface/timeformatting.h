#ifndef __TIMEFORMATTING_H__
#define __TIMEFORMATTING_H__

class CDateTime;

class CTimeFormat
{
public:
	static wxString Format(CDateTime const& time);
	static wxString FormatDateTime(wxDateTime const& time);
	static wxString FormatDate(wxDateTime const& time);
};

#endif //__TIMEFORMATTING_H__
