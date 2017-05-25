#ifndef FILEZILLA_ENGINE_EVENT_HANDLER
#define FILEZILLA_ENGINE_EVENT_HANDLER

class CEventBase;
class CEventLoop;

#include "event_loop.h"

class CEventHandler
{
public:
	CEventHandler(CEventLoop& loop);
	virtual ~CEventHandler();

	void RemoveHandler();

	virtual void operator()(CEventBase const&) = 0;

	template<typename T, typename... Args>
	void SendEvent(Args&&... args) {
		event_loop_.SendEvent(this, new T(std::forward<Args>(args)...));
	};

	timer_id AddTimer(int ms_interval, bool one_shot);
	void StopTimer(timer_id id);

	CEventLoop & event_loop_;

private:
	friend class CEventLoop;
	bool removing_{};
};

#endif