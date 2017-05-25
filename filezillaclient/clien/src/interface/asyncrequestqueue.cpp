#include <filezilla.h>

#include "asyncrequestqueue.h"
#include "defaultfileexistsdlg.h"
#include "fileexistsdlg.h"
#include "loginmanager.h"
#include "Mainfrm.h"
#include "Options.h"
#include "queue.h"
#include "verifycertdialog.h"
#include "verifyhostkeydialog.h"

DECLARE_EVENT_TYPE(fzEVT_PROCESSASYNCREQUESTQUEUE, -1)
DEFINE_EVENT_TYPE(fzEVT_PROCESSASYNCREQUESTQUEUE)

BEGIN_EVENT_TABLE(CAsyncRequestQueue, wxEvtHandler)
EVT_COMMAND(wxID_ANY, fzEVT_PROCESSASYNCREQUESTQUEUE, CAsyncRequestQueue::OnProcessQueue)
EVT_TIMER(wxID_ANY, CAsyncRequestQueue::OnTimer)
END_EVENT_TABLE()

CAsyncRequestQueue::CAsyncRequestQueue(CMainFrame *pMainFrame)
{
	m_pMainFrame = pMainFrame;
	m_pQueueView = 0;
	m_pVerifyCertDlg = new CVerifyCertDialog;
	m_inside_request = false;
	m_timer.SetOwner(this);
}

CAsyncRequestQueue::~CAsyncRequestQueue()
{
	delete m_pVerifyCertDlg;
}

bool CAsyncRequestQueue::ProcessDefaults(CFileZillaEngine *pEngine, std::unique_ptr<CAsyncRequestNotification> & pNotification)
{
	// Process notifications, see if we have defaults not requirering user interaction.
	switch (pNotification->GetRequestID())
	{
	case reqId_fileexists:
		{
			CFileExistsNotification *pFileExistsNotification = static_cast<CFileExistsNotification *>(pNotification.get());

			// Get the action, go up the hierarchy till one is found
			enum CFileExistsNotification::OverwriteAction action = pFileExistsNotification->overwriteAction;
			if (action == CFileExistsNotification::unknown)
				action = CDefaultFileExistsDlg::GetDefault(pFileExistsNotification->download);
			if (action == CFileExistsNotification::unknown) {
				int option = COptions::Get()->GetOptionVal(pFileExistsNotification->download ? OPTION_FILEEXISTS_DOWNLOAD : OPTION_FILEEXISTS_UPLOAD);
				if (option < CFileExistsNotification::unknown || option >= CFileExistsNotification::ACTION_COUNT)
					action = CFileExistsNotification::unknown;
				else
					action = (enum CFileExistsNotification::OverwriteAction)option;
			}

			// Ask and rename options require user interaction
			if (action == CFileExistsNotification::unknown || action == CFileExistsNotification::ask || action == CFileExistsNotification::rename)
				break;

			if (action == CFileExistsNotification::resume && pFileExistsNotification->ascii) {
				// Check if resuming ascii files is allowed
				if (!COptions::Get()->GetOptionVal(OPTION_ASCIIRESUME))
					// Overwrite instead
					action = CFileExistsNotification::overwrite;
			}

			pFileExistsNotification->overwriteAction = action;

			pEngine->SetAsyncRequestReply(std::move(pNotification));

			return true;
		}
	case reqId_hostkey:
	case reqId_hostkeyChanged:
		{
			auto & hostKeyNotification = static_cast<CHostKeyNotification&>(*pNotification.get());

			if (!CVerifyHostkeyDialog::IsTrusted(hostKeyNotification))
				break;

			hostKeyNotification.m_trust = true;
			hostKeyNotification.m_alwaysTrust = false;

			pEngine->SetAsyncRequestReply(std::move(pNotification));

			return true;
		}
	case reqId_certificate:
		{
			auto & certNotification = static_cast<CCertificateNotification&>(*pNotification.get());

			if (!m_pVerifyCertDlg->IsTrusted(certNotification))
				break;

			certNotification.m_trusted = true;
			pEngine->SetAsyncRequestReply(std::move(pNotification));

			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

bool CAsyncRequestQueue::AddRequest(CFileZillaEngine *pEngine, std::unique_ptr<CAsyncRequestNotification> && pNotification)
{
	ClearPending(pEngine);

	if (ProcessDefaults(pEngine, pNotification))
		return false;

	m_requestList.emplace_back(pEngine, std::move(pNotification));

	if (m_requestList.size() == 1) {
		QueueEvent(new wxCommandEvent(fzEVT_PROCESSASYNCREQUESTQUEUE));
	}

	return true;
}

bool CAsyncRequestQueue::ProcessNextRequest()
{
	if (m_requestList.empty())
		return true;

	t_queueEntry &entry = m_requestList.front();

	if (!entry.pEngine || !entry.pEngine->IsPendingAsyncRequestReply(entry.pNotification)) {
		m_requestList.pop_front();
		return true;
	}

	if (entry.pNotification->GetRequestID() == reqId_fileexists) {
		if (!ProcessFileExistsNotification(entry)) {
			return false;
		}
	}
	else if (entry.pNotification->GetRequestID() == reqId_interactiveLogin) {
		auto & notification = static_cast<CInteractiveLoginNotification&>(*entry.pNotification.get());

		if (CLoginManager::Get().GetPassword(notification.server, true, wxString(), notification.GetChallenge()))
			notification.passwordSet = true;
		else {
			// Retry with prompt

			if (!CheckWindowState())
				return false;

			if (CLoginManager::Get().GetPassword(notification.server, false, wxString(), notification.GetChallenge()))
				notification.passwordSet = true;
		}

		entry.pEngine->SetAsyncRequestReply(std::move(entry.pNotification));
	}
	else if (entry.pNotification->GetRequestID() == reqId_hostkey || entry.pNotification->GetRequestID() == reqId_hostkeyChanged) {
		if (!CheckWindowState())
			return false;

		auto & notification = static_cast<CHostKeyNotification&>(*entry.pNotification.get());

		if (CVerifyHostkeyDialog::IsTrusted(notification)) {
			notification.m_trust = true;
			notification.m_alwaysTrust = false;
		}
		else
			CVerifyHostkeyDialog::ShowVerificationDialog(m_pMainFrame, notification);

		entry.pEngine->SetAsyncRequestReply(std::move(entry.pNotification));
	}
	else if (entry.pNotification->GetRequestID() == reqId_certificate) {
		if (!CheckWindowState())
			return false;

		auto & notification = static_cast<CCertificateNotification&>(*entry.pNotification.get());
		m_pVerifyCertDlg->ShowVerificationDialog(notification);

		entry.pEngine->SetAsyncRequestReply(std::move(entry.pNotification));
	}
	else {
		entry.pEngine->SetAsyncRequestReply(std::move(entry.pNotification));
	}

	RecheckDefaults();
	m_requestList.pop_front();

	return true;
}

bool CAsyncRequestQueue::ProcessFileExistsNotification(t_queueEntry &entry)
{
	auto & notification = static_cast<CFileExistsNotification&>(*entry.pNotification.get());

	// Get the action, go up the hierarchy till one is found
	enum CFileExistsNotification::OverwriteAction action = notification.overwriteAction;
	if (action == CFileExistsNotification::unknown)
		action = CDefaultFileExistsDlg::GetDefault(notification.download);
	if (action == CFileExistsNotification::unknown) {
		int option = COptions::Get()->GetOptionVal(notification.download ? OPTION_FILEEXISTS_DOWNLOAD : OPTION_FILEEXISTS_UPLOAD);
		if (option <= CFileExistsNotification::unknown || option >= CFileExistsNotification::ACTION_COUNT)
			action = CFileExistsNotification::ask;
		else
			action = (enum CFileExistsNotification::OverwriteAction)option;
	}

	if (action == CFileExistsNotification::ask) {
		if (!CheckWindowState())
			return false;

		CFileExistsDlg dlg(&notification);
		dlg.Create(m_pMainFrame);
		int res = dlg.ShowModal();

		if (res == wxID_OK) {
			action = dlg.GetAction();

			bool directionOnly, queueOnly;
			if (dlg.Always(directionOnly, queueOnly)) {
				if (!queueOnly) {
					if (notification.download || !directionOnly)
						CDefaultFileExistsDlg::SetDefault(true, action);

					if (!notification.download || !directionOnly)
						CDefaultFileExistsDlg::SetDefault(false, action);
				}
				else {
					// For the notifications already in the request queue, we have to set the queue action directly
					for (auto iter = ++m_requestList.begin(); iter != m_requestList.end(); ++iter) {
						if (!iter->pNotification || iter->pNotification->GetRequestID() != reqId_fileexists)
							continue;
						auto & p = static_cast<CFileExistsNotification&>(*iter->pNotification.get());

						if (!directionOnly || notification.download == p.download)
							p.overwriteAction = CFileExistsNotification::OverwriteAction(action);
					}

					TransferDirection direction;
					if (directionOnly) {
						if (notification.download)
							direction = TransferDirection::download;
						else
							direction = TransferDirection::upload;
					}
					else
						direction = TransferDirection::both;

					if (m_pQueueView)
						m_pQueueView->SetDefaultFileExistsAction(action, direction);
				}
			}
		}
		else
			action = CFileExistsNotification::skip;
	}

	if (action == CFileExistsNotification::unknown || action == CFileExistsNotification::ask)
		action = CFileExistsNotification::skip;

	if (action == CFileExistsNotification::resume && notification.ascii) {
		// Check if resuming ascii files is allowed
		if (!COptions::Get()->GetOptionVal(OPTION_ASCIIRESUME))
			// Overwrite instead
			action = CFileExistsNotification::overwrite;
	}

	switch (action)
	{
		case CFileExistsNotification::rename:
		{
			if (!CheckWindowState())
				return false;

			wxString msg;
			wxString defaultName;
			if (notification.download) {
				msg.Printf(_("The file %s already exists.\nPlease enter a new name:"), notification.localFile);
				wxFileName fn = notification.localFile;
				defaultName = fn.GetFullName();
			}
			else {
				wxString fullName = notification.remotePath.GetPath() + notification.remoteFile;
				msg.Printf(_("The file %s already exists.\nPlease enter a new name:"), fullName);
				defaultName = notification.remoteFile;
			}
			wxTextEntryDialog dlg(m_pMainFrame, msg, _("Rename file"), defaultName);

			// Repeat until user cancels or enters a new name
			for (;;) {
				int res = dlg.ShowModal();
				if (res == wxID_OK) {
					if (dlg.GetValue().empty())
						continue; // Disallow empty names
					if (dlg.GetValue() == defaultName) {
						wxMessageDialog dlg2(m_pMainFrame, _("You did not enter a new name for the file. Overwrite the file instead?"), _("Filename unchanged"),
							wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION | wxCANCEL);
						int res = dlg2.ShowModal();

						if (res == wxID_CANCEL)
							notification.overwriteAction = CFileExistsNotification::skip;
						else if (res == wxID_NO)
							continue;
						else
							notification.overwriteAction = CFileExistsNotification::skip;
					}
					else {
						notification.overwriteAction = CFileExistsNotification::rename;
						notification.newName = dlg.GetValue();

						// If request got processed successfully, notify queue about filename change
						if (entry.pEngine->SetAsyncRequestReply(std::move(entry.pNotification)) && m_pQueueView)
							m_pQueueView->RenameFileInTransfer(entry.pEngine, dlg.GetValue(), notification.download);
						return true;
					}
				}
				else
					notification.overwriteAction = CFileExistsNotification::skip;
				break;
			}
		}
		break;
		default:
			notification.overwriteAction = action;
			break;
	}

	entry.pEngine->SetAsyncRequestReply(std::move(entry.pNotification));
	return true;
}

void CAsyncRequestQueue::ClearPending(const CFileZillaEngine *pEngine)
{
	if (m_requestList.empty())
		return;

	// Remove older requests coming from the same engine, but never the first
	// entry in the list as that one displays a dialog at this moment.
	for (auto iter = ++m_requestList.begin(); iter != m_requestList.end(); ++iter) {
		if (iter->pEngine == pEngine) {
			m_requestList.erase(iter);

			// At most one pending request per engine possible,
			// so we can stop here
			break;
		}
	}
}

void CAsyncRequestQueue::RecheckDefaults()
{
	if (m_requestList.size() <= 1)
		return;

	std::list<t_queueEntry>::iterator cur, next;
	cur = ++m_requestList.begin();
	while (cur != m_requestList.end()) {
		next = cur;
		++next;

		if (ProcessDefaults(cur->pEngine, cur->pNotification))
			m_requestList.erase(cur);
		cur = next;
	}
}

void CAsyncRequestQueue::SetQueue(CQueueView *pQueue)
{
	m_pQueueView = pQueue;
}

void CAsyncRequestQueue::OnProcessQueue(wxCommandEvent &)
{
	if (m_inside_request)
		return;

	m_inside_request = true;
	bool success = ProcessNextRequest();
	m_inside_request = false;

	if (success && !m_requestList.empty()) {
		QueueEvent(new wxCommandEvent(fzEVT_PROCESSASYNCREQUESTQUEUE));
	}
}

void CAsyncRequestQueue::TriggerProcessing()
{
	if (m_inside_request)
		return;

	QueueEvent(new wxCommandEvent(fzEVT_PROCESSASYNCREQUESTQUEUE));
}

bool CAsyncRequestQueue::CheckWindowState()
{
	m_timer.Stop();
	wxMouseState mouseState = wxGetMouseState();
	if (mouseState.LeftIsDown() || mouseState.MiddleIsDown() || mouseState.RightIsDown()) {
		m_timer.Start(100, true);
		return false;
	}

#ifndef __WXMAC__
	if (m_pMainFrame->IsIconized())
	{
#ifndef __WXGTK__
		m_pMainFrame->Show();
		m_pMainFrame->Iconize(true);
		m_pMainFrame->RequestUserAttention();
#endif
		return false;
	}

	wxWindow* pFocus = m_pMainFrame->FindFocus();
	while (pFocus && pFocus != m_pMainFrame)
		pFocus = pFocus->GetParent();
	if (!pFocus)
		m_pMainFrame->RequestUserAttention();
#endif

	return true;
}

void CAsyncRequestQueue::OnTimer(wxTimerEvent&)
{
	TriggerProcessing();
}

