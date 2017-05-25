#include <filezilla.h>
#include "timeformatting.h"
#include "Options.h"
#include <option_change_event_handler.h>

namespace {

class Impl : public COptionChangeEventHandler
{
public:
	Impl()
	{
		InitFormat();

		RegisterOption(OPTION_DATE_FORMAT);
		RegisterOption(OPTION_TIME_FORMAT);
	}

	void InitFormat()
	{
		wxString dateFormat = COptions::Get()->GetOption(OPTION_DATE_FORMAT);
		wxString timeFormat = COptions::Get()->GetOption(OPTION_TIME_FORMAT);

		if (dateFormat == _T("1"))
			m_dateFormat = _T("%Y-%m-%d");
		else if (!dateFormat.empty() && dateFormat[0] == '2') {
			dateFormat = dateFormat.Mid(1);
			if (CDateTime::VerifyFormat(dateFormat)) {
				m_dateFormat = dateFormat;
			}
			else {
				m_dateFormat = _T("%x");
			}
		}
		else
			m_dateFormat = _T("%x");

		m_dateTimeFormat = m_dateFormat;
		m_dateTimeFormat += ' ';

		if (timeFormat == _T("1"))
			m_dateTimeFormat += _T("%H:%M");
		else if (!timeFormat.empty() && timeFormat[0] == '2') {
			timeFormat = timeFormat.Mid(1);
			if (CDateTime::VerifyFormat(timeFormat)) {
				m_dateTimeFormat += timeFormat;
			}
			else {
				m_dateTimeFormat += _T("%X");
			}
		}
		else
			m_dateTimeFormat += _T("%X");
	}

	virtual void OnOptionsChanged(changed_options_t const&)
	{
		InitFormat();
	}

	wxString m_dateFormat;
	wxString m_dateTimeFormat;
};

Impl& GetImpl()
{
	static Impl impl;
	return impl;
}
}

wxString CTimeFormat::Format(CDateTime const& time)
{
	wxString ret;
	if( time.IsValid() ) {
		if( time.GetAccuracy() > CDateTime::days ) {
			ret = FormatDateTime(time.Degenerate());
		}
		else {
			ret = FormatDate(time.Degenerate());
		}
	}
	return ret;
}

wxString CTimeFormat::FormatDateTime(const wxDateTime &time)
{
	Impl& impl = GetImpl();

	return time.Format(impl.m_dateTimeFormat);
}

wxString CTimeFormat::FormatDate(const wxDateTime &time)
{
	Impl& impl = GetImpl();

	return time.Format(impl.m_dateFormat);
}