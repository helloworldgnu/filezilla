#ifndef FZ_RTT_HEADER
#define FZ_RTT_HEADER

#include "socket.h"

class CLatencyMeasurement final : public CCallback
{
public:
	CLatencyMeasurement();

	// Returns false if measurement cannot be started due to
	// a measurement already running
	bool Start();

	// Returns fals if there was no measurement running
	bool Stop();

	// In ms, returns -1 if no data is available.
	int GetLatency() const;

	void Reset();

	virtual void cb();

protected:
	wxDateTime m_start;

	wxLongLong m_summed_latency;
	int m_measurements;

	mutable mutex m_sync;
};

#endif
