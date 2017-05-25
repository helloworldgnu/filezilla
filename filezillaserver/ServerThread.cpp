// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2016 - Tim Kosse <tim.kosse@filezilla-project.org>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// ServerThread.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "iputils.h"
#include "ServerThread.h"
#include "ControlSocket.h"
#include "transfersocket.h"
#include "Options.h"
#include "version.h"
#include "Permissions.h"
#include "ExternalIpCheck.h"
#include "autobanmanager.h"
#include "hash_thread.h"

#include <algorithm>

std::map<int, t_socketdata> CServerThread::m_userids;
std::recursive_mutex CServerThread::m_global_mutex;
std::map<CStdString, int> CServerThread::m_userIPs;
std::vector<CServerThread*> CServerThread::m_sInstanceList;
std::map<CStdString, int> CServerThread::m_antiHammerInfo;
CHashThread* CServerThread::m_hashThread = 0;

/////////////////////////////////////////////////////////////////////////////
// CServerThread

CServerThread::CServerThread(int nNotificationMessageId)
	: m_nNotificationMessageId(nNotificationMessageId)
{
	m_lastLimits[0] = m_lastLimits[1] = 0;
}

CServerThread::~CServerThread()
{
}

BOOL CServerThread::InitInstance()
{
	BOOL res = TRUE;
	WSADATA wsaData;

	WORD wVersionRequested = MAKEWORD(2, 2);
	int nResult = WSAStartup(wVersionRequested, &wsaData);
	if (nResult != 0)
		res = FALSE;
	else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		res = FALSE;
	}

	threadSocketData_ = new CAsyncSocketEx;
	threadSocketData_->InitAsyncSocketExInstance();

	m_timerid = SetTimer(0, 0, 1000, 0);
	m_nRateTimer = SetTimer(0, 0, 100, 0);

	// Reduce anti hammer value twice an hour
	m_antiHammerTimer = SetTimer(0, 0, 1800 * 1000, 0);

	m_nRecvCount = 0;
	m_nSendCount = 0;
	m_pOptions = new COptions;
	m_pAutoBanManager = new CAutoBanManager(m_pOptions);
	m_pPermissions = new CPermissions([this]() { OnPermissionsUpdated(); });

	{
		simple_lock lock(m_global_mutex);
		if (m_sInstanceList.empty()) {
			m_bIsMaster = TRUE;
		}
		else {
			m_bIsMaster = FALSE;
		}
		m_sInstanceList.push_back(this);
	}

	m_nLoopCount = 0;

	simple_lock lock(m_mutex);
	if (!m_bIsMaster)
		m_pExternalIpCheck = NULL;
	else {
		m_pExternalIpCheck = new CExternalIpCheck(this);
		m_hashThread = new CHashThread();
	}

	m_throttled = 0;

	return TRUE;
}

DWORD CServerThread::ExitInstance()
{
	delete threadSocketData_;

	ASSERT(m_pPermissions);
	delete m_pPermissions;
	m_pPermissions = 0;
	delete m_pAutoBanManager;
	m_pAutoBanManager = 0;
	delete m_pOptions;
	m_pOptions = 0;
	KillTimer(0, m_timerid);
	KillTimer(0, m_nRateTimer);
	WSACleanup();
	m_hashThread->Stop(this);

	if (m_bIsMaster) {
		simple_lock lock(m_mutex);
		delete m_pExternalIpCheck;
		m_pExternalIpCheck = NULL;
	}

	simple_lock lock(m_global_mutex);
	auto it = std::find(m_sInstanceList.begin(), m_sInstanceList.end(), this);
	if (it != m_sInstanceList.end()) {
		m_sInstanceList.erase(it);
	}
	if (!m_sInstanceList.empty()) {
		m_sInstanceList.front()->m_bIsMaster = TRUE;
	}
	else {
		delete m_hashThread;
		m_hashThread = 0;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CServerThread

const int CServerThread::GetNumConnections()
{
	simple_lock lock(m_mutex);
	int num = m_LocalUserIDs.size();
	return num;
}

void CServerThread::AddSocket(SOCKET sockethandle, bool ssl)
{
	PostThreadMessage(WM_FILEZILLA_THREADMSG, ssl ? FTM_NEWSOCKET_SSL : FTM_NEWSOCKET, (LPARAM)sockethandle);
}

#define IDMAX 1000000000
int CServerThread::CalcUserID()
{
	if (m_userids.size() >= IDMAX)
		return -1;
	static int curid=0;
	curid++;
	if (curid==IDMAX)
		curid=1;
	while (m_userids.find(curid) != m_userids.end())
	{
		curid++;
		if (curid == IDMAX)
			curid=1;
	}
	return curid;
}

void CServerThread::AddNewSocket(SOCKET sockethandle, bool ssl)
{
	CControlSocket *socket = new CControlSocket(*this);
	if (!socket->Attach(sockethandle))
	{
		socket->SendStatus(_T("Failed to attach socket."), 1);
		closesocket(sockethandle);
		delete socket;
		return;
	}

	CStdString ip;
	UINT port = 0;
	if (socket->GetPeerName(ip, port)) {
		if (socket->GetFamily() == AF_INET6)
			ip = GetIPV6ShortForm(ip);
		socket->m_RemoteIP = ip;
	}
	else {
		socket->m_RemoteIP = _T("ip unknown");
		socket->SendStatus(_T("Can't get remote IP, disconnected"), 1);
		socket->Close();
		delete socket;
		return;
	}
	scoped_lock glock(m_global_mutex);
	int userid = CalcUserID();
	if (userid == -1) {
		glock.unlock();
		socket->SendStatus(_T("Refusing connection, server too busy!"), 1);
		socket->Send(_T("421 Server too busy, closing connection. Please retry later!"));
		socket->Close();
		delete socket;
		return;
	}
	socket->m_userid = userid;
	t_socketdata data;
	data.pSocket = socket;
	data.pThread = this;
	m_userids[userid] = data;

	// Check if remote IP is blocked due to hammering
	std::map<CStdString, int>::const_iterator iter = m_antiHammerInfo.find(ip);
	if (iter != m_antiHammerInfo.end() && iter->second > 10)
		socket->AntiHammerIncrease(25); // ~6 secs delay
	glock.unlock();
	{
		simple_lock lock(m_mutex);
		m_LocalUserIDs[userid] = socket;
	}

	t_connectiondata_add *conndata = new t_connectiondata_add;
	t_connop *op = new t_connop;
	op->data = conndata;
	op->op = USERCONTROL_CONNOP_ADD;
	op->userid = userid;
	conndata->pThread = this;

	conndata->port = port;
	_tcsncpy(conndata->ip, socket->m_RemoteIP, t_connectiondata_add::ip_size);

	SendNotification(FSM_CONNECTIONDATA, (LPARAM)op);

	if (ssl && !socket->InitImplicitSsl())
		return;

	socket->AsyncSelect(FD_READ|FD_WRITE|FD_CLOSE);

	UINT localPort = 0;
	CStdString localHost;
	if( socket->GetSockName(localHost, localPort) ) {
		CStdString msg;
		msg.Format(_T("Connected on port %u, sending welcome message..."), localPort);
		socket->SendStatus(msg, 0);
	}
	else {
		socket->SendStatus(_T("Connected, sending welcome message..."), 0);
	}

	CStdString msg;
	if (m_pOptions->GetOptionVal(OPTION_ENABLE_HASH))
		msg = _T("EXPERIMENTAL BUILD\nNOT FOR PRODUCTION USE\n\nImplementing draft-bryan-ftp-hash-06");
	else
		msg = m_pOptions->GetOption(OPTION_WELCOMEMESSAGE);
	if (m_RawWelcomeMessage != msg) {
		m_RawWelcomeMessage = msg;
		m_ParsedWelcomeMessage.clear();

		msg.Replace(_T("%%"), _T("\001"));
		msg.Replace(_T("%v"), GetProductVersionString());
		msg.Replace(_T("\001"), _T("%"));

		ASSERT(msg != _T(""));
		int oldpos = 0;
		msg.Replace(_T("\r\n"), _T("\n"));
		int pos = msg.Find(_T("\n"));
		CStdString line;
		while (pos != -1) {
			ASSERT(pos);
			m_ParsedWelcomeMessage.push_back(_T("220-") +  msg.Mid(oldpos, pos-oldpos) );
			oldpos = pos + 1;
			pos = msg.Find(_T("\n"), oldpos);
		}

		line = msg.Mid(oldpos);
		if (line != _T(""))
			m_ParsedWelcomeMessage.push_back(_T("220 ") + line);
		else {
			m_ParsedWelcomeMessage.back()[3] = 0;
		}
	}

	bool hideStatus = m_pOptions->GetOptionVal(OPTION_WELCOMEMESSAGE_HIDE) != 0;
	ASSERT(!m_ParsedWelcomeMessage.empty());

	CStdString reply;
	for (auto const& line : m_ParsedWelcomeMessage) {
		reply += socket->PrepareSend(line, !hideStatus);
	}
	socket->Send(reply, false, false);
}

int CServerThread::OnThreadMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_FILEZILLA_THREADMSG) {
		if (wParam == FTM_NEWSOCKET) //Add a new socket to this thread
			AddNewSocket((SOCKET)lParam, false);
		else if (wParam == FTM_NEWSOCKET_SSL) //Add a new socket to this thread
			AddNewSocket((SOCKET)lParam, true);
		else if (wParam == FTM_DELSOCKET) { //Remove a socket from this thread
			CControlSocket *socket = GetControlSocket(lParam);
			{
				simple_lock lock(m_mutex);
				if (m_LocalUserIDs.find(lParam) != m_LocalUserIDs.end())
					m_LocalUserIDs.erase(m_LocalUserIDs.find(lParam));
			}
			if (socket) {
				socket->Close();
				simple_lock lock(m_global_mutex);
				if (m_userids.find(lParam) != m_userids.end())
					m_userids.erase(m_userids.find(lParam));
				delete socket;
			}
			simple_lock lock(m_mutex);
			if (m_bQuit) {
				if (!m_LocalUserIDs.size()) {
					SendNotification(FSM_THREADCANQUIT, (LPARAM)this);
				}
			}
		}
		else if (wParam == FTM_COMMAND) { //Process a command sent from a client
			CControlSocket *socket=GetControlSocket(lParam);
			if (socket)
				socket->ParseCommand();
		}
		else if (wParam == FTM_TRANSFERMSG) {
			CControlSocket *socket=GetControlSocket(lParam);
			if (socket)
				socket->ProcessTransferMsg();
		}
		else if (wParam == FTM_GOOFFLINE) {
			simple_lock lock(m_mutex);
			m_bQuit = true;
			int count = m_LocalUserIDs.size();
			if (!count) {
				SendNotification(FSM_THREADCANQUIT, (LPARAM)this);
				return 0;
			}
			if (lParam == 2) {
				return 0;
			}

			for (auto & it : m_LocalUserIDs) {
				switch (lParam) {
				case 0:
				default:
					it.second->ForceClose(0);
					break;
				case 1:
					it.second->WaitGoOffline();
					break;
				}
			}
		}
		else if (wParam == FTM_CONTROL)
			ProcessControlMessage((t_controlmessage *)lParam);
		else if (wParam == FTM_HASHRESULT) {
			CHashThread::_algorithm alg;
			CStdString hash;
			CStdString file;
			int hash_res = GetHashThread().GetResult(lParam, alg, hash, file);
			simple_lock lock(m_mutex);

			for (auto & it : m_LocalUserIDs) {
				it.second->ProcessHashResult(lParam, hash_res, alg, hash, file);
			}
		}
	}
	else if (Msg == WM_TIMER)
		OnTimer(wParam, lParam);
	return 0;
}

void CServerThread::OnTimer(WPARAM wParam,LPARAM lParam)
{
	if (wParam == m_timerid) {
		simple_lock lock(m_mutex);
		/*
		 * Check timeouts and collect transfer file offsets.
		 * Do both in the same loop to save performance.
		 */

		/*
		 * Maximum memory required for file offsets:
		 * 2 unused prefix bytes, will be filled by CServer,
		 * This avoids buffer copying.
		 * For each connection 4 bytes for the userid
		 * and 8 for the offset.
		 * We do not need to store the number of elements, this
		 * information can be calculated from the length if neccessary.
		 */
		int bufferLen = 2 + m_LocalUserIDs.size() * 12;
		unsigned char* buffer = new unsigned char[bufferLen];
		unsigned char* p = buffer + 2;
		for (std::map<int, CControlSocket *>::iterator iter = m_LocalUserIDs.begin(); iter != m_LocalUserIDs.end(); ++iter) {
			CControlSocket* pSocket = iter->second;
			CTransferSocket* pTransferSocket = pSocket->GetTransferSocket();
			if (pTransferSocket && pTransferSocket->WasActiveSinceCheck()) {
				memcpy(p, &iter->first, 4);
				p += 4;
				__int64 offset = pTransferSocket->GetCurrentFileOffset();
				memcpy(p, &offset, 8);
				p += 8;
			}
			iter->second->CheckForTimeout();
		}

		if ((p - buffer) <= 2) {
			delete [] buffer;
		}
		else {
			t_connectiondata_transferoffsets* conndata = new t_connectiondata_transferoffsets;
			conndata->pData = buffer;
			conndata->len = p - buffer;
			t_connop* op = new t_connop;
			op->data = conndata;
			op->op = USERCONTROL_CONNOP_TRANSFEROFFSETS;
			SendNotification(FSM_CONNECTIONDATA, (LPARAM)op);
		}

		// Check if master thread has changed
		if (m_bIsMaster && !m_pExternalIpCheck) {
			m_pExternalIpCheck = new CExternalIpCheck(this);
		}
	}
	else if (wParam == m_nRateTimer) {
		if (m_nSendCount) {
			SendNotification(FSM_SEND, m_nSendCount);
			m_nSendCount = 0;
		}
		if (m_nRecvCount) {
			SendNotification(FSM_RECV, m_nRecvCount);
			m_nRecvCount = 0;
		}

		if (m_bIsMaster) {
			simple_lock glock(m_global_mutex);

			// Only update the speed limits from the rule set every 2 seconds to improve performance
			if (!m_nLoopCount) {
				m_lastLimits[download] = m_pOptions->GetCurrentSpeedLimit(download);
				m_lastLimits[upload] = m_pOptions->GetCurrentSpeedLimit(upload);
			}
			++m_nLoopCount %= 20;

			// Gather transfer statistics if a speed limit is set
			if (m_lastLimits[download] != -1 || m_lastLimits[upload] != -1) {
				for (auto & pThread : m_sInstanceList) {
					pThread->GatherTransferedBytes();
				}
			}

			for (int i = 0; i < 2; ++i) {
				long long limit = m_lastLimits[i];

				if (limit == -1) {
					for (auto & pThread : m_sInstanceList) {
						simple_lock lock(pThread->m_mutex);
						pThread->m_SlQuotas[i].nBytesAllowedToTransfer = -1;
						pThread->m_SlQuotas[i].nTransferred = 0;
					}
					continue;
				}

				limit *= 100;

				long long nRemaining = limit;
				long long nThreadLimit = limit / m_sInstanceList.size();

				std::vector<CServerThread *> fullUsageList;

				for (auto & pThread : m_sInstanceList) {
					pThread->m_mutex.lock();
					long long r = pThread->m_SlQuotas[i].nBytesAllowedToTransfer - pThread->m_SlQuotas[i].nTransferred;
					if ( r > 0 && pThread->m_SlQuotas[i].nBytesAllowedToTransfer <= nThreadLimit) {
						pThread->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
						nRemaining -= pThread->m_SlQuotas[i].nTransferred;
						pThread->m_SlQuotas[i].nTransferred = 0;
					}
					else if (r > 0 && pThread->m_SlQuotas[i].nTransferred < nThreadLimit) {
						pThread->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
						nRemaining -= pThread->m_SlQuotas[i].nTransferred;
						pThread->m_SlQuotas[i].nTransferred = 0;
					}
					else {
						fullUsageList.push_back(pThread);
						// Don't unlock thread here, do it later
						continue;
					}
					pThread->m_mutex.unlock();
				}

				// fullUsageList now contains all threads which did use up their assigned quota
				if (!fullUsageList.empty()) {
					std::vector<CServerThread *> fullUsageList2;
					nThreadLimit = nRemaining / fullUsageList.size();
					for (auto & pThread : fullUsageList) {
						// Thread has already been locked
						long long r = pThread->m_SlQuotas[i].nBytesAllowedToTransfer - pThread->m_SlQuotas[i].nTransferred;
						if (r > 0) {
							if (pThread->m_SlQuotas[i].nTransferred > nThreadLimit)
								pThread->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
							else
								pThread->m_SlQuotas[i].nBytesAllowedToTransfer = pThread->m_SlQuotas[i].nTransferred;
							pThread->m_SlQuotas[i].nTransferred = 0;
							nRemaining -= pThread->m_SlQuotas[i].nBytesAllowedToTransfer;
						}
						else {
							fullUsageList2.push_back(pThread);
							// Don't unlock thread here either, do it later
							continue;
						}
						pThread->m_mutex.unlock();
					}

					if (!fullUsageList2.empty()) {
						nThreadLimit = nRemaining / fullUsageList2.size();
						for (auto & pThread : fullUsageList2) {
							pThread->m_SlQuotas[i].nTransferred = 0;
							pThread->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;

							// Finally unlock threads
							pThread->m_mutex.unlock();
						}
					}
				}
			}
		}
		ProcessNewSlQuota();
	}
	else if (m_pExternalIpCheck && wParam == m_pExternalIpCheck->GetTimerID()) {
		simple_lock lock(m_mutex);
		m_pExternalIpCheck->OnTimer();
	}
	else if (wParam == m_antiHammerTimer && m_bIsMaster) {
		AntiHammerDecay();
	}
}

const int CServerThread::GetGlobalNumConnections()
{
	simple_lock lock(m_global_mutex);
	return m_userids.size();
}

CControlSocket * CServerThread::GetControlSocket(int userid)
{
	CControlSocket *ret=0;
	simple_lock lock(m_mutex);
	std::map<int, CControlSocket *>::iterator iter=m_LocalUserIDs.find(userid);
	if (iter != m_LocalUserIDs.end())
		ret = iter->second;
	return ret;
}

void CServerThread::ProcessControlMessage(t_controlmessage *msg)
{
	if (msg->command == USERCONTROL_KICK) {
		CControlSocket *socket = GetControlSocket(msg->socketid);
		if (socket)
			socket->ForceClose(4);
	}
	delete msg;
}

bool CServerThread::IsReady()
{
	return !m_bQuit;
}

int CServerThread::GetIpCount(const CStdString &ip) const
{
	int count = 0;
	simple_lock lock(m_global_mutex);
	std::map<CStdString, int>::iterator iter = m_userIPs.find(ip);
	if (iter != m_userIPs.end())
		count = iter->second;
	return count;
}

void CServerThread::IncIpCount(const CStdString &ip)
{
	simple_lock lock(m_global_mutex);
	std::map<CStdString, int>::iterator iter=m_userIPs.find(ip);
	if (iter != m_userIPs.end())
		++iter->second;
	else
		m_userIPs[ip] = 1;
}

void CServerThread::DecIpCount(const CStdString &ip)
{
	simple_lock lock(m_global_mutex);
	std::map<CStdString, int>::iterator iter=m_userIPs.find(ip);
	ASSERT(iter != m_userIPs.end());
	if (iter != m_userIPs.end()) {
		ASSERT(iter->second > 0);
		if (iter->second > 1)
			--iter->second;
		else {
			m_userIPs.erase(iter);
		}
	}
}

void CServerThread::IncSendCount(int count)
{
	m_nSendCount += count;
}

void CServerThread::IncRecvCount(int count)
{
	m_nRecvCount += count;
}

void CServerThread::ProcessNewSlQuota()
{
	simple_lock lock(m_mutex);

	for (int i = 0; i < 2; ++i) {
		if (m_SlQuotas[i].nBytesAllowedToTransfer == -1) {
			for (auto & it : m_LocalUserIDs ) {
				CControlSocket *pControlSocket = it.second;
				pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = -1;
				pControlSocket->m_SlQuotas[i].nTransferred = 0;
			}
			continue;
		}

		long long nRemaining = m_SlQuotas[i].nBytesAllowedToTransfer;
		long long nThreadLimit = nRemaining / m_sInstanceList.size();

		std::vector<CControlSocket *> fullUsageList;

		for (auto & it : m_LocalUserIDs ) {
			CControlSocket *pControlSocket = it.second;
			long long r = pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer - pControlSocket->m_SlQuotas[i].nTransferred;
			if (pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer == -1) {
				pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
				pControlSocket->m_SlQuotas[i].nTransferred = 0;
			}
			else if (r > 0 && pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer <= nThreadLimit) {
				pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
				nRemaining -= pControlSocket->m_SlQuotas[i].nTransferred;
				pControlSocket->m_SlQuotas[i].nTransferred = 0;
			}
			else if (r > 0 && pControlSocket->m_SlQuotas[i].nTransferred < nThreadLimit) {
				pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
				nRemaining -= pControlSocket->m_SlQuotas[i].nTransferred;
				pControlSocket->m_SlQuotas[i].nTransferred = 0;
			}
			else {
				fullUsageList.push_back(pControlSocket);
				continue;
			}
		}

		if (!fullUsageList.empty()) {
			std::vector<CControlSocket *> fullUsageList2;

			nThreadLimit = nRemaining / fullUsageList.size();
			for (auto & pControlSocket : fullUsageList ) {
				long long r = pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer - pControlSocket->m_SlQuotas[i].nTransferred;
				if (r) {
					if (pControlSocket->m_SlQuotas[i].nTransferred > nThreadLimit)
						pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
					else
						pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = pControlSocket->m_SlQuotas[i].nTransferred;
					pControlSocket->m_SlQuotas[i].nTransferred = 0;
					nRemaining -= pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer;
				}
				else {
					fullUsageList2.push_back(pControlSocket);
					continue;
				}
			}

			if (!fullUsageList2.empty()) {
				nThreadLimit = nRemaining / fullUsageList2.size();
				for (auto & pControlSocket : fullUsageList2 ) {
					pControlSocket->m_SlQuotas[i].nTransferred = 0;
					pControlSocket->m_SlQuotas[i].nBytesAllowedToTransfer = nThreadLimit;
				}
			}
		}
	}

	for (auto & it : m_LocalUserIDs) {
		it.second->Continue();
	}
}

void CServerThread::GatherTransferedBytes()
{
	simple_lock lock(m_mutex);
	for (auto & it : m_LocalUserIDs) {
		for (int i = 0; i < 2; ++i) {
			if (it.second->m_SlQuotas[i].nBytesAllowedToTransfer > -1) {
				if (it.second->m_SlQuotas[i].bBypassed)
					it.second->m_SlQuotas[i].nTransferred = 0;
				else
					m_SlQuotas[i].nTransferred += it.second->m_SlQuotas[i].nTransferred;
			}
			it.second->m_SlQuotas[i].bBypassed = false;
		}
	}
}

CStdString CServerThread::GetExternalIP(const CStdString& localIP)
{
	{
		simple_lock lock(m_mutex);
		if (m_pExternalIpCheck) {
			return m_pExternalIpCheck->GetIP(localIP);
		}
	}
	
	simple_lock glock(m_global_mutex);
	CServerThread *pThread = m_sInstanceList.front();
	if (pThread && pThread != this) {
		simple_lock lock(pThread->m_mutex);
		if (pThread->m_pExternalIpCheck)
			return pThread->m_pExternalIpCheck->GetIP(localIP);
	}

	return CStdString();
}

void CServerThread::ExternalIPFailed()
{
	{
		simple_lock lock(m_mutex);
		if (m_pExternalIpCheck) {
			m_pExternalIpCheck->TriggerUpdate();
			return;
		}
	}
	
	simple_lock glock(m_global_mutex);
	CServerThread *pThread = m_sInstanceList.front();
	if (pThread && pThread != this) {
		simple_lock lock(pThread->m_mutex);
		if (pThread->m_pExternalIpCheck)
			pThread->m_pExternalIpCheck->TriggerUpdate();
	}
}

void CServerThread::SendNotification(WPARAM wParam, LPARAM lParam)
{
	simple_lock lock(m_mutex);
	t_Notification notification;
	notification.wParam = wParam;
	notification.lParam = lParam;

	if (m_pendingNotifications.empty())
		PostMessage(hMainWnd, m_nNotificationMessageId, 0, 0);

	m_pendingNotifications.push_back(notification);

	// Check if main thread can't handle number of notifications fast enough, throttle thread if neccessary
	if (m_pendingNotifications.size() > 200 && m_throttled < 3) {
		SetPriority(THREAD_PRIORITY_IDLE);
		m_throttled = 3;
	}
	else if (m_pendingNotifications.size() > 150 && m_throttled < 2) {
		SetPriority(THREAD_PRIORITY_LOWEST);
		m_throttled = 2;
	}
	else if (m_pendingNotifications.size() > 100 && !m_throttled) {
		SetPriority(THREAD_PRIORITY_BELOW_NORMAL);
		m_throttled = 1;
	}
}

void CServerThread::GetNotifications(std::list<CServerThread::t_Notification>& list)
{
	simple_lock lock(m_mutex);

	m_pendingNotifications.swap(list);

	if (m_throttled)
		SetPriority(THREAD_PRIORITY_NORMAL);
}

void CServerThread::AntiHammerIncrease(CStdString const& ip)
{
	simple_lock lock(m_global_mutex);

	std::map<CStdString, int>::iterator iter = m_antiHammerInfo.find(ip);
	if (iter != m_antiHammerInfo.end()) {
		if (iter->second < 20)
			iter->second++;
		return;
	}
	else {
		if (m_antiHammerInfo.size() >= 1000) {
			std::map<CStdString, int>::iterator best = m_antiHammerInfo.begin();
			for (iter = m_antiHammerInfo.begin(); iter != m_antiHammerInfo.end(); ++iter) {
				if (iter->second < best->second)
					best = iter;
			}
			m_antiHammerInfo.erase(best);
		}
		
		m_antiHammerInfo.insert(std::make_pair(ip, 1));
	}
}

void CServerThread::AntiHammerDecrease(CStdString const& ip)
{
	simple_lock lock(m_global_mutex);

	auto iter = m_antiHammerInfo.find(ip);
	if (iter != m_antiHammerInfo.end()) {
		if (iter->second > 1) {
			--(iter->second);
			++iter;
		}
		else
			m_antiHammerInfo.erase(iter++);
	}
}

void CServerThread::AntiHammerDecay()
{
	simple_lock lock(m_global_mutex);

	std::map<CStdString, int>::iterator iter = m_antiHammerInfo.begin();
	while (iter != m_antiHammerInfo.end()) {
		if (iter->second > 1) {
			--(iter->second);
			++iter;
		}
		else
			m_antiHammerInfo.erase(iter++);
	}
}

CHashThread& CServerThread::GetHashThread()
{
	return *m_hashThread;
}

void CServerThread::OnPermissionsUpdated()
{
	simple_lock lock(m_mutex);
	for( auto & cs : m_LocalUserIDs ) {
		if( cs.second ) {
			cs.second->UpdateUser();
		}
	}
}

long long CServerThread::GetInitialSpeedLimit(int mode)
{
	long long ret;
	simple_lock lock(m_mutex);
	ret = m_SlQuotas[mode].nBytesAllowedToTransfer;
	if( ret > -1 ) {
		ret /= m_LocalUserIDs.size() + 1;
	}
	return ret;
}