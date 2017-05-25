#ifndef __TIMEEX_H__
#define __TIMEEX_H__

#include <wx/time.h>

#include <chrono>

#if HAVE_UNSTEADY_STEADY_CLOCK
#include <sys/time.h>
#endif

class CDateTime final
{
public:
	enum Accuracy : char {
		days,
		hours,
		minutes,
		seconds,
		milliseconds
	};

	CDateTime();
	CDateTime( int year, int month, int day, int hour = -1, int minute = -1, int second = -1, int millisecond = -1 );
	CDateTime( wxDateTime const& t, Accuracy a );

	CDateTime( CDateTime const& op );
	CDateTime& operator=( CDateTime const& op );

	wxDateTime Degenerate() const { return t_; }

	bool IsValid() const { return t_.IsValid(); }
	void clear();

	Accuracy GetAccuracy() const { return a_; }

	static CDateTime Now();

	bool operator==( CDateTime const& op ) const;
	bool operator!=( CDateTime const& op ) const { return !(*this == op); }
	bool operator<( CDateTime const& op ) const;
	bool operator>( CDateTime const& op ) const { return op < *this; }

	wxTimeSpan operator-( CDateTime const& op ) const;

	int Compare( CDateTime const& op ) const;
	bool IsEarlierThan( CDateTime const& op ) const { return Compare(op) < 0; };
	bool IsLaterThan( CDateTime const& op ) const { return Compare(op) > 0; };

	CDateTime& operator+=( wxTimeSpan const& op );
	CDateTime operator+( wxTimeSpan const& op ) const { CDateTime t(*this); t += op; return t; }

	// Beware: month and day are 1-indexed!
	bool Set( int year, int month, int day, int hour = -1, int minute = -1, int second = -1, int millisecond = -1 );
	bool ImbueTime( int hour, int minute, int second = -1, int millisecond = -1 );

	static bool VerifyFormat(wxString const& fmt);

private:
	int CompareSlow( CDateTime const& op ) const;

	bool IsClamped();

	wxDateTime t_;
	Accuracy a_;
};

/* If called multiple times in a row, wxDateTime::Now may return the same
 * time. This causes problems with the cache logic. This class implements
 * an extended time class in wich Now() never returns the same value.
 */

class CMonotonicTime final
{
public:
	CMonotonicTime(const CDateTime& time);
	CMonotonicTime() = default;

	static CMonotonicTime Now();

	CDateTime GetTime() const { return m_time; }

	bool IsValid() const { return m_time.IsValid(); }

	bool operator < (const CMonotonicTime& op) const;
	bool operator <= (const CMonotonicTime& op) const;
	bool operator > (const CMonotonicTime& op) const;
	bool operator >= (const CMonotonicTime& op) const;
	bool operator == (const CMonotonicTime& op) const;

protected:
	static CDateTime m_lastTime;
	static int m_lastOffset;

	CDateTime m_time;
	int m_offset{};
};


class CMonotonicClock final
{
public:
	CMonotonicClock() = default;

#if defined(_MSC_VER) && _MSC_VER < 1900
	// Most unfortunate: steady_clock is implemented in terms
	// of system_clock which is not monotonic prior to Visual Studio
	// 2015 which is unreleased as of writing this.
	// FIXME: Remove once Visual Studio 2015 is released
	static CMonotonicClock CMonotonicClock::now() {
		LARGE_INTEGER i;
		(void)QueryPerformanceCounter(&i); // Cannot fail on XP or later according to MSDN
		return CMonotonicClock(i.QuadPart);
	}

private:
	CMonotonicClock(int64_t t)
		: t_(t)
	{}

	int64_t t_;

	static int64_t const freq_;

#elif HAVE_UNSTEADY_STEADY_CLOCK
	// FIXME: Remove once Debian Jessie is stable
	static CMonotonicClock now() {
		timespec t;
		if (clock_gettime(CLOCK_MONOTONIC, &t) != -1) {
			return CMonotonicClock(t.tv_sec * 1000 + t.tv_nsec / 1000000);
		}

		timeval tv;
		(void)gettimeofday(&tv, 0);
		return CMonotonicClock(tv.tv_sec * 1000 + tv.tv_usec / 1000);
	}

private:
	CMonotonicClock(int64_t  t)
		: t_(t)
	{}

	int64_t t_;
#else
private:
	typedef std::chrono::steady_clock clock_type;
	static_assert(std::chrono::steady_clock::is_steady, "Nonconforming stdlib, your steady_clock isn't steady");

public:
	static CMonotonicClock now() {
		return CMonotonicClock(clock_type::now());
	}

private:
	CMonotonicClock(clock_type::time_point const& t)
		: t_(t)
	{}

	clock_type::time_point t_;
#endif

	friend int64_t operator-(CMonotonicClock const& a, CMonotonicClock const& b);
};

inline int64_t operator-(CMonotonicClock const& a, CMonotonicClock const& b)
{
#if defined(_MSC_VER) && _MSC_VER < 1900
	return (a.t_ - b.t_) * 1000 / CMonotonicClock::freq_;
#elif HAVE_UNSTEADY_STEADY_CLOCK
	return a.t_ - b.t_;
#else
	return std::chrono::duration_cast<std::chrono::milliseconds>(a.t_ - b.t_).count();
#endif
}

#endif //__TIMEEX_H__
