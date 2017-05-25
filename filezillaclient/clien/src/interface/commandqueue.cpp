#include <filezilla.h>
#include "commandqueue.h"
#include "Mainfrm.h"
#include "state.h"
#include "recursive_operation.h"
#include "loginmanager.h"
#include "queue.h"
#include "RemoteListView.h"

DEFINE_EVENT_TYPE(fzEVT_GRANTEXCLUSIVEENGINEACCESS)

int CCommandQueue::m_requestIdCounter = 0;

CCommandQueue::CCommandQueue(CFileZillaEngine *pEngine, CMainFrame* pMainFrame, CState* pState)
{
	m_pEngine = pEngine;
	m_pMainFrame = pMainFrame;
	m_pState = pState;
	m_exclusiveEngineRequest = false;
	m_exclusiveEngineLock = false;
	m_requestId = 0;
}

CCommandQueue::~CCommandQueue()
{
}

bool CCommandQueue::Idle() const
{
	return m_CommandList.empty() && !m_exclusiveEngineLock;
}

void CCommandQueue::ProcessCommand(CCommand *pCommand)
{
	if (m_quit) {
		delete pCommand;
	}

	m_CommandList.emplace_back(pCommand);
	if (m_CommandList.size() == 1) {
		m_pState->NotifyHandlers(STATECHANGE_REMOTE_IDLE);
		ProcessNextCommand();
	}
}

void CCommandQueue::ProcessNextCommand()
{
	if (m_inside_commandqueue)
		return;

	if (m_exclusiveEngineLock)
		return;

	if (m_pEngine->IsBusy())
		return;

	++m_inside_commandqueue;

	if (m_CommandList.empty()) {
		// Possible sequence of events:
		// - Engine emits listing and operation finished
		// - Connection gets terminated
		// - Interface cannot obtain listing since not connected
		// - Yet getting operation successful
		// To keep things flowing, we need to advance the recursive operation.
		m_pState->GetRecursiveOperationHandler()->NextOperation();
	}

	while (!m_CommandList.empty()) {
		std::unique_ptr<CCommand> const& pCommand = m_CommandList.front();

		int res = m_pEngine->Execute(*pCommand);
		ProcessReply(res, pCommand->GetId());
		if (res == FZ_REPLY_WOULDBLOCK) {
			break;
		}
	}

	--m_inside_commandqueue;

	if (m_CommandList.empty()) {
		if (m_exclusiveEngineRequest)
			GrantExclusiveEngineRequest();
		else
			m_pState->NotifyHandlers(STATECHANGE_REMOTE_IDLE);

		if (!m_pState->SuccessfulConnect())
			m_pState->SetServer(0);
	}
}

bool CCommandQueue::Cancel()
{
	if (m_exclusiveEngineLock)
		return false;

	if (m_CommandList.empty())
		return true;

	m_CommandList.erase(++m_CommandList.begin(), m_CommandList.end());

	if (!m_pEngine)	{
		m_CommandList.clear();
		m_pState->NotifyHandlers(STATECHANGE_REMOTE_IDLE);
		return true;
	}

	int res = m_pEngine->Cancel();
	if (res == FZ_REPLY_WOULDBLOCK)
		return false;
	else {
		m_CommandList.clear();
		m_pState->NotifyHandlers(STATECHANGE_REMOTE_IDLE);
		return true;
	}
}

void CCommandQueue::Finish(std::unique_ptr<COperationNotification> && pNotification)
{
	if (m_exclusiveEngineLock) {
		m_pMainFrame->GetQueue()->ProcessNotification(m_pEngine, std::move(pNotification));
		return;
	}

	ProcessReply(pNotification->nReplyCode, pNotification->commandId);
}

void CCommandQueue::ProcessReply(int nReplyCode, Command commandId)
{
	if (nReplyCode == FZ_REPLY_WOULDBLOCK) {
		return;
	}
	if (nReplyCode & FZ_REPLY_DISCONNECTED) {
		if (commandId == Command::none && !m_CommandList.empty()) {
			// Pending event, has no relevance during command execution
			return;
		}
		if (nReplyCode & FZ_REPLY_PASSWORDFAILED)
			CLoginManager::Get().CachedPasswordFailed(*m_pState->GetServer());
	}

	if (m_CommandList.empty()) {
		return;
	}

	if (commandId != Command::connect &&
		commandId != Command::disconnect)
	{
		if (nReplyCode == FZ_REPLY_NOTCONNECTED) {
			// Try automatic reconnect
			const CServer* pServer = m_pState->GetServer();
			if (pServer) {
				m_CommandList.emplace_front(make_unique<CConnectCommand>(*pServer));
				return;
			}
		}
	}

	++m_inside_commandqueue;

	auto const& pCommand = m_CommandList.front();

	if (pCommand->GetId() == Command::list && nReplyCode != FZ_REPLY_OK) {
		if (nReplyCode & FZ_REPLY_LINKNOTDIR)
		{
			// Symbolic link does not point to a directory. Either points to file
			// or is completely invalid
			CListCommand* pListCommand = static_cast<CListCommand*>(pCommand.get());
			wxASSERT(pListCommand->GetFlags() & LIST_FLAG_LINK);

			m_pState->LinkIsNotDir(pListCommand->GetPath(), pListCommand->GetSubDir());
		}
		else
			m_pState->ListingFailed(nReplyCode);
		m_CommandList.pop_front();
	}
	else if (nReplyCode == FZ_REPLY_ALREADYCONNECTED && pCommand->GetId() == Command::connect) {
		m_CommandList.emplace_front(make_unique<CDisconnectCommand>());
	}
	else if (pCommand->GetId() == Command::connect && nReplyCode != FZ_REPLY_OK) {
		// Remove pending events
		auto it = ++m_CommandList.begin();
		while (it != m_CommandList.end() && (*it)->GetId() != Command::connect) {
			++it;
		}
		m_CommandList.erase(m_CommandList.begin(), it);

		// If this was an automatic reconnect during a recursive
		// operation, stop the recursive operation
		m_pState->GetRecursiveOperationHandler()->StopRecursiveOperation();
	}
	else if (pCommand->GetId() == Command::connect && nReplyCode == FZ_REPLY_OK) {
		m_pState->SetSuccessfulConnect();
		m_CommandList.pop_front();
	}
	else
		m_CommandList.pop_front();

	--m_inside_commandqueue;

	ProcessNextCommand();
}

void CCommandQueue::RequestExclusiveEngine(bool requestExclusive)
{
	wxASSERT(!m_exclusiveEngineLock || !requestExclusive);

	if (!m_exclusiveEngineRequest && requestExclusive)
	{
		m_requestId = ++m_requestIdCounter;
		if (m_requestId < 0)
		{
			m_requestIdCounter = 0;
			m_requestId = 0;
		}
		if (m_CommandList.empty())
		{
			m_pState->NotifyHandlers(STATECHANGE_REMOTE_IDLE);
			GrantExclusiveEngineRequest();
			return;
		}
	}
	if (!requestExclusive)
		m_exclusiveEngineLock = false;
	m_exclusiveEngineRequest = requestExclusive;
	m_pState->NotifyHandlers(STATECHANGE_REMOTE_IDLE);
}

void CCommandQueue::GrantExclusiveEngineRequest()
{
	wxASSERT(!m_exclusiveEngineLock);
	m_exclusiveEngineLock = true;
	m_exclusiveEngineRequest = false;

	wxCommandEvent *evt = new wxCommandEvent(fzEVT_GRANTEXCLUSIVEENGINEACCESS);
	evt->SetId(m_requestId);
	m_pMainFrame->GetQueue()->GetEventHandler()->QueueEvent(evt);
}

CFileZillaEngine* CCommandQueue::GetEngineExclusive(int requestId)
{
	if (!m_exclusiveEngineLock)
		return 0;

	if (requestId != m_requestId)
		return 0;

	return m_pEngine;
}


void CCommandQueue::ReleaseEngine()
{
	m_exclusiveEngineLock = false;

	ProcessNextCommand();
}

bool CCommandQueue::Quit()
{
	m_quit = true;
	return Cancel();
}