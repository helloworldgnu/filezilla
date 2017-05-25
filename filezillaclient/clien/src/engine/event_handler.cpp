#include <filezilla.h>

#include "event_handler.h"
#include "event_loop.h"

CEventHandler::CEventHandler(CEventLoop& loop)
	: event_loop_(loop)
{
}

CEventHandler::~CEventHandler()
{
	wxASSERT(removing_); // To avoid races, the base class must have removed us already
}

void CEventHandler::RemoveHandler()
{
	event_loop_.RemoveHandler(this);
}

timer_id CEventHandler::AddTimer(int ms_interval, bool one_shot)
{
	return event_loop_.AddTimer(this, ms_interval, one_shot);
}

void CEventHandler::StopTimer(timer_id id)
{
	event_loop_.StopTimer(id);
}
