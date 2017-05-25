#ifndef FILEZILLA_ENGINE_CONTEXT
#define FILEZILLA_ENGINE_CONTEXT

#include <memory>

class CDirectoryCache;
class CEventLoop;
class COptionsBase;
class CPathCache;
class CRateLimiter;

// There can be multiple engines, but there can be at most one context
class CFileZillaEngineContext final
{
public:
	CFileZillaEngineContext(COptionsBase & options);
	~CFileZillaEngineContext();

	COptionsBase& GetOptions();
	CEventLoop& GetEventLoop();
	CRateLimiter& GetRateLimiter();
	CDirectoryCache& GetDirectoryCache();
	CPathCache& GetPathCache();

protected:
	COptionsBase& options_;

	class Impl;
	std::unique_ptr<Impl> impl_;
};

#endif