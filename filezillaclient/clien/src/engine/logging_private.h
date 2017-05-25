#ifndef __LOGGING_PRIVATE_H__
#define __LOGGING_PRIVATE_H__

#include "engineprivate.h"
#include <mutex.h>
#include <utility>

class CLogging
{
public:
	explicit CLogging(CFileZillaEnginePrivate & engine);
	virtual ~CLogging();

	CLogging(CLogging const&) = delete;
	CLogging& operator=(CLogging const&) = delete;

	template<typename String, typename...Args>
	void LogMessage(MessageType nMessageType, String&& msgFormat, Args&& ...args) const
	{
		if( !ShouldLog(nMessageType) ) {
			return;
		}

		CLogmsgNotification *notification = new CLogmsgNotification(nMessageType);
		notification->msg.Printf(std::forward<String>(msgFormat), std::forward<Args>(args)...);

		LogToFile(nMessageType, notification->msg);
		engine_.AddLogNotification(notification);
	}

	template<typename String>
	void LogMessageRaw(MessageType nMessageType, String&& msg) const
	{
		if( !ShouldLog(nMessageType) ) {
			return;
		}

		CLogmsgNotification *notification = new CLogmsgNotification(nMessageType, std::forward<String>(msg));

		LogToFile(nMessageType, notification->msg);
		engine_.AddLogNotification(notification);
	}

	template<typename String, typename String2, typename...Args>
	void LogMessage(String&& sourceFile, int nSourceLine, void *pInstance, MessageType nMessageType
					, String2&& msgFormat, Args&& ...args) const
	{
		if( !ShouldLog(nMessageType) ) {
			return;
		}

		wxString source(sourceFile);
		int pos = source.Find('\\', true);
		if (pos != -1)
			source = source.Mid(pos+1);

		pos = source.Find('/', true);
		if (pos != -1)
			source = source.Mid(pos+1);

		wxString text = wxString::Format(std::forward<String2>(msgFormat), std::forward<Args>(args)...);

		CLogmsgNotification *notification = new CLogmsgNotification(nMessageType);
		notification->msg.Printf(_T("%s(%d): %s   caller=%p"), source, nSourceLine, text, pInstance);

		LogToFile(nMessageType, notification->msg);
		engine_.AddLogNotification(notification);
	}

	bool ShouldLog(MessageType nMessageType) const;

	// Only affects calling thread
	static void UpdateLogLevel(COptionsBase & options);

private:
	CFileZillaEnginePrivate & engine_;

	void InitLogFile() const;
	void LogToFile(MessageType nMessageType, const wxString& msg) const;

	static bool m_logfile_initialized;
#ifdef __WXMSW__
	static HANDLE m_log_fd;
#else
	static int m_log_fd;
#endif
	static wxString m_prefixes[static_cast<int>(MessageType::count)];
	static unsigned int m_pid;
	static int m_max_size;
	static wxString m_file;

	static int m_refcount;

	static mutex mutex_;

#if defined(_MSC_VER) && _MSC_VER < 1900
	// Fixme: Get rid of this once VS2015 comes out
	#define thread_local __declspec(thread)
#elif HAVE_NO_THREAD_LOCAL
	// Fixme: Get rid of this once a) Debian Jessie is stable and b) OS X' clang supports it.
	#define thread_local __thread
#endif
	static thread_local int debug_level_;
	static thread_local int raw_listing_;
};

#endif
