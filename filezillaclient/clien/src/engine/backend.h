#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "ratelimiter.h"
#include "socket.h"

class CBackend : public CRateLimiterObject, public CSocketEventSource
{
public:
	CBackend(CEventHandler* pEvtHandler);
	virtual ~CBackend();

	CBackend(CBackend const&) = delete;
	CBackend& operator=(CBackend const&) = delete;

	virtual int Read(void *buffer, unsigned int size, int& error) = 0;
	virtual int Peek(void *buffer, unsigned int size, int& error) = 0;
	virtual int Write(const void *buffer, unsigned int size, int& error) = 0;

	virtual void OnRateAvailable(enum CRateLimiter::rate_direction direction) = 0;

protected:
	CEventHandler* const m_pEvtHandler;
};

class CSocket;
class CSocketBackend final : public CBackend
{
public:
	CSocketBackend(CEventHandler* pEvtHandler, CSocket & socket, CRateLimiter& rateLimiter);
	virtual ~CSocketBackend();
	// Backend definitions
	virtual int Read(void *buffer, unsigned int size, int& error);
	virtual int Peek(void *buffer, unsigned int size, int& error);
	virtual int Write(const void *buffer, unsigned int size, int& error);

protected:
	virtual void OnRateAvailable(enum CRateLimiter::rate_direction direction);

	CSocket &socket_;
	CRateLimiter& m_rateLimiter;
};

#endif //__BACKEND_H__
