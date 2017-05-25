#include <filezilla.h>
#include "ControlSocket.h"
#include "directorycache.h"
#include "engineprivate.h"
#include "event_loop.h"
#include "ftpcontrolsocket.h"
#include "httpcontrolsocket.h"
#include "logging_private.h"
#include "pathcache.h"
#include "ratelimiter.h"
#include "sftpcontrolsocket.h"

mutex CFileZillaEnginePrivate::mutex_;
std::list<CFileZillaEnginePrivate*> CFileZillaEnginePrivate::m_engineList;
std::atomic_int CFileZillaEnginePrivate::m_activeStatus[2] = {{0}, {0}};
std::list<CFileZillaEnginePrivate::t_failedLogins> CFileZillaEnginePrivate::m_failedLogins;

CFileZillaEnginePrivate::CFileZillaEnginePrivate(CFileZillaEngineContext& context, CFileZillaEngine& parent)
	: CEventHandler(context.GetEventLoop())
	, transfer_status_(*this)
	, m_options(context.GetOptions())
	, m_rateLimiter(context.GetRateLimiter())
	, directory_cache_(context.GetDirectoryCache())
	, path_cache_(context.GetPathCache())
	, parent_(parent)
{
	m_engineList.push_back(this);

	{
		scoped_lock lock(mutex_);
		static int id = 0;
		m_engine_id = ++id;
	}

	m_pLogging = new CLogging(*this);

	{
		bool queue_logs = ShouldQueueLogsFromOptions();
		scoped_lock lock(notification_mutex_);
		queue_logs_ = queue_logs;
	}

	RegisterOption(OPTION_LOGGING_SHOW_DETAILED_LOGS);
	RegisterOption(OPTION_LOGGING_DEBUGLEVEL);
	RegisterOption(OPTION_LOGGING_RAWLISTING);
}

bool CFileZillaEnginePrivate::ShouldQueueLogsFromOptions() const
{
	return
		m_options.GetOptionVal(OPTION_LOGGING_RAWLISTING) == 0 &&
		m_options.GetOptionVal(OPTION_LOGGING_DEBUGLEVEL) == 0 &&
		m_options.GetOptionVal(OPTION_LOGGING_SHOW_DETAILED_LOGS) == 0;
}

CFileZillaEnginePrivate::~CFileZillaEnginePrivate()
{
	RemoveHandler();
	m_maySendNotificationEvent = false;

	m_pControlSocket.reset();
	m_pCurrentCommand.reset();

	// Delete notification list
	for (auto & notification : m_NotificationList) {
		delete notification;
	}

	// Remove ourself from the engine list
	for (auto iter = m_engineList.begin(); iter != m_engineList.end(); ++iter) {
		if (*iter == this) {
			m_engineList.erase(iter);
			break;
		}
	}

	delete m_pLogging;

	if (m_engineList.empty())
		CSocket::Cleanup(true);
}

void CFileZillaEnginePrivate::OnEngineEvent(EngineNotificationType type)
{
	switch (type)
	{
	case engineCancel:
		DoCancel();
		break;
	case engineTransferEnd:
		if (m_pControlSocket)
			m_pControlSocket->TransferEnd();
	default:
		break;
	}
}

bool CFileZillaEnginePrivate::IsBusy() const
{
	scoped_lock lock(mutex_);
	return m_pCurrentCommand != 0;
}

bool CFileZillaEnginePrivate::IsConnected() const
{
	scoped_lock lock(mutex_);
	if (!m_pControlSocket)
		return false;

	return m_pControlSocket->Connected();
}

const CCommand *CFileZillaEnginePrivate::GetCurrentCommand() const
{
	scoped_lock lock(mutex_);
	return m_pCurrentCommand.get();
}

Command CFileZillaEnginePrivate::GetCurrentCommandId() const
{
	scoped_lock lock(mutex_);
	if (!m_pCurrentCommand)
		return Command::none;
	else
		return GetCurrentCommand()->GetId();
}

void CFileZillaEnginePrivate::AddNotification(CNotification *pNotification)
{
	{
		scoped_lock lock(notification_mutex_);
		m_NotificationList.push_back(pNotification);

		if (!m_maySendNotificationEvent || !m_pEventHandler) {
			return;
		}
		m_maySendNotificationEvent = false;
	}

	m_pEventHandler->QueueEvent(new wxFzEvent(&parent_));
}

void CFileZillaEnginePrivate::AddLogNotification(CLogmsgNotification *pNotification)
{
	scoped_lock lock(notification_mutex_);

	if (pNotification->msgType == MessageType::Error) {
		queue_logs_ = false;
		SendQueuedLogs();
		AddNotification(pNotification);
	}
	else if (pNotification->msgType == MessageType::Status) {
		ClearQueuedLogs(false);
		AddNotification(pNotification);
	}
	else if (!queue_logs_) {
		AddNotification(pNotification);
	}
	else {
		queued_logs_.push_back(pNotification);
	}
}

void CFileZillaEnginePrivate::SendQueuedLogs(bool reset_flag)
{
	{
		scoped_lock lock(notification_mutex_);
		m_NotificationList.insert(m_NotificationList.end(), queued_logs_.begin(), queued_logs_.end());
		queued_logs_.clear();

		if (reset_flag) {
			queue_logs_ = ShouldQueueLogsFromOptions();
		}

		if (!m_maySendNotificationEvent || !m_pEventHandler || m_NotificationList.empty()) {
			return;
		}
		m_maySendNotificationEvent = false;
	}

	m_pEventHandler->QueueEvent(new wxFzEvent(&parent_));
}

void CFileZillaEnginePrivate::ClearQueuedLogs(bool reset_flag)
{
	scoped_lock lock(notification_mutex_);

	for (auto msg : queued_logs_) {
		delete msg;
	}
	queued_logs_.clear();

	if (reset_flag) {
		queue_logs_ = ShouldQueueLogsFromOptions();
	}
}

int CFileZillaEnginePrivate::ResetOperation(int nErrorCode)
{
	scoped_lock lock(mutex_);
	m_pLogging->LogMessage(MessageType::Debug_Debug, _T("CFileZillaEnginePrivate::ResetOperation(%d)"), nErrorCode);

	if (nErrorCode & FZ_REPLY_DISCONNECTED)
		m_lastListDir.clear();

	if (m_pCurrentCommand) {
		if ((nErrorCode & FZ_REPLY_NOTSUPPORTED) == FZ_REPLY_NOTSUPPORTED) {
			wxASSERT(m_bIsInCommand);
			m_pLogging->LogMessage(MessageType::Error, _("Command not supported by this protocol"));
		}

		if (m_pCurrentCommand->GetId() == Command::connect) {
			if (!(nErrorCode & ~(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED | FZ_REPLY_TIMEOUT | FZ_REPLY_CRITICALERROR | FZ_REPLY_PASSWORDFAILED)) &&
				nErrorCode & (FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED))
			{
				CConnectCommand const& connectCommand = static_cast<CConnectCommand const&>(*m_pCurrentCommand.get());

				RegisterFailedLoginAttempt(connectCommand.GetServer(), (nErrorCode & FZ_REPLY_CRITICALERROR) == FZ_REPLY_CRITICALERROR);

				if ((nErrorCode & FZ_REPLY_CRITICALERROR) != FZ_REPLY_CRITICALERROR) {
					++m_retryCount;
					if (m_retryCount < m_options.GetOptionVal(OPTION_RECONNECTCOUNT) && connectCommand.RetryConnecting()) {
						unsigned int delay = GetRemainingReconnectDelay(connectCommand.GetServer());
						if (!delay)
							delay = 1;
						m_pLogging->LogMessage(MessageType::Status, _("Waiting to retry..."));
						StopTimer(m_retryTimer);
						m_retryTimer = AddTimer(delay, true);
						return FZ_REPLY_WOULDBLOCK;
					}
				}
			}
		}

		if (!m_bIsInCommand) {
			COperationNotification *notification = new COperationNotification();
			notification->nReplyCode = nErrorCode;
			notification->commandId = m_pCurrentCommand->GetId();
			AddNotification(notification);
		}
		else
			m_nControlSocketError |= nErrorCode;

		m_pCurrentCommand.reset();
	}
	else if (nErrorCode & FZ_REPLY_DISCONNECTED) {
		if (!m_bIsInCommand) {
			COperationNotification *notification = new COperationNotification();
			notification->nReplyCode = nErrorCode;
			notification->commandId = Command::none;
			AddNotification(notification);
		}
	}

	if (nErrorCode != FZ_REPLY_OK) {
		SendQueuedLogs(true);
	}
	else {
		ClearQueuedLogs(true);
	}

	return nErrorCode;
}

unsigned int CFileZillaEnginePrivate::GetNextAsyncRequestNumber()
{
	scoped_lock lock(notification_mutex_);
	return ++m_asyncRequestCounter;
}

// Command handlers
int CFileZillaEnginePrivate::Connect(const CConnectCommand &command)
{
	if (IsConnected())
		return FZ_REPLY_ALREADYCONNECTED;

	m_retryCount = 0;

	// Need to delete before setting m_pCurrentCommand.
	// The destructor can call CFileZillaEnginePrivate::ResetOperation
	// which would delete m_pCurrentCommand
	m_pControlSocket.reset();
	m_nControlSocketError = 0;

	if (command.GetServer().GetPort() != CServer::GetDefaultPort(command.GetServer().GetProtocol())) {
		ServerProtocol protocol = CServer::GetProtocolFromPort(command.GetServer().GetPort(), true);
		if (protocol != UNKNOWN && protocol != command.GetServer().GetProtocol())
			m_pLogging->LogMessage(MessageType::Status, _("Selected port usually in use by a different protocol."));
	}

	return ContinueConnect();
}

int CFileZillaEnginePrivate::Disconnect(CDisconnectCommand const&)
{
	if (!IsConnected())
		return FZ_REPLY_OK;

	int res = m_pControlSocket->Disconnect();
	if (res == FZ_REPLY_OK) {
		m_pControlSocket.reset();
	}

	return res;
}

int CFileZillaEnginePrivate::List(const CListCommand &command)
{
	int flags = command.GetFlags();
	bool const refresh = (command.GetFlags() & LIST_FLAG_REFRESH) != 0;
	bool const avoid = (command.GetFlags() & LIST_FLAG_AVOID) != 0;

	if (!refresh && !command.GetPath().empty()) {
		const CServer* pServer = m_pControlSocket->GetCurrentServer();
		if (pServer) {
			CServerPath path(path_cache_.Lookup(*pServer, command.GetPath(), command.GetSubDir()));
			if (path.empty() && command.GetSubDir().empty())
				path = command.GetPath();
			if (!path.empty()) {
				CDirectoryListing *pListing = new CDirectoryListing;
				bool is_outdated = false;
				bool found = directory_cache_.Lookup(*pListing, *pServer, path, true, is_outdated);
				if (found && !is_outdated) {
					if (pListing->get_unsure_flags())
						flags |= LIST_FLAG_REFRESH;
					else {
						if (!avoid) {
							m_lastListDir = pListing->path;
							m_lastListTime = CDateTime::Now();
							CDirectoryListingNotification *pNotification = new CDirectoryListingNotification(pListing->path);
							AddNotification(pNotification);
						}
						delete pListing;
						return FZ_REPLY_OK;
					}
				}
				if (is_outdated)
					flags |= LIST_FLAG_REFRESH;
				delete pListing;
			}
		}
	}

	return m_pControlSocket->List(command.GetPath(), command.GetSubDir(), flags);
}

int CFileZillaEnginePrivate::FileTransfer(const CFileTransferCommand &command)
{
	return m_pControlSocket->FileTransfer(command.GetLocalFile(), command.GetRemotePath(), command.GetRemoteFile(), command.Download(), command.GetTransferSettings());
}

int CFileZillaEnginePrivate::RawCommand(const CRawCommand& command)
{
	{
		scoped_lock lock(notification_mutex_);
		queue_logs_ = false;
	}
	return m_pControlSocket->RawCommand(command.GetCommand());
}

int CFileZillaEnginePrivate::Delete(const CDeleteCommand& command)
{
	if (command.GetFiles().size() == 1) {
		m_pLogging->LogMessage(MessageType::Status, _("Deleting \"%s\""), command.GetPath().FormatFilename(command.GetFiles().front()));
	}
	else {
		m_pLogging->LogMessage(MessageType::Status, _("Deleting %u files from \"%s\""), static_cast<unsigned int>(command.GetFiles().size()), command.GetPath().GetPath());
	}
	return m_pControlSocket->Delete(command.GetPath(), command.GetFiles());
}

int CFileZillaEnginePrivate::RemoveDir(const CRemoveDirCommand& command)
{
	return m_pControlSocket->RemoveDir(command.GetPath(), command.GetSubDir());
}

int CFileZillaEnginePrivate::Mkdir(const CMkdirCommand& command)
{
	return m_pControlSocket->Mkdir(command.GetPath());
}

int CFileZillaEnginePrivate::Rename(const CRenameCommand& command)
{
	return m_pControlSocket->Rename(command);
}

int CFileZillaEnginePrivate::Chmod(const CChmodCommand& command)
{
	return m_pControlSocket->Chmod(command);
}

void CFileZillaEnginePrivate::SendDirectoryListingNotification(const CServerPath& path, bool onList, bool modified, bool failed)
{
	scoped_lock lock(mutex_);

	wxASSERT(m_pControlSocket);

	const CServer* const pOwnServer = m_pControlSocket->GetCurrentServer();
	wxASSERT(pOwnServer);

	m_lastListDir = path;

	if (failed) {
		AddNotification(new CDirectoryListingNotification(path, false, true));
		m_lastListTime = CMonotonicTime::Now();

		// On failed messages, we don't notify other engines
		return;
	}

	CMonotonicTime changeTime;
	if (!directory_cache_.GetChangeTime(changeTime, *pOwnServer, path))
		return;

	AddNotification(new CDirectoryListingNotification(path, !onList));
	m_lastListTime = changeTime;

	if (!modified)
		return;

	// Iterate over the other engine, send notification if last listing
	// directory is the same
	for (std::list<CFileZillaEnginePrivate*>::iterator iter = m_engineList.begin(); iter != m_engineList.end(); ++iter) {
		CFileZillaEnginePrivate* const pEngine = *iter;
		if (!pEngine->m_pControlSocket || pEngine->m_pControlSocket == m_pControlSocket)
			continue;

		const CServer* const pServer = pEngine->m_pControlSocket->GetCurrentServer();
		if (!pServer || *pServer != *pOwnServer)
			continue;

		if (pEngine->m_lastListDir != path)
			continue;

		if (pEngine->m_lastListTime.GetTime().IsValid() && changeTime <= pEngine->m_lastListTime)
			continue;

		pEngine->m_lastListTime = changeTime;
		pEngine->AddNotification(new CDirectoryListingNotification(path, true));
	}
}

void CFileZillaEnginePrivate::RegisterFailedLoginAttempt(const CServer& server, bool critical)
{
	scoped_lock lock(mutex_);
	std::list<t_failedLogins>::iterator iter = m_failedLogins.begin();
	while (iter != m_failedLogins.end())
	{
		const wxTimeSpan span = wxDateTime::UNow() - iter->time;
		if (span.GetSeconds() >= m_options.GetOptionVal(OPTION_RECONNECTDELAY) ||
			iter->server == server || (!critical && (iter->server.GetHost() == server.GetHost() && iter->server.GetPort() == server.GetPort())))
		{
			std::list<t_failedLogins>::iterator prev = iter;
			++iter;
			m_failedLogins.erase(prev);
		}
		else
			++iter;
	}

	t_failedLogins failure;
	failure.server = server;
	failure.time = wxDateTime::UNow();
	failure.critical = critical;
	m_failedLogins.push_back(failure);
}

unsigned int CFileZillaEnginePrivate::GetRemainingReconnectDelay(const CServer& server)
{
	scoped_lock lock(mutex_);
	std::list<t_failedLogins>::iterator iter = m_failedLogins.begin();
	while (iter != m_failedLogins.end())
	{
		const wxTimeSpan span = wxDateTime::UNow() - iter->time;
		const int delay = m_options.GetOptionVal(OPTION_RECONNECTDELAY);
		if (span.GetSeconds() >= delay)
		{
			std::list<t_failedLogins>::iterator prev = iter;
			++iter;
			m_failedLogins.erase(prev);
		}
		else if (!iter->critical && iter->server.GetHost() == server.GetHost() && iter->server.GetPort() == server.GetPort())
			return delay * 1000 - span.GetMilliseconds().GetLo();
		else if (iter->server == server)
			return delay * 1000 - span.GetMilliseconds().GetLo();
		else
			++iter;
	}

	return 0;
}

void CFileZillaEnginePrivate::OnTimer(int)
{
	if (!m_retryTimer) {
		return;
	}
	m_retryTimer = 0;

	if (!m_pCurrentCommand || m_pCurrentCommand->GetId() != Command::connect) {
		wxFAIL_MSG(_T("CFileZillaEnginePrivate::OnTimer called without pending Command::connect"));
		return;
	}
	wxASSERT(!IsConnected());

	m_pControlSocket.reset();

	ContinueConnect();
}

int CFileZillaEnginePrivate::ContinueConnect()
{
	scoped_lock lock(mutex_);

	const CConnectCommand *pConnectCommand = static_cast<CConnectCommand *>(m_pCurrentCommand.get());
	const CServer& server = pConnectCommand->GetServer();
	unsigned int delay = GetRemainingReconnectDelay(server);
	if (delay) {
		m_pLogging->LogMessage(MessageType::Status, wxPLURAL("Delaying connection for %d second due to previously failed connection attempt...", "Delaying connection for %d seconds due to previously failed connection attempt...", (delay + 999) / 1000), (delay + 999) / 1000);
		StopTimer(m_retryTimer);
		m_retryTimer = AddTimer(delay, true);
		return FZ_REPLY_WOULDBLOCK;
	}

	switch (server.GetProtocol())
	{
	case FTP:
	case FTPS:
	case FTPES:
	case INSECURE_FTP:
		m_pControlSocket = make_unique<CFtpControlSocket>(*this);
		break;
	case SFTP:
		m_pControlSocket = make_unique<CSftpControlSocket>(*this);
		break;
	case HTTP:
	case HTTPS:
		m_pControlSocket = make_unique<CHttpControlSocket>(*this);
		break;
	default:
		m_pLogging->LogMessage(MessageType::Debug_Warning, _T("Not a valid protocol: %d"), server.GetProtocol());
		return FZ_REPLY_SYNTAXERROR|FZ_REPLY_DISCONNECTED;
	}

	int res = m_pControlSocket->Connect(server);
	if (m_retryTimer)
		return FZ_REPLY_WOULDBLOCK;

	return res;
}

void CFileZillaEnginePrivate::InvalidateCurrentWorkingDirs(const CServerPath& path)
{
	scoped_lock lock(mutex_);

	wxASSERT(m_pControlSocket);
	const CServer* const pOwnServer = m_pControlSocket->GetCurrentServer();
	wxASSERT(pOwnServer);

	for (std::list<CFileZillaEnginePrivate*>::iterator iter = m_engineList.begin(); iter != m_engineList.end(); ++iter)
	{
		if (*iter == this)
			continue;

		CFileZillaEnginePrivate* pEngine = *iter;
		if (!pEngine->m_pControlSocket)
			continue;

		const CServer* const pServer = pEngine->m_pControlSocket->GetCurrentServer();
		if (!pServer || *pServer != *pOwnServer)
			continue;

		pEngine->m_pControlSocket->InvalidateCurrentWorkingDir(path);
	}
}

void CFileZillaEnginePrivate::operator()(CEventBase const& ev)
{
	scoped_lock lock(mutex_);

	Dispatch<CFileZillaEngineEvent, CCommandEvent, CAsyncRequestReplyEvent, CTimerEvent>(ev, this,
		&CFileZillaEnginePrivate::OnEngineEvent,
		&CFileZillaEnginePrivate::OnCommandEvent,
		&CFileZillaEnginePrivate::OnSetAsyncRequestReplyEvent,
		&CFileZillaEnginePrivate::OnTimer
		);
}

int CFileZillaEnginePrivate::CheckCommandPreconditions(CCommand const& command, bool checkBusy)
{
	if (!command.valid()) {
		return FZ_REPLY_SYNTAXERROR;
	}
	else if (checkBusy && IsBusy()) {
		return FZ_REPLY_BUSY;
	}
	else if (command.GetId() != Command::connect && command.GetId() != Command::disconnect && !IsConnected()) {
		return FZ_REPLY_NOTCONNECTED;
	}
	return FZ_REPLY_OK;
}

void CFileZillaEnginePrivate::OnCommandEvent()
{
	scoped_lock lock(mutex_);

	if (m_pCurrentCommand) {
		CCommand const& command = *m_pCurrentCommand;
		Command id = command.GetId();

		int res = CheckCommandPreconditions(command, false);
		if (res == FZ_REPLY_OK) {
			switch (command.GetId())
			{
			case Command::connect:
				res = Connect(static_cast<CConnectCommand const&>(command));
				break;
			case Command::disconnect:
				res = Disconnect(static_cast<CDisconnectCommand const&>(command));
				break;
			case Command::list:
				res = List(static_cast<CListCommand const&>(command));
				break;
			case Command::transfer:
				res = FileTransfer(static_cast<CFileTransferCommand const&>(command));
				break;
			case Command::raw:
				res = RawCommand(static_cast<CRawCommand const&>(command));
				break;
			case Command::del:
				res = Delete(static_cast<CDeleteCommand const&>(command));
				break;
			case Command::removedir:
				res = RemoveDir(static_cast<CRemoveDirCommand const&>(command));
				break;
			case Command::mkdir:
				res = Mkdir(static_cast<CMkdirCommand const&>(command));
				break;
			case Command::rename:
				res = Rename(static_cast<CRenameCommand const&>(command));
				break;
			case Command::chmod:
				res = Chmod(static_cast<CChmodCommand const&>(command));
				break;
			default:
				res = FZ_REPLY_SYNTAXERROR;
			}
		}

		if (id != Command::disconnect)
			res |= m_nControlSocketError;
		else if (res & FZ_REPLY_DISCONNECTED)
			res = FZ_REPLY_OK;
		m_nControlSocketError = 0;

		if (res != FZ_REPLY_WOULDBLOCK)
			ResetOperation(res);
	}
}

void CFileZillaEnginePrivate::DoCancel()
{
	scoped_lock lock(mutex_);
	if (!IsBusy())
		return;

	if (m_retryTimer) {
		wxASSERT(m_pCurrentCommand && m_pCurrentCommand->GetId() == Command::connect);

		m_pControlSocket.reset();

		m_pCurrentCommand.reset();

		StopTimer(m_retryTimer);
		m_retryTimer = 0;

		m_pLogging->LogMessage(MessageType::Error, _("Connection attempt interrupted by user"));
		COperationNotification *notification = new COperationNotification();
		notification->nReplyCode = FZ_REPLY_DISCONNECTED | FZ_REPLY_CANCELED;
		notification->commandId = Command::connect;
		AddNotification(notification);

		ClearQueuedLogs(true);
	}
	else {
		if (m_pControlSocket)
			m_pControlSocket->Cancel();
		else
			ResetOperation(FZ_REPLY_CANCELED);
	}
}

bool CFileZillaEnginePrivate::CheckAsyncRequestReplyPreconditions(std::unique_ptr<CAsyncRequestNotification> const& reply)
{
	if (!reply)
		return false;
	if (!IsBusy())
		return false;

	bool match;
	{
		scoped_lock l(notification_mutex_);
		match = reply->requestNumber == m_asyncRequestCounter;
	}

	return match && m_pControlSocket;
}

void CFileZillaEnginePrivate::OnSetAsyncRequestReplyEvent(std::unique_ptr<CAsyncRequestNotification> const& reply)
{
	scoped_lock lock(mutex_);
	if (!CheckAsyncRequestReplyPreconditions(reply)) {
		return;
	}

	m_pControlSocket->SetAlive();
	m_pControlSocket->SetAsyncRequestReply(reply.get());
}

int CFileZillaEnginePrivate::Init(wxEvtHandler *pEventHandler)
{
	scoped_lock lock(mutex_);
	m_pEventHandler = pEventHandler;
	return FZ_REPLY_OK;
}

int CFileZillaEnginePrivate::Execute(const CCommand &command)
{
	scoped_lock lock(mutex_);

	int res = CheckCommandPreconditions(command, true);
	if (res != FZ_REPLY_OK) {
		return res;
	}

	m_pCurrentCommand.reset(command.Clone());
	SendEvent<CCommandEvent>();

	return FZ_REPLY_WOULDBLOCK;
}

std::unique_ptr<CNotification> CFileZillaEnginePrivate::GetNextNotification()
{
	scoped_lock lock(notification_mutex_);

	if (m_NotificationList.empty()) {
		m_maySendNotificationEvent = true;
		return 0;
	}
	std::unique_ptr<CNotification> pNotification(m_NotificationList.front());
	m_NotificationList.pop_front();

	return pNotification;
}

bool CFileZillaEnginePrivate::SetAsyncRequestReply(std::unique_ptr<CAsyncRequestNotification> && pNotification)
{
	scoped_lock lock(mutex_);
	if (!CheckAsyncRequestReplyPreconditions(pNotification)) {
		return false;
	}

	SendEvent<CAsyncRequestReplyEvent>(std::move(pNotification));

	return true;
}

bool CFileZillaEnginePrivate::IsPendingAsyncRequestReply(std::unique_ptr<CAsyncRequestNotification> const& pNotification)
{
	if (!pNotification)
		return false;

	if (!IsBusy())
		return false;

	scoped_lock lock(notification_mutex_);
	return pNotification->requestNumber == m_asyncRequestCounter;
}

void CFileZillaEnginePrivate::SetActive(int direction)
{
	int const old_status = m_activeStatus[direction].fetch_or(0x1);
	if (!old_status) {
		AddNotification(new CActiveNotification(direction));
	}
}

bool CFileZillaEnginePrivate::IsActive(CFileZillaEngine::_direction direction)
{
	int const old = m_activeStatus[direction].exchange(0x2);
	if (!(old & 0x1)) {
		// Race: We might lose updates between the first exchange and this assignment.
		// It is harmless though
		m_activeStatus[direction] = 0;
		return false;
	}
	return true;
}

CTransferStatus CFileZillaEnginePrivate::GetTransferStatus(bool &changed)
{
	return transfer_status_.Get(changed);
}

int CFileZillaEnginePrivate::CacheLookup(const CServerPath& path, CDirectoryListing& listing)
{
	// TODO: Possible optimization: Atomically get current server. The cache has its own mutex.
	scoped_lock lock(mutex_);

	if (!IsConnected())
		return FZ_REPLY_ERROR;

	wxASSERT(m_pControlSocket->GetCurrentServer());

	bool is_outdated = false;
	if (!directory_cache_.Lookup(listing, *m_pControlSocket->GetCurrentServer(), path, true, is_outdated))
		return FZ_REPLY_ERROR;

	return FZ_REPLY_OK;
}

int CFileZillaEnginePrivate::Cancel()
{
	scoped_lock lock(mutex_);
	if (!IsBusy())
		return FZ_REPLY_OK;

	SendEvent<CFileZillaEngineEvent>(engineCancel);
	return FZ_REPLY_WOULDBLOCK;
}

void CFileZillaEnginePrivate::OnOptionsChanged(changed_options_t const&)
{
	bool queue_logs = ShouldQueueLogsFromOptions();
	scoped_lock lock(notification_mutex_);
	queue_logs_ = queue_logs;

	if (!queue_logs_) {
		SendQueuedLogs();
	}
}


CTransferStatusManager::CTransferStatusManager(CFileZillaEnginePrivate& engine)
	: engine_(engine)
{
}

void CTransferStatusManager::Reset()
{
	{
		scoped_lock lock(mutex_);
		status_.clear();
		send_state_ = 0;
	}

	engine_.AddNotification(new CTransferStatusNotification());
}

void CTransferStatusManager::Init(wxFileOffset totalSize, wxFileOffset startOffset, bool list)
{
	scoped_lock lock(mutex_);
	if (startOffset < 0)
		startOffset = 0;

	status_ = CTransferStatus(totalSize, startOffset, list);
	currentOffset_ = 0;
}

void CTransferStatusManager::SetStartTime()
{
	scoped_lock lock(mutex_);
	if (!status_)
		return;

	status_.started = wxDateTime::UNow();
}

void CTransferStatusManager::SetMadeProgress()
{
	scoped_lock lock(mutex_);
	if (!status_)
		return;

	status_.madeProgress = true;
}

void CTransferStatusManager::Update(wxFileOffset transferredBytes)
{
	CNotification* notification = 0;

	{
		int64_t oldOffset = currentOffset_.fetch_add(transferredBytes);
		if (!oldOffset) {
			scoped_lock lock(mutex_);
			if (!status_) {
				return;
			}

			if (!send_state_) {
				status_.currentOffset += currentOffset_.exchange(0);
				notification = new CTransferStatusNotification(status_);
			}
			send_state_ = 2;
		}
	}

	if (notification) {
		engine_.AddNotification(notification);
	}
}

CTransferStatus CTransferStatusManager::Get(bool &changed)
{
	scoped_lock lock(mutex_);
	if (!status_) {
		changed = false;
		send_state_ = 0;
	}
	else {
		status_.currentOffset += currentOffset_.exchange(0);
		if (send_state_ == 2) {
			changed = true;
			send_state_ = 1;
		}
		else {
			changed = false;
			send_state_ = 0;
		}
	}
	return status_;
}

bool CTransferStatusManager::empty()
{
	scoped_lock lock(mutex_);
	return status_.empty();
}
