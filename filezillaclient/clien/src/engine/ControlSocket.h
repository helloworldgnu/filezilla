#ifndef __CONTROLSOCKET_H__
#define __CONTROLSOCKET_H__

#include "socket.h"
#include "logging_private.h"
#include "backend.h"

class COpData
{
public:
	COpData(Command op_Id);
	virtual ~COpData();

	COpData(COpData const&) = delete;
	COpData& operator=(COpData const&) = delete;

	int opState;
	Command const opId;

	bool waitForAsyncRequest;
	bool holdsLock;

	COpData *pNextOpData;
};

class CConnectOpData : public COpData
{
public:
	CConnectOpData()
		: COpData(Command::connect)
		, port(0)
	{
	}

	wxString host;
	unsigned int port;
};

class CFileTransferOpData : public COpData
{
public:
	CFileTransferOpData(bool is_download, const wxString& local_file, const wxString& remote_file, const CServerPath& remote_path);
	virtual ~CFileTransferOpData();
	// Transfer data
	wxString localFile, remoteFile;
	CServerPath remotePath;
	const bool download;

	CDateTime fileTime;
	wxFileOffset localFileSize;
	wxFileOffset remoteFileSize;

	bool tryAbsolutePath;
	bool resume;

	CFileTransferCommand::t_transferSettings transferSettings;

	// Set to true when sending the command which
	// starts the actual transfer
	bool transferInitiated;
};

class CMkdirOpData : public COpData
{
public:
	CMkdirOpData()
		: COpData(Command::mkdir)
	{
	}

	virtual ~CMkdirOpData()
	{
	}

	CServerPath path;
	CServerPath currentPath;
	CServerPath commonParent;
	std::list<wxString> segments;
};

class CChangeDirOpData : public COpData
{
public:
	CChangeDirOpData()
		: COpData(Command::cwd)
	{
		tryMkdOnFail = false;
		link_discovery = false;
	}

	virtual ~CChangeDirOpData()
	{
	}

	CServerPath path;
	wxString subDir;
	bool tryMkdOnFail;
	CServerPath target;

	bool link_discovery;
};

enum class TransferEndReason
{
	none,
	successful,
	timeout,
	transfer_failure,					// Error during transfer, like lost connection. Retry automatically
	transfer_failure_critical,			// Error during transfer like lack of diskspace. Needs user interaction
	pre_transfer_command_failure,		// If a command fails prior to sending the transfer command
	transfer_command_failure_immediate,	// Used if server does not send the 150 reply after the transfer command
	transfer_command_failure,			// Used if the transfer command fails, but after receiving a 150 first
	failure,							// Other unspecific failure
	failed_resumetest
};

class CTransferStatus;
class CControlSocket: public CLogging, public CEventHandler
{
public:
	CControlSocket(CFileZillaEnginePrivate & engine);
	virtual ~CControlSocket();

	CControlSocket(CControlSocket const&) = delete;
	CControlSocket& operator=(CControlSocket const&) = delete;

	virtual int Connect(const CServer &server) = 0;
	virtual int Disconnect();
	virtual void Cancel();
	virtual int List(CServerPath path = CServerPath(), wxString subDir = wxString(), int flags = 0);
	virtual int FileTransfer(const wxString localFile, const CServerPath &remotePath,
							 const wxString &remoteFile, bool download,
							 const CFileTransferCommand::t_transferSettings& transferSettings);
	virtual int RawCommand(const wxString& command = wxString());
	virtual int Delete(const CServerPath& path, const std::list<wxString>& files);
	virtual int RemoveDir(const CServerPath& path = CServerPath(), const wxString& subDir = wxString());
	virtual int Mkdir(const CServerPath& path);
	virtual int Rename(const CRenameCommand& command);
	virtual int Chmod(const CChmodCommand& command);
	virtual bool Connected() = 0;

	// If m_pCurrentOpData is zero, this function returns the current command
	// from the engine.
	Command GetCurrentCommandId() const;

	virtual void TransferEnd() {}

	void SendAsyncRequest(CAsyncRequestNotification* pNotification);
	virtual bool SetAsyncRequestReply(CAsyncRequestNotification *pNotification) = 0;
	bool SetFileExistsAction(CFileExistsNotification *pFileExistsNotification);

	const CServer* GetCurrentServer() const;

	// Conversion function which convert between local and server charset.
	wxString ConvToLocal(const char* buffer, size_t len);
	wxChar* ConvToLocalBuffer(const char* buffer, size_t len, size_t& outlen);
	wxChar* ConvToLocalBuffer(const char* buffer, wxMBConv& conv, size_t len, size_t& outlen);
	wxCharBuffer ConvToServer(const wxString& str, bool force_utf8 = false);

	void SetActive(CFileZillaEngine::_direction direction);

	// ---
	// The following two functions control the timeout behaviour:
	// ---

	// Call this if data could be sent or retrieved
	void SetAlive();

	// Set to true if waiting for data
	void SetWait(bool waiting);

	CFileZillaEnginePrivate& GetEngine() { return engine_; }

	// Only called from the engine, see there for description
	void InvalidateCurrentWorkingDir(const CServerPath& path);

protected:
	wxTimeSpan GetTimezoneOffset();

	virtual int DoClose(int nErrorCode = FZ_REPLY_DISCONNECTED);
	bool m_closed;

	virtual int ResetOperation(int nErrorCode);

	virtual int SendNextCommand();

	void LogTransferResultMessage(int nErrorCode, CFileTransferOpData *pData);

	// Called by ResetOperation if there's a queued operation
	virtual int ParseSubcommandResult(int prevResult);

	wxString ConvertDomainName(wxString const& domain);

	int CheckOverwriteFile();

	void CreateLocalDir(const wxString &local_file);

	bool ParsePwdReply(wxString reply, bool unquoted = false, const CServerPath& defaultPath = CServerPath());

	COpData *m_pCurOpData;
	int m_nOpState;
	CFileZillaEnginePrivate & engine_;
	CServer *m_pCurrentServer;

	CServerPath m_CurrentPath;

	wxCSConv *m_pCSConv;
	bool m_useUTF8;

	// Timeout data
	timer_id m_timer{};
	CMonotonicClock m_lastActivity;

	// -------------------------
	// Begin cache locking stuff
	// -------------------------

	enum locking_reason
	{
		lock_unknown = -1,
		lock_list,
		lock_mkdir
	};

	// Tries to obtain lock. Returns true on success.
	// On failure, caller has to pass control.
	// SendNextCommand will be called once the lock gets available
	// and engine could obtain it.
	// Lock is recursive. Lock counter increases on suboperations.
	bool TryLockCache(enum locking_reason reason, const CServerPath& directory);
	bool IsLocked(enum locking_reason reason, const CServerPath& directory);

	// Unlocks the cache. Can be called if not holding the lock
	// Doesn't need reason as one engine can at most hold one lock
	void UnlockCache();

	// Called from the fzOBTAINLOCK event.
	// Returns reason != unknown iff engine is the first waiting engine
	// and obtains the lock.
	// On failure, the engine was not waiting for a lock.
	enum locking_reason ObtainLockFromEvent();

	bool IsWaitingForLock();

	struct t_lockInfo
	{
		CControlSocket* pControlSocket;
		CServerPath directory;
		enum locking_reason reason;
		bool waiting;
		int lockcount;
	};
	static std::list<t_lockInfo> m_lockInfoList;

	const std::list<t_lockInfo>::iterator GetLockStatus();

	// -----------------------
	// End cache locking stuff
	// -----------------------

	bool m_invalidateCurrentPath;

	virtual void operator()(CEventBase const& ev);

	void OnTimer(timer_id id);
	void OnObtainLock();
};

class CProxySocket;
class CRealControlSocket : public CControlSocket
{
public:
	CRealControlSocket(CFileZillaEnginePrivate & engine);
	virtual ~CRealControlSocket();

	virtual int Connect(const CServer &server);
	virtual int ContinueConnect();

	virtual bool Connected() { return m_pSocket->GetState() == CSocket::connected; }

protected:
	virtual int DoClose(int nErrorCode = FZ_REPLY_DISCONNECTED);
	void ResetSocket();

	virtual void operator()(CEventBase const& ev);
	void OnSocketEvent(CSocketEventSource* source, SocketEventType t, int error);
	void OnHostAddress(CSocketEventSource* source, wxString const& address);

	virtual void OnConnect();
	virtual void OnReceive();
	void OnSend();
	virtual void OnClose(int error);

	bool Send(const char *buffer, int len);

	CSocket* m_pSocket;

	CBackend* m_pBackend;
	CProxySocket* m_pProxyBackend;

	char *m_pSendBuffer;
	int m_nSendBufferLen;
};

#endif
