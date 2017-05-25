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

// ListenSocket.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "ListenSocket.h"
#include "ServerThread.h"
#include "Server.h"
#include "Options.h"
#include "iputils.h"
#include "autobanmanager.h"

CListenSocket::CListenSocket(CServer & server, std::vector<CServerThread*> & threadList, bool ssl)
	: m_server(server)
	, m_threadList(threadList)
	, m_ssl(ssl)
{
}

void CListenSocket::OnAccept(int nErrorCode)
{
	CAsyncSocketEx socket;
	if (!Accept(socket)) {
		int nError = WSAGetLastError();
		CStdString str;
		str.Format(_T("Failure in CListenSocket::OnAccept(%d) - call to CAsyncSocketEx::Accept failed, errorcode %d"), nErrorCode, nError);
		SendStatus(str, 1);
		SendStatus(_T("If you use a firewall, please check your firewall configuration"), 1);
		return;
	}

	if (!AccessAllowed(socket)) {
		CStdStringA str = "550 No connections allowed from your IP\r\n";
		(void)socket.Send(str, str.GetLength());
		return;
	}

	if (m_bLocked) {
		CStdStringA str = "421 Server is locked, please try again later.\r\n";
		(void)socket.Send(str, str.GetLength());
		return;
	}

	int minnum = 255*255*255;
	CServerThread *pBestThread = 0;
	for (auto const& pThread : m_threadList) {
		int num = pThread->GetNumConnections();
		if (num < minnum && pThread->IsReady()) {
			minnum = num;
			pBestThread = pThread;
			if (!num)
				break;
		}
	}

	if (!pBestThread) {
		char str[] = "421 Server offline.";
		(void)socket.Send(str, strlen(str) + 1);
		return;
	}

	/* Disable Nagle algorithm. Most of the time single short strings get
	 * transferred over the control connection. Waiting for additional data
	 * where there will be most likely none affects performance.
	 */
	socket.SetNodelay(true);

	SOCKET sockethandle = socket.Detach();

	pBestThread->AddSocket(sockethandle, m_ssl);
}

void CListenSocket::SendStatus(CStdString status, int type)
{
	m_server.ShowStatus(status, type);
}

bool CListenSocket::AccessAllowed(CAsyncSocketEx &socket) const
{
	CStdString peerIP;
	UINT port = 0;
	BOOL bResult = socket.GetPeerName(peerIP, port);
	if (!bResult)
		return true;

	if (m_server.m_pAutoBanManager) {
		if (m_server.m_pAutoBanManager->IsBanned((peerIP.Find(':') == -1) ? peerIP : GetIPV6ShortForm(peerIP))) {
			return false;
		}
	}

	bool disallowed = false;

	// Get the list of IP filter rules.
	CStdString ips = m_server.m_pOptions->GetOption(OPTION_IPFILTER_DISALLOWED);
	ips += _T(" ");

	int pos = ips.Find(' ');
	while (pos != -1)
	{
		CStdString blockedIP = ips.Left(pos);
		ips = ips.Mid(pos + 1);
		pos = ips.Find(' ');

		if ((disallowed = MatchesFilter(blockedIP, peerIP)))
			break;
	}

	if (!disallowed)
		return true;

	ips = m_server.m_pOptions->GetOption(OPTION_IPFILTER_ALLOWED);
	ips += _T(" ");

	pos = ips.Find(' ');
	while (pos != -1)
	{
		CStdString blockedIP = ips.Left(pos);
		ips = ips.Mid(pos + 1);
		pos = ips.Find(' ');

		if (MatchesFilter(blockedIP, peerIP))
			return true;
	}

	return false;
}
