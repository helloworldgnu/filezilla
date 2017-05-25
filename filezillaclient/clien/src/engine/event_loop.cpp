#include <filezilla.h>

#include "event_loop.h"

#include <algorithm>

CEventLoop::CEventLoop()
	: wxThread(wxTHREAD_JOINABLE)
	, sync_(false)
{
	Create();
	Run();
}

CEventLoop::~CEventLoop()
{
	{
		scoped_lock lock(sync_);
		quit_ = true;
		signalled_ = true;
		cond_.signal(lock);
	}

	Wait(wxTHREAD_WAIT_BLOCK);

	scoped_lock lock(sync_);
	for (auto & v : pending_events_) {
		delete v.second;
	}
}

void CEventLoop::SendEvent(CEventHandler* handler, CEventBase* evt)
{
	{
		scoped_lock lock(sync_);
		if (!handler->removing_) {
			pending_events_.emplace_back(handler, evt);
			signalled_ = true;
			cond_.signal(lock);
		}
		else {
			delete evt;
		}
	}
}

void CEventLoop::RemoveHandler(CEventHandler* handler)
{
	scoped_lock l(sync_);

	handler->removing_ = true;

	pending_events_.erase(
		std::remove_if(pending_events_.begin(), pending_events_.end(),
			[&](Events::value_type const& v) {
				if (v.first == handler) {
					delete v.second;
				}
				return v.first == handler;
			}
		),
		pending_events_.end()
	);

	timers_.erase(
		std::remove_if(timers_.begin(), timers_.end(),
			[&](timer_data const& v) {
				return v.handler_ == handler;
			}
		),
		timers_.end()
	);

	while (active_handler_ == handler) {
		l.unlock();
		wxMilliSleep(1);
		l.lock();
	}
}

void CEventLoop::FilterEvents(std::function<bool(Events::value_type &)> filter)
{
	scoped_lock l(sync_);

	pending_events_.erase(
		std::remove_if(pending_events_.begin(), pending_events_.end(),
			[&](Events::value_type & v) {
				bool const remove = filter(v);
				if (remove) {
					delete v.second;
				}
				return remove;
			}
		),
		pending_events_.end()
	);
}

timer_id CEventLoop::AddTimer(CEventHandler* handler, int ms_interval, bool one_shot)
{
	timer_data d;
	d.handler_ = handler;
	d.ms_interval_ = ms_interval;
	d.one_shot_ = one_shot;
	d.deadline_ = CDateTime::Now() + wxTimeSpan::Milliseconds(ms_interval);


	scoped_lock lock(sync_);
	static timer_id id{};
	if (!handler->removing_) {
		d.id_ = ++id; // 64bit, can this really ever overflow?

		timers_.emplace_back(d);
		signalled_ = true;
		cond_.signal(lock);
	}
	return d.id_;
}

void CEventLoop::StopTimer(timer_id id)
{
	if (id) {
		scoped_lock lock(sync_);
		for (auto it = timers_.begin(); it != timers_.end(); ++it) {
			if (it->id_ == id) {
				timers_.erase(it);
				break;
			}
		}
	}
}

bool CEventLoop::ProcessEvent(scoped_lock & l)
{
	Events::value_type ev{};
	bool requestMore = false;

	if (pending_events_.empty()) {
		return false;
	}
	ev = pending_events_.front();
	pending_events_.pop_front();
	requestMore = !pending_events_.empty() || quit_;

	if (ev.first && !ev.first->removing_) {
		active_handler_ = ev.first;
		l.unlock();
		if (ev.second) {
			(*ev.first)(*ev.second);
		}
		delete ev.second;
		l.lock();
		active_handler_ = 0;
	}
	else {
		delete ev.second;
	}

	return requestMore;
}

wxThread::ExitCode CEventLoop::Entry()
{
	scoped_lock l(sync_);
	while (!quit_) {
		if (!signalled_) {
			int wait = GetNextWaitInterval();
			if (wait == std::numeric_limits<int>::max()) {
				cond_.wait(l);
			}
			else if (wait) {
				cond_.wait(l, wait);
			}
		}

		signalled_ = false;

		if (!ProcessTimers(l)) {
			if (ProcessEvent(l)) {
				signalled_ = true;
			}
		}
	}

	return 0;
}

bool CEventLoop::ProcessTimers(scoped_lock & l)
{
	CDateTime const now(CDateTime::Now());
	for (auto it = timers_.begin(); it != timers_.end(); ++it) {
		int diff = static_cast<int>((it->deadline_ - now).GetMilliseconds().GetValue());
		if (diff <= -60000) {
			// Did the system time change? Update deadline
			it->deadline_ = now + it->ms_interval_;
			continue;
		}
		else if (diff <= 0) {
			CEventHandler *const handler = it->handler_;
			auto const id = it->id_;
			if (it->one_shot_) {
				timers_.erase(it);
			}
			else {
				it->deadline_ = now + wxTimeSpan::Milliseconds(it->ms_interval_);
			}

			bool const requestMore = !pending_events_.empty() || quit_;

			if (!handler->removing_) {
				active_handler_ = handler;
				l.unlock();
				(*handler)(CTimerEvent(id));
				l.lock();
				active_handler_ = 0;
			}

			signalled_ |= requestMore;

			return true;
		}
	}

	return false;
}

int CEventLoop::GetNextWaitInterval()
{
	int wait = std::numeric_limits<int>::max();

	CDateTime const now(CDateTime::Now());
	for (auto & timer : timers_) {
		int diff = static_cast<int>((timer.deadline_ - now).GetMilliseconds().GetValue());
		if (diff <= -60000) {
			// Did the system time change? Update deadline
			timer.deadline_ = now + timer.ms_interval_;
			diff = timer.ms_interval_;
		}

		if (diff < wait) {
			wait = diff;
		}
	}

	if (wait < 0) {
		wait = 0;
	}

	return wait;
}
