#ifndef __COMMANDQUEUE_H__
#define __COMMANDQUEUE_H__

class CFileZillaEngine;
class CNotification;
class CState;
class CMainFrame;

DECLARE_EVENT_TYPE(fzEVT_GRANTEXCLUSIVEENGINEACCESS, -1)

class CCommandQueue
{
public:
	CCommandQueue(CFileZillaEngine *pEngine, CMainFrame* pMainFrame, CState* pState);
	~CCommandQueue();

	void ProcessCommand(CCommand *pCommand);
	void ProcessNextCommand();
	bool Idle() const;
	bool Cancel();
	bool Quit();
	void Finish(std::unique_ptr<COperationNotification> && pNotification);

	void RequestExclusiveEngine(bool requestExclusive);

	CFileZillaEngine* GetEngineExclusive(int requestId);
	void ReleaseEngine();
	bool EngineLocked() const { return m_exclusiveEngineLock; }

protected:
	void ProcessReply(int nReplyCode, Command commandId);

	void GrantExclusiveEngineRequest();

	CFileZillaEngine *m_pEngine;
	CMainFrame* m_pMainFrame;
	CState* m_pState;
	bool m_exclusiveEngineRequest;
	bool m_exclusiveEngineLock;
	int m_requestId;
	static int m_requestIdCounter;

	// Used to make this class reentrance-safe
	int m_inside_commandqueue{};

	std::list<std::unique_ptr<CCommand>> m_CommandList;

	bool m_quit{};
};

#endif //__COMMANDQUEUE_H__

