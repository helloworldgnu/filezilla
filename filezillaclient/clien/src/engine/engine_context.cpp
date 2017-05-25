#include <filezilla.h>
#include "engine_context.h"

#include "directorycache.h"
#include "event_loop.h"
#include "logging_private.h"
#include "pathcache.h"
#include "ratelimiter.h"
#include "socket.h"

namespace {
struct logging_options_changed_event_type;
typedef CEvent<logging_options_changed_event_type> CLoggingOptionsChangedEvent;

class CLoggingOptionsChanged final : public CEventHandler, COptionChangeEventHandler
{
public:
	CLoggingOptionsChanged(COptionsBase& options, CEventLoop & loop)
		: CEventHandler(loop)
		, options_(options)
	{
		RegisterOption(OPTION_LOGGING_DEBUGLEVEL);
		RegisterOption(OPTION_LOGGING_RAWLISTING);
		SendEvent<CLoggingOptionsChangedEvent>();
	}

	virtual void OnOptionsChanged(changed_options_t const& options)
	{
		if (options.test(OPTION_LOGGING_DEBUGLEVEL) || options.test(OPTION_LOGGING_RAWLISTING)) {
			CLogging::UpdateLogLevel(options_); // In main thread
			SendEvent<CLoggingOptionsChangedEvent>();
		}
	}

	virtual void operator()(const CEventBase&)
	{
		CLogging::UpdateLogLevel(options_); // In worker thread
	}

	COptionsBase& options_;
};
}

class CFileZillaEngineContext::Impl final
{
public:
	Impl(COptionsBase& options)
		: limiter_(loop_, options)
		, optionChangeHandler_(options, loop_)
	{
		CLogging::UpdateLogLevel(options);
	}

	~Impl()
	{
		loop_.RemoveHandler(&optionChangeHandler_);
	}

	CEventLoop loop_;
	CRateLimiter limiter_;
	CDirectoryCache directory_cache_;
	CPathCache path_cache_;
	CLoggingOptionsChanged optionChangeHandler_;
};

CFileZillaEngineContext::CFileZillaEngineContext(COptionsBase & options)
: options_(options)
, impl_(new Impl(options))
{
}

CFileZillaEngineContext::~CFileZillaEngineContext()
{
}

COptionsBase& CFileZillaEngineContext::GetOptions()
{
	return options_;
}

CEventLoop& CFileZillaEngineContext::GetEventLoop()
{
	return impl_->loop_;
}

CRateLimiter& CFileZillaEngineContext::GetRateLimiter()
{
	return impl_->limiter_;
}

CDirectoryCache& CFileZillaEngineContext::GetDirectoryCache()
{
	return impl_->directory_cache_;
}

CPathCache& CFileZillaEngineContext::GetPathCache()
{
	return impl_->path_cache_;
}
