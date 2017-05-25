#ifndef __ASYNCREQUESTQUEUE_H__
#define __ASYNCREQUESTQUEUE_H__

#include <wx/timer.h>

class CMainFrame;
class CQueueView;
class CVerifyCertDialog;

class CAsyncRequestQueue : public wxEvtHandler
{
public:
	CAsyncRequestQueue(CMainFrame *pMainFrame);
	~CAsyncRequestQueue();

	bool AddRequest(CFileZillaEngine *pEngine, std::unique_ptr<CAsyncRequestNotification> && pNotification);
	void ClearPending(const CFileZillaEngine* pEngine);
	void RecheckDefaults();

	void SetQueue(CQueueView *pQueue);

	void TriggerProcessing();

protected:

	// Returns falls if main window doesn't have focus or is minimized.
	// Request attention if needed
	bool CheckWindowState();

	CMainFrame *m_pMainFrame;
	CQueueView *m_pQueueView;
	CVerifyCertDialog *m_pVerifyCertDlg;

	bool ProcessNextRequest();
	bool ProcessDefaults(CFileZillaEngine *pEngine, std::unique_ptr<CAsyncRequestNotification> & pNotification);

	struct t_queueEntry
	{
		t_queueEntry(CFileZillaEngine *e, std::unique_ptr<CAsyncRequestNotification>&& n)
			: pEngine(e)
			, pNotification(std::move(n))
		{
		}

		CFileZillaEngine *pEngine;
		std::unique_ptr<CAsyncRequestNotification> pNotification;
	};
	std::list<t_queueEntry> m_requestList;

	bool ProcessFileExistsNotification(t_queueEntry &entry);

	DECLARE_EVENT_TABLE()
	void OnProcessQueue(wxCommandEvent &event);
	void OnTimer(wxTimerEvent& event);

	// Reentrancy guard
	bool m_inside_request;

	wxTimer m_timer;
};

#endif //__ASYNCREQUESTQUEUE_H__
