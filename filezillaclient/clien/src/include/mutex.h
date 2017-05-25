#ifndef FILEZILLA_MUTEX_HEADER
#define FILEZILLA_MUTEX_HEADER

/* A mutex, or critical section, should be as lightweight as possible.
 * Unfortunately, wxWidgets' mutex isn't lightweight at all.
 * Testing has shown that locking or unlocking a wxMutex consists of
 * 60% useless cruft, e.g. deadlock detection (Why try to detect deadlocks?
 * Deadlocks detect itself!)
 *
 * Likewise, wxCondition carries a heavy overhead as well. In particular, under MSW
 * it doesn't and can't (due to XP compatibility) use Vista+'s CONDITION_VARIABLE.
 *
 * Unfortunately we can't use std::mutex for the rescue as MinGW doesn't implement
 * C++11 threading yet in common configurations.
 */

#ifndef __WXMSW__
#include <pthread.h>
#endif

class mutex final
{
public:
	explicit mutex(bool recursive = true);
	~mutex();

	mutex( mutex const& ) = delete;
	mutex& operator=( mutex const& ) = delete;

	// Beware, manual locking isn't exception safe
	void lock();
	void unlock();

private:
	friend class condition;
	friend class scoped_lock;

#ifdef __WXMSW__
	CRITICAL_SECTION m_;
#else
	pthread_mutex_t m_;
#endif
};

class scoped_lock final
{
public:
	explicit scoped_lock(mutex& m)
		: m_(&m.m_)
	{
#ifdef __WXMSW__
		EnterCriticalSection(m_);
#else
		pthread_mutex_lock(m_);
#endif
	}

	~scoped_lock()
	{
		if (locked_) {
	#ifdef __WXMSW__
			LeaveCriticalSection(m_);
	#else
			pthread_mutex_unlock(m_);
	#endif
		}

	}

	scoped_lock( scoped_lock const& ) = delete;
	scoped_lock& operator=( scoped_lock const& ) = delete;

	void lock()
	{
		locked_ = true;
#ifdef __WXMSW__
		EnterCriticalSection(m_);
#else
		pthread_mutex_lock(m_);
#endif
	}

	void unlock()
	{
		locked_ = false;
#ifdef __WXMSW__
		LeaveCriticalSection(m_);
#else
		pthread_mutex_unlock(m_);
#endif
	}

private:
	friend class condition;

#ifdef __WXMSW__
	CRITICAL_SECTION * const m_;
#else
	pthread_mutex_t * const m_;
#endif
	bool locked_{true};
};

class condition final
{
public:
	condition();
	~condition();

	condition(condition const&) = delete;
	condition& operator=(condition const&) = delete;

	void wait(scoped_lock& l);

	// Milliseconds
	bool wait(scoped_lock& l, int timeout_ms);

	void signal(scoped_lock& l);
private:
#ifdef __WXMSW__
	CONDITION_VARIABLE cond_;
#else
	pthread_cond_t cond_;
#endif
	bool signalled_;
};

#endif
