#ifndef __RATELIMITER_H__
#define __RATELIMITER_H__

#include <option_change_event_handler.h>

class COptionsBase;

class CRateLimiterObject;

// This class implements a simple rate limiter based on the Token Bucket algorithm.
class CRateLimiter final : protected CEventHandler, COptionChangeEventHandler
{
public:
	CRateLimiter(CEventLoop& loop, COptionsBase& options);
	~CRateLimiter();

	enum rate_direction
	{
		inbound,
		outbound
	};

	void AddObject(CRateLimiterObject* pObject);
	void RemoveObject(CRateLimiterObject* pObject);

protected:
	int64_t GetLimit(rate_direction direction) const;

	int GetBucketSize() const;

	std::list<CRateLimiterObject*> m_objectList;
	std::list<CRateLimiterObject*> m_wakeupList[2];

	timer_id m_timer{};

	int64_t m_tokenDebt[2];

	COptionsBase& options_;

	void WakeupWaitingObjects(scoped_lock & l);

	void OnOptionsChanged(changed_options_t const& options);

	void operator()(CEventBase const& ev);
	void OnTimer(timer_id id);
	void OnRateChanged();

	mutex sync_;
};

struct ratelimit_changed_event_type{};
typedef CEvent<ratelimit_changed_event_type> CRateLimitChangedEvent;

class CRateLimiterObject
{
	friend class CRateLimiter;

public:
	CRateLimiterObject();
	virtual ~CRateLimiterObject() {}
	int64_t GetAvailableBytes(CRateLimiter::rate_direction direction) const { return m_bytesAvailable[direction]; }

	bool IsWaiting(CRateLimiter::rate_direction direction) const;

protected:
	void UpdateUsage(CRateLimiter::rate_direction direction, int usedBytes);
	void Wait(CRateLimiter::rate_direction direction);

	virtual void OnRateAvailable(CRateLimiter::rate_direction) {}

private:
	bool m_waiting[2];
	int64_t m_bytesAvailable[2];
};

#endif //__RATELIMITER_H__
