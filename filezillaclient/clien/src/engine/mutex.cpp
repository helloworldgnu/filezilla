#include <filezilla.h>
#include "mutex.h"

#ifndef __WXMSW__
#include <sys/time.h>
#endif

mutex::mutex(bool recursive)
{
#ifdef __WXMSW__
	(void)recursive; // Critical sections are always recursive
	InitializeCriticalSection(&m_);
#else
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
	pthread_mutex_init(&m_, &attr);
#endif
}

mutex::~mutex()
{
#ifdef __WXMSW__
	DeleteCriticalSection(&m_);
#else
	pthread_mutex_destroy(&m_);
#endif
}

void mutex::lock()
{
#ifdef __WXMSW__
	EnterCriticalSection(&m_);
#else
	pthread_mutex_lock(&m_);
#endif
}

void mutex::unlock()
{
#ifdef __WXMSW__
	LeaveCriticalSection(&m_);
#else
	pthread_mutex_unlock(&m_);
#endif
}


condition::condition()
	: signalled_()
{
#ifdef __WXMSW__
	InitializeConditionVariable(&cond_);
#else
	pthread_cond_init(&cond_, 0);
#endif
}


condition::~condition()
{
#ifdef __WXMSW__
#else
	pthread_cond_destroy(&cond_);
#endif
}

void condition::wait(scoped_lock& l)
{
	if (signalled_) {
		signalled_ = false;
		return;
	}
#ifdef __WXMSW__
	SleepConditionVariableCS(&cond_, l.m_, INFINITE);
#else
	int res;
	do {
		res = pthread_cond_wait(&cond_, l.m_);
	}
	while (res == EINTR);
#endif
	signalled_ = false;
}

bool condition::wait(scoped_lock& l, int timeout_ms)
{
	if (signalled_) {
		signalled_ = false;
		return true;
	}
#ifdef __WXMSW__
	bool const success = SleepConditionVariableCS(&cond_, l.m_, timeout_ms) != 0;
#else
	int res;
	do {
		timeval tv = {0, 0};
		gettimeofday(&tv, 0);

		timespec ts;
		ts.tv_sec = tv.tv_sec + timeout_ms / 1000;
		ts.tv_nsec = tv.tv_usec * 1000 + (timeout_ms % 1000) * 1000 * 1000;
		if (ts.tv_nsec > 1000000000ll) {
			++ts.tv_sec;
			ts.tv_nsec -= 1000000000ll;
		}
		res = pthread_cond_timedwait(&cond_, l.m_, &ts);
	}
	while (res == EINTR);
	bool const success = res == 0;
#endif
	if (success) {
		signalled_ = false;
	}

	return success;
}


void condition::signal(scoped_lock &)
{
	signalled_ = true;
#ifdef __WXMSW__
	WakeConditionVariable(&cond_);
#else
	pthread_cond_signal(&cond_);
#endif
}
