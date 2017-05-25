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

#include "stdafx.h"
#include "AdminSocket.h"
#include "MainFrm.h"
#include "../iputils.h"
#include "../OptionTypes.h"
#include "../platform.h"
#include "../misc\md5.h"

#define BUFSIZE 4096

CAdminSocket::CAdminSocket(CMainFrame *pMainFrame)
{
	ASSERT(pMainFrame);
	m_pMainFrame = pMainFrame;
	m_pRecvBuffer = new unsigned char[BUFSIZE];
	m_nRecvBufferLen = BUFSIZE;
}

CAdminSocket::~CAdminSocket()
{
	Close();

	delete [] m_pRecvBuffer;
}

void CAdminSocket::OnConnect(int nErrorCode)
{
	if (!nErrorCode) {
		if (!m_nConnectionState) {
			m_pMainFrame->ShowStatus(_T("Connected, waiting for authentication"), 0);
			m_nConnectionState = 1;
		}
		m_pMainFrame->OnAdminInterfaceConnected();
	}
	else {
		m_pMainFrame->ShowStatus(_T("Error, could not connect to server"), 1);
		Close();
	}
}

void CAdminSocket::OnReceive(int nErrorCode)
{
	if (nErrorCode) {
		m_pMainFrame->ShowStatus(_T("OnReceive failed, closing connection"), 1);
		Close();
		return;
	}

	if (!m_nConnectionState) {
		m_pMainFrame->ShowStatus(_T("Connected, waiting for authentication"), 0);
		m_nConnectionState = 1;
	}

	int numread = Receive(m_pRecvBuffer + m_nRecvBufferPos, m_nRecvBufferLen - m_nRecvBufferPos);
	if (numread > 0) {
		m_nRecvBufferPos += numread;
		if (m_nRecvBufferLen-m_nRecvBufferPos < (BUFSIZE/4)) {
			unsigned char *tmp = m_pRecvBuffer;
			m_nRecvBufferLen += BUFSIZE;
			m_pRecvBuffer = new unsigned char[m_nRecvBufferLen];
			memcpy(m_pRecvBuffer, tmp, m_nRecvBufferPos);
			delete [] tmp;
		}
	}
	if (!numread) {
		Close();
		return;
	}
	else if (numread == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			Close();
			return;
		}
	}
	while (ParseRecvBuffer());
}

void CAdminSocket::OnSend(int nErrorCode)
{
	if (nErrorCode) {
		Close();
		return;
	}
	if (!m_nConnectionState)
		return;

	if (!SendPendingData()) {
		Close();
	}
}

void CAdminSocket::Close()
{
	if (m_nConnectionState) {
		m_pMainFrame->ShowStatus(_T("Connection to server closed."), 1);
	}
	m_nConnectionState = 0;
	if (!m_bClosed) {
		m_bClosed = TRUE;
		m_pMainFrame->PostMessage(WM_APP + 1, 0, 0);
	}
}

BOOL CAdminSocket::ParseRecvBuffer()
{
	DWORD len;
	switch (m_nConnectionState)
	{
	case 1:
		{
			if (m_nRecvBufferPos < 3) {
				return FALSE;
			}
			if (m_pRecvBuffer[0] != 'F' || m_pRecvBuffer[1] != 'Z' || m_pRecvBuffer[2] != 'S') {
				CString str;
				str.Format(_T("Protocol error: Unknown protocol identifier (0x%d 0x%d 0x%d). Most likely connected to the wrong port."), (int)m_pRecvBuffer[0], (int)m_pRecvBuffer[1], (int)m_pRecvBuffer[2]);
				m_pMainFrame->ShowStatus(str, 1);
				Close();
				return FALSE;
			}
			if (m_nRecvBufferPos < 5) {
				return FALSE;
			}
			len = m_pRecvBuffer[3] * 256 + m_pRecvBuffer[4];
			if (len != 4) {
				CString str;
				str.Format(_T("Protocol error: Invalid server version length (%lu)."), len);
				m_pMainFrame->ShowStatus(str, 1);
				Close();
				return FALSE;
			}
			if (m_nRecvBufferPos < 9) {
				return FALSE;
			}

			int version = (int)GET32(m_pRecvBuffer + 5);
			if (version != SERVER_VERSION) {
				CString str;
				str.Format(_T("Protocol warning: Server version mismatch: Server version is %d.%d.%d.%d, interface version is %d.%d.%d.%d"),
						   (version >> 24) & 0xFF,
						   (version >> 16) & 0xFF,
						   (version >>  8) & 0xFF,
						   (version >>  0) & 0xFF,
						   (SERVER_VERSION >> 24) & 0xFF,
						   (SERVER_VERSION >> 16) & 0xFF,
						   (SERVER_VERSION >>  8) & 0xFF,
						   (SERVER_VERSION >>  0) & 0xFF);
				m_pMainFrame->ShowStatus(str, 1);
			}

			if (m_nRecvBufferPos < 11) {
				return FALSE;
			}
			len = m_pRecvBuffer[9] * 256 + m_pRecvBuffer[10];
			if (len != 4) {
				CString str;
				str.Format(_T("Protocol error: Invalid protocol version length (%lu)."), len);
				m_pMainFrame->ShowStatus(str, 1);
				Close();
				return FALSE;
			}
			if (m_nRecvBufferPos < 15) {
				return FALSE;
			}
			version = (int)GET32(m_pRecvBuffer + 11);
			if (version != PROTOCOL_VERSION) {
				CString str;
				str.Format(_T("Protocol error: Protocol version mismatch: Server protocol version is %d.%d.%d.%d, interface protocol version is %d.%d.%d.%d"),
						   (version >> 24) & 0xFF,
						   (version >> 16) & 0xFF,
						   (version >>  8) & 0xFF,
						   (version >>  0) & 0xFF,
						   (PROTOCOL_VERSION >> 24) & 0xFF,
						   (PROTOCOL_VERSION >> 16) & 0xFF,
						   (PROTOCOL_VERSION >>  8) & 0xFF,
						   (PROTOCOL_VERSION >>  0) & 0xFF);
				m_pMainFrame->ShowStatus(str, 1);
				Close();
				return FALSE;
			}

			memmove(m_pRecvBuffer, m_pRecvBuffer + 15, m_nRecvBufferPos - 15);
			m_nRecvBufferPos -= 15;
			m_nConnectionState = 2;
		}
		break;
	case 2:
		if (m_nRecvBufferPos < 5) {
			return FALSE;
		}
		if ((m_pRecvBuffer[0]&0x03) > 2) {
			CString str;
			str.Format(_T("Protocol error: Unknown command type (%d), closing connection."), (int)(m_pRecvBuffer[0]&0x03));
			m_pMainFrame->ShowStatus(str, 1);
			Close();
			return FALSE;
		}
		len = (int)GET32(m_pRecvBuffer + 1);
		if (len + 5 <= m_nRecvBufferPos) {
			if ((m_pRecvBuffer[0] & 0x03) == 0 && (m_pRecvBuffer[0] & 0x7C) >> 2 == 0) {
				if (len < 4) {
					m_pMainFrame->ShowStatus(_T("Invalid auth data"), 1);
					Close();
					return FALSE;
				}
				unsigned char *p = m_pRecvBuffer + 5;

				unsigned int noncelen1 = *p*256 + p[1];
				if ((noncelen1+2) > (len-2)) {
					m_pMainFrame->ShowStatus(_T("Invalid auth data"), 1);
					Close();
					return FALSE;
				}

				unsigned int noncelen2 = p[2 + noncelen1] * 256 + p[2 + noncelen1 + 1];
				if ((noncelen1+noncelen2+4) > len) {
					m_pMainFrame->ShowStatus(_T("Invalid auth data"), 1);
					Close();
					return FALSE;
				}

				MD5 md5;
				if (noncelen1) {
					md5.update(p + 2, noncelen1);
				}
				auto utf8 = ConvToNetwork(m_Password);
				if (utf8.empty() && !m_Password.IsEmpty()) {
					m_pMainFrame->ShowStatus(_T("Can't convert password to UTF-8"), 1);
					Close();
					return FALSE;
				}
				md5.update((const unsigned char *)utf8.c_str(), utf8.size());
				if (noncelen2) {
					md5.update(p + noncelen1 + 4, noncelen2);
				}
				md5.finalize();

				memmove(m_pRecvBuffer, m_pRecvBuffer + len + 5, m_nRecvBufferPos - len - 5);
				m_nRecvBufferPos -= len + 5;

				unsigned char *digest = md5.raw_digest();
				SendCommand(0, digest, 16);
				delete [] digest;
				m_nConnectionState = 3;
				return TRUE;
			}
			else if ((m_pRecvBuffer[0] & 0x03) == 1 && (m_pRecvBuffer[0] & 0x7C) >> 2 == 0) {
				m_nConnectionState=3;
				m_pMainFrame->ParseReply((m_pRecvBuffer[0] & 0x7C) >> 2, m_pRecvBuffer + 5, len);
			}
			else {
				CString str;
				str.Format(_T("Protocol error: Unknown command ID (%d), closing connection."), (int)(m_pRecvBuffer[0]&0x7C)>>2);
				m_pMainFrame->ShowStatus(str, 1);
				Close();
				return FALSE;
			}
			memmove(m_pRecvBuffer, m_pRecvBuffer + len + 5, m_nRecvBufferPos - len - 5);
			m_nRecvBufferPos -= len + 5;
		}
		break;
	case 3:
		if (m_nRecvBufferPos < 5) {
			return FALSE;
		}
		int nType = *m_pRecvBuffer & 0x03;
		int nID = (*m_pRecvBuffer & 0x7C) >> 2;
		if (nType > 2 || nType < 1) {
			CString str;
			str.Format(_T("Protocol error: Unknown command type (%d), closing connection."), nType);
			m_pMainFrame->ShowStatus(str, 1);
			Close();
			return FALSE;
		}
		else {
			len = (unsigned int)GET32(m_pRecvBuffer + 1);
			if (len > 0xFFFFFF) {
				CString str;
				str.Format(_T("Protocol error: Invalid data length (%lu) for command (%d:%d)"), len, nType, nID);
				m_pMainFrame->ShowStatus(str, 1);
				Close();
				return FALSE;
			}
			if (m_nRecvBufferPos < len + 5) {
				return FALSE;
			}
			else {
				if (nType == 1) {
					m_pMainFrame->ParseReply(nID, m_pRecvBuffer + 5, len);
				}
				else if (nType == 2) {
					m_pMainFrame->ParseStatus(nID, m_pRecvBuffer + 5, len);
				}
				else {
					CString str;
					str.Format(_T("Protocol warning: Command type %d not implemented."), nType);
					m_pMainFrame->ShowStatus(str, 1);
				}

				memmove(m_pRecvBuffer, m_pRecvBuffer + len + 5, m_nRecvBufferPos - len - 5);
				m_nRecvBufferPos -= len + 5;
			}
		}
		break;
	}
	return TRUE;
}

BOOL CAdminSocket::SendCommand(int nType)
{
	t_data data(5);
	*data.pData = nType << 2;
	DWORD dwDataLength = 0;
	memcpy(&*data.pData + 1, &dwDataLength, 4);
	m_SendBuffer.push_back(data);

	bool res = SendPendingData();
	if (!res) {
		Close();
	}

	return res;
}

BOOL CAdminSocket::SendCommand(int nType, void *pData, int nDataLength)
{
	ASSERT((pData && nDataLength) || (!pData && !nDataLength));

	t_data data(nDataLength + 5);
	*data.pData = nType << 2;
	memcpy(&*data.pData + 1, &nDataLength, 4);
	if (pData) {
		memcpy(&*data.pData + 5, pData, nDataLength);
	}

	m_SendBuffer.push_back(data);

	bool res = SendPendingData();
	if (!res) {
		Close();
	}

	return res;
}

bool CAdminSocket::SendPendingData()
{
	for (auto it = m_SendBuffer.begin(); it != m_SendBuffer.end(); it = m_SendBuffer.erase(it)) {
		auto& data = *it;
		int nSent = Send(&*data.pData + data.dwOffset, data.dwLength - data.dwOffset);
		if (!nSent) {
			return false;
		}
		if (nSent == SOCKET_ERROR) {
			return WSAGetLastError() == WSAEWOULDBLOCK;
		}

		if ((unsigned int)nSent < (data.dwLength - data.dwOffset)) {
			data.dwOffset += nSent;
			break;
		}
	}

	return true;
}

BOOL CAdminSocket::IsConnected()
{
	return m_nConnectionState == 3;
}

void CAdminSocket::OnClose(int nErrorCode)
{
	Close();
}

void CAdminSocket::DoClose()
{
	m_bClosed = true;
	Close();
}

bool CAdminSocket::IsLocal()
{
	CString ip;
	UINT port;
	if (!GetPeerName(ip, port)) {
		return false;
	}

	return IsLocalhost(ip);
}
