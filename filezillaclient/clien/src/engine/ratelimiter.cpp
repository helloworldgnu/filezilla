#include <filezilla.h>
#include "ratelimiter.h"

#include "event_loop.h"

static int const tickDelay = 250;

CRateLimiter::CRateLimiter(CEventLoop& loop, COptionsBase& options)
	: CEventHandler(loop)
	, options_(options)
{
	RegisterOption(OPTION_SPEEDLIMIT_ENABLE);
	RegisterOption(OPTION_SPEEDLIMIT_INBOUND);
	RegisterOption(OPTION_SPEEDLIMIT_OUTBOUND);

	m_tokenDebt[0] = 0;
	m_tokenDebt[1] = 0;
}

CRateLimiter::~CRateLimiter()
{
	RemoveHandler();
}

int64_t CRateLimiter::GetLimit(rate_direction direction) const
{
	int64_t ret{};
	if (options_.GetOptionVal(OPTION_SPEEDLIMIT_ENABLE) != 0) {
		ret = static_cast<int64_t>(options_.GetOptionVal(OPTION_SPEEDLIMIT_INBOUND + direction)) * 1024;
	}

	return ret;
}

void CRateLimiter::AddObject(CRateLimiterObject* pObject)
{
	scoped_lock lock(sync_);

	m_objectList.push_back(pObject);

	for (int i = 0; i < 2; ++i) {
		int64_t limit = GetLimit(static_cast<rate_direction>(i));
		if (limit > 0) {
			int64_t tokens = limit / (1000 / tickDelay);

			tokens /= m_objectList.size();
			if (m_tokenDebt[i] > 0) {
				if (tokens >= m_tokenDebt[i]) {
					tokens -= m_tokenDebt[i];
					m_tokenDebt[i] = 0;
				}
				else {
					tokens = 0;
					m_tokenDebt[i] -= tokens;
				}
			}

			pObject->m_bytesAvailable[i] = tokens;

			if (!m_timer)
				m_timer = AddTimer(tickDelay, false);
		}
		else {
			pObject->m_bytesAvailable[i] = -1;
		}
	}
}

void CRateLimiter::RemoveObject(CRateLimiterObject* pObject)
{
	scoped_lock lock(sync_);

	for (auto iter = m_objectList.begin(); iter != m_objectList.end(); ++iter) {
		if (*iter == pObject) {
			for (int i = 0; i < 2; ++i) {
				// If an object already used up some of its assigned tokens, add them to m_tokenDebt,
				// so that newly created objects get less initial tokens.
				// That ensures that rapidly adding and removing objects does not exceed the rate
				int64_t limit = GetLimit(static_cast<rate_direction>(i));
				int64_t tokens = limit / (1000 / tickDelay);
				tokens /= m_objectList.size();
				if ((*iter)->m_bytesAvailable[i] < tokens)
					m_tokenDebt[i] += tokens - (*iter)->m_bytesAvailable[i];
			}
			m_objectList.erase(iter);
			break;
		}
	}

	for (int i = 0; i < 2; ++i) {
		for (auto iter = m_wakeupList[i].begin(); iter != m_wakeupList[i].end(); ++iter) {
			if (*iter == pObject) {
				m_wakeupList[i].erase(iter);
				break;
			}
		}
	}
}

void CRateLimiter::OnTimer(timer_id)
{
	scoped_lock lock(sync_);

	int64_t const limits[2] = { GetLimit(inbound), GetLimit(outbound) };

	for (int i = 0; i < 2; ++i) {
		m_tokenDebt[i] = 0;

		if (m_objectList.empty())
			continue;

		if (limits[i] == 0) {
			for (auto iter = m_objectList.begin(); iter != m_objectList.end(); ++iter) {
				(*iter)->m_bytesAvailable[i] = -1;
				if ((*iter)->m_waiting[i])
					m_wakeupList[i].push_back(*iter);
			}
			continue;
		}

		int64_t tokens = (limits[i] * tickDelay) / 1000;
		int64_t maxTokens = tokens * GetBucketSize();

		// Get amount of tokens for each object
		int64_t tokensPerObject = tokens / m_objectList.size();

		if (tokensPerObject == 0)
			tokensPerObject = 1;
		tokens = 0;

		// This list will hold all objects which didn't reach maxTokens
		std::list<CRateLimiterObject*> unsaturatedObjects;

		for (auto iter = m_objectList.begin(); iter != m_objectList.end(); ++iter) {
			if ((*iter)->m_bytesAvailable[i] == -1) {
				wxASSERT(!(*iter)->m_waiting[i]);
				(*iter)->m_bytesAvailable[i] = tokensPerObject;
				unsaturatedObjects.push_back(*iter);
			}
			else {
				(*iter)->m_bytesAvailable[i] += tokensPerObject;
				if ((*iter)->m_bytesAvailable[i] > maxTokens)
				{
					tokens += (*iter)->m_bytesAvailable[i] - maxTokens;
					(*iter)->m_bytesAvailable[i] = maxTokens;
				}
				else
					unsaturatedObjects.push_back(*iter);

				if ((*iter)->m_waiting[i])
					m_wakeupList[i].push_back(*iter);
			}
		}

		// If there are any left-over tokens (in case of objects with a rate below the limit)
		// assign to the unsaturated sources
		while (tokens != 0 && !unsaturatedObjects.empty()) {
			tokensPerObject = tokens / unsaturatedObjects.size();
			if (tokensPerObject == 0)
				break;
			tokens = 0;

			std::list<CRateLimiterObject*> objects;
			objects.swap(unsaturatedObjects);

			for (auto iter = objects.begin(); iter != objects.end(); ++iter) {
				(*iter)->m_bytesAvailable[i] += tokensPerObject;
				if ((*iter)->m_bytesAvailable[i] > maxTokens) {
					tokens += (*iter)->m_bytesAvailable[i] - maxTokens;
					(*iter)->m_bytesAvailable[i] = maxTokens;
				}
				else
					unsaturatedObjects.push_back(*iter);
			}
		}
	}

	WakeupWaitingObjects(lock);

	if (m_objectList.empty() || (limits[inbound] == 0 && limits[outbound] == 0)) {
		if (m_timer) {
			StopTimer(m_timer);
			m_timer = 0;
		}
	}
}

void CRateLimiter::WakeupWaitingObjects(scoped_lock & l)
{
	for (int i = 0; i < 2; ++i) {
		while (!m_wakeupList[i].empty()) {
			CRateLimiterObject* pObject = m_wakeupList[i].front();
			m_wakeupList[i].pop_front();
			if (!pObject->m_waiting[i])
				continue;

			wxASSERT(pObject->m_bytesAvailable != 0);
			pObject->m_waiting[i] = false;

			l.unlock(); // Do not hold while executing callback
			pObject->OnRateAvailable((rate_direction)i);
			l.lock();
		}
	}
}

int CRateLimiter::GetBucketSize() const
{
	const int burst_tolerance = options_.GetOptionVal(OPTION_SPEEDLIMIT_BURSTTOLERANCE);

	int bucket_size = 1000 / tickDelay;
	switch (burst_tolerance)
	{
	case 1:
		bucket_size *= 2;
		break;
	case 2:
		bucket_size *= 5;
		break;
	default:
		break;
	}

	return bucket_size;
}

void CRateLimiter::operator()(CEventBase const& ev)
{
	if (Dispatch<CTimerEvent>(ev, this, &CRateLimiter::OnTimer)) {
		return;
	}
	Dispatch<CRateLimitChangedEvent>(ev, this, &CRateLimiter::OnRateChanged);
}

void CRateLimiter::OnRateChanged()
{
	scoped_lock lock(sync_);
	if (GetLimit(inbound) > 0 || GetLimit(outbound) > 0) {
		if (!m_timer)
			m_timer = AddTimer(tickDelay, false);
	}
}

void CRateLimiter::OnOptionsChanged(changed_options_t const&)
{
	SendEvent<CRateLimitChangedEvent>();
}

CRateLimiterObject::CRateLimiterObject()
{
	for (int i = 0; i < 2; ++i) {
		m_waiting[i] = false;
		m_bytesAvailable[i] = -1;
	}
}

void CRateLimiterObject::UpdateUsage(CRateLimiter::rate_direction direction, int usedBytes)
{
	wxASSERT(usedBytes <= m_bytesAvailable[direction]);
	if (usedBytes > m_bytesAvailable[direction])
		m_bytesAvailable[direction] = 0;
	else
		m_bytesAvailable[direction] -= usedBytes;
}

void CRateLimiterObject::Wait(CRateLimiter::rate_direction direction)
{
	wxASSERT(m_bytesAvailable[direction] == 0);
	m_waiting[direction] = true;
}

bool CRateLimiterObject::IsWaiting(CRateLimiter::rate_direction direction) const
{
	return m_waiting[direction];
}
