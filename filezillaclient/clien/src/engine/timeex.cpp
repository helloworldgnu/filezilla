#include <filezilla.h>
#include "timeex.h"

#define TIME_ASSERT(x) //wxASSERT(x)

CDateTime::CDateTime()
: a_(days)
{
}

CDateTime::CDateTime( CDateTime const& op )
: t_(op.t_)
, a_(op.a_)
{
}

CDateTime::CDateTime( int year, int month, int day, int hour, int minute, int second, int millisecond )
	: a_()
{
	if( !Set( year, month, day, hour, minute, second, millisecond ) ) {
		t_ = wxDateTime();
	}
}

CDateTime::CDateTime( wxDateTime const& t, Accuracy a )
: t_(t)
, a_(a)
{
	TIME_ASSERT(IsClamped());
}

CDateTime& CDateTime::operator=( CDateTime const& op )
{
	a_ = op.a_;
	t_ = op.t_;
	return *this;
}

CDateTime CDateTime::Now()
{
	return CDateTime( wxDateTime::UNow(), milliseconds );
}

bool CDateTime::operator<( CDateTime const& op ) const
{
	if( !t_.IsValid() ) {
		return op.t_.IsValid();
	}
	else if( !op.t_.IsValid() ) {
		return false;
	}

	if( t_ < op.t_ ) {
		return true;
	}
	if( t_ > op.t_ ) {
		return false;
	}

	return a_ < op.a_;
}

bool CDateTime::operator==( CDateTime const& op ) const
{
	return t_ == op.t_ && a_ == op.a_;
}

wxTimeSpan CDateTime::operator-( CDateTime const& op ) const
{
	return t_ - op.t_;
}

bool CDateTime::IsClamped()
{
	bool ret = true;
	wxDateTime::Tm t = t_.GetTm();
	if( a_ < milliseconds && t.msec ) {
		ret = false;
	}
	else if( a_ < seconds && t.sec ) {
		ret = false;
	}
	else if( a_ < minutes && t.min ) {
		ret = false;
	}
	else if( a_ < hours && t.hour ) {
		ret = false;
	}
	return ret;
}

int CDateTime::Compare( CDateTime const& op ) const
{
	if( !t_.IsValid() ) {
		return op.t_.IsValid();
	}
	else if( !op.t_.IsValid() ) {
		return false;
	}

	if( a_ == op.a_ ) {
		// First fast path: Same accuracy
		int ret = 0;
		if( t_ < op.t_ ) {
			ret = -1;
		}
		else if( t_ > op.t_ ) {
			ret = 1;
		}
		TIME_ASSERT( CompareSlow(op) == ret );
		return ret;
	}

	// Second fast path: Lots of difference, at least 2 days
	wxLongLong diff = t_.GetValue() - op.t_.GetValue();
	if( diff > 60 * 60 * 24 * 1000 * 2 ) {
		TIME_ASSERT( CompareSlow(op) == 1 );
		return 1;
	}
	else if( diff < -60 * 60 * 24 * 1000 * 2 ) {
		TIME_ASSERT( CompareSlow(op) == -1 );
		return -1;
	}

	return CompareSlow( op );
}

int CDateTime::CompareSlow( CDateTime const& op ) const
{
	wxDateTime::Tm t1 = t_.GetTm();
	wxDateTime::Tm t2 = op.t_.GetTm();
	if( t1.year < t2.year ) {
		return -1;
	}
	else if( t1.year > t2.year ) {
		return 1;
	}
	if( t1.mon < t2.mon ) {
		return -1;
	}
	else if( t1.mon > t2.mon ) {
		return 1;
	}
	if( t1.mday < t2.mday ) {
		return -1;
	}
	else if( t1.mday > t2.mday ) {
		return 1;
	}

	Accuracy a = (a_ < op.a_ ) ? a_ : op.a_;

	if( a < hours ) {
		return 0;
	}
	if( t1.hour < t2.hour ) {
		return -1;
	}
	else if( t1.hour > t2.hour ) {
		return 1;
	}

	if( a < minutes ) {
		return 0;
	}
	if( t1.min < t2.min ) {
		return -1;
	}
	else if( t1.min > t2.min ) {
		return 1;
	}

	if( a < seconds ) {
		return 0;
	}
	if( t1.sec < t2.sec ) {
		return -1;
	}
	else if( t1.sec > t2.sec ) {
		return 1;
	}

	if( a < milliseconds ) {
		return 0;
	}
	if( t1.msec < t2.msec ) {
		return -1;
	}
	else if( t1.msec > t2.msec ) {
		return 1;
	}

	return 0;
}

CDateTime& CDateTime::operator+=( wxTimeSpan const& op )
{
	if( IsValid() ) {
		if( a_ < hours ) {
			t_ += wxTimeSpan::Days(op.GetDays());
		}
		else if( a_ < minutes ) {
			t_ += wxTimeSpan::Hours(op.GetHours());
		}
		else if( a_ < seconds ) {
			t_ += wxTimeSpan::Minutes(op.GetMinutes());
		}
		else if( a_ < milliseconds ) {
			t_ += wxTimeSpan::Seconds(op.GetSeconds());
		}
		else {
			t_ += op;
		}
	}
	return *this;
}

bool CDateTime::Set( int year, int month, int day, int hour, int minute, int second, int millisecond )
{
	if (year < 1900 || year > 3000)
		return false;

	if (month < 1 || month > 12)
		return false;

	int maxDays = wxDateTime::GetNumberOfDays(static_cast<wxDateTime::Month>(month - 1), year);
	if (day < 1 || day > maxDays)
		return false;

	if( hour == -1 ) {
		a_ = days;
		TIME_ASSERT(minute == -1);
		TIME_ASSERT(second == -1);
		TIME_ASSERT(millisecond == -1);
		hour = minute = second = millisecond = 0;
	}
	else if( minute == -1 ) {
		a_ = hours;
		TIME_ASSERT(second == -1);
		TIME_ASSERT(millisecond == -1);
		minute = second = millisecond = 0;
	}
	else if( second == -1 ) {
		a_ = minutes;
		TIME_ASSERT(millisecond == -1);
		second = millisecond = 0;
	}
	else if( millisecond == -1 ) {
		a_ = seconds;
		millisecond = 0;
	}
	else {
		a_ = milliseconds;
	}

	if( hour < 0 || hour >= 24 ) {
		return false;
	}
	if( minute < 0 || minute >= 60 ) {
		return false;
	}
	if( second < 0 || second >= 60 ) {
		return false;
	}
	if( millisecond < 0 || millisecond >= 1000 ) {
		return false;
	}

	t_.Set( day, static_cast<wxDateTime::Month>(month - 1), year, hour, minute, second, millisecond );
	return t_.IsValid();
}

bool CDateTime::ImbueTime( int hour, int minute, int second, int millisecond )
{
	if( !IsValid() || a_ > days ) {
		return false;
	}

	if( second == -1 ) {
		a_ = minutes;
		TIME_ASSERT(millisecond == -1);
		second = millisecond = 0;
	}
	else if( millisecond == -1 ) {
		a_ = seconds;
		millisecond = 0;
	}
	else {
		a_ = milliseconds;
	}

	if( hour < 0 || hour >= 24 ) {
		return false;
	}
	if( minute < 0 || minute >= 60 ) {
		return false;
	}
	if( second < 0 || second >= 60 ) {
		return false;
	}
	if( millisecond < 0 || millisecond >= 1000 ) {
		return false;
	}

	t_ += wxTimeSpan(hour, minute, second, millisecond);
	return true;
}

void CDateTime::clear()
{
	a_ = days;
	t_ = wxDateTime();
}

#ifdef __VISUALC__

namespace {
extern "C" void NullInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
}

struct CrtAssertSuppressor
{
	CrtAssertSuppressor()
	{
		oldError = _CrtSetReportMode(_CRT_ERROR, 0);
		oldAssert = _CrtSetReportMode(_CRT_ASSERT, 0);
		oldHandler = _set_invalid_parameter_handler(NullInvalidParameterHandler);
	}

	~CrtAssertSuppressor()
	{
		_set_invalid_parameter_handler(oldHandler);
		_CrtSetReportMode(_CRT_ASSERT, oldAssert);
		_CrtSetReportMode(_CRT_ERROR, oldError);
	}

	int oldError;
	int oldAssert;
	_invalid_parameter_handler oldHandler;
};
}
#endif

bool CDateTime::VerifyFormat(wxString const& fmt)
{
	wxChar buf[4096];
	tm *t = wxDateTime::GetTmNow();

#ifdef __VISUALC__
	CrtAssertSuppressor suppressor;
#endif

	return wxStrftime(buf, sizeof(buf)/sizeof(wxChar), fmt, t) != 0;
}

CDateTime CMonotonicTime::m_lastTime = CDateTime::Now();
int CMonotonicTime::m_lastOffset = 0;

CMonotonicTime::CMonotonicTime(const CDateTime& time)
	: m_time(time)
{
}

CMonotonicTime CMonotonicTime::Now()
{
	CMonotonicTime time;
	time.m_time = CDateTime::Now();
	if (time.m_time == m_lastTime)
		time.m_offset = ++m_lastOffset;
	else
	{
		m_lastTime = time.m_time;
		time.m_offset = m_lastOffset = 0;
	}
	return time;
}

bool CMonotonicTime::operator < (const CMonotonicTime& op) const
{
	if (m_time < op.m_time)
		return true;
	if (m_time > op.m_time)
		return false;

	return m_offset < op.m_offset;
}

bool CMonotonicTime::operator <= (const CMonotonicTime& op) const
{
	if (m_time < op.m_time)
		return true;
	if (m_time > op.m_time)
		return false;

	return m_offset <= op.m_offset;
}

bool CMonotonicTime::operator > (const CMonotonicTime& op) const
{
	if (m_time > op.m_time)
		return true;
	if (m_time < op.m_time)
		return false;

	return m_offset > op.m_offset;
}

bool CMonotonicTime::operator >= (const CMonotonicTime& op) const
{
	if (m_time > op.m_time)
		return true;
	if (m_time < op.m_time)
		return false;

	return m_offset >= op.m_offset;
}

bool CMonotonicTime::operator == (const CMonotonicTime& op) const
{
	if (m_time != op.m_time)
		return false;

	return m_offset == op.m_offset;
}

#if defined(_MSC_VER) && _MSC_VER < 1900
namespace {
int64_t GetFreq() {
	LARGE_INTEGER f;
	(void)QueryPerformanceFrequency(&f); // Cannot fail on XP or later according to MSDN
	return f.QuadPart;
}
}

int64_t const CMonotonicClock::freq_ = GetFreq();
#endif