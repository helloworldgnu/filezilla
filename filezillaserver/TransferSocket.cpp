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

// TransferSocket.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "TransferSocket.h"
#include "ControlSocket.h"
#include "options.h"
#include "ServerThread.h"
#include "AsyncSslSocketLayer.h"
#include "Permissions.h"
#include "iputils.h"

/////////////////////////////////////////////////////////////////////////////
// CTransferSocket
CTransferSocket::CTransferSocket(CControlSocket *pOwner)
: m_pSslLayer()
{
	ASSERT(pOwner);
	m_pOwner = pOwner;
	m_nMode = TRANSFERMODE_NOTSET;

	m_nBufferPos = NULL;
	m_pBuffer = NULL;
	m_pDirListing = NULL;
	m_bAccepted = FALSE;

	GetSystemTime(&m_LastActiveTime);
	m_wasActiveSinceCheck = false;
	m_nRest = 0;

	m_hFile = INVALID_HANDLE_VALUE;

	m_nBufSize = (int)m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_BUFFERSIZE);

	m_useZlib = false;
	memset(&m_zlibStream, 0, sizeof(m_zlibStream));

	m_zlibBytesIn = 0;
	m_zlibBytesOut = 0;

	m_pBuffer2 = 0;

	m_currentFileOffset = 0;

	m_waitingForSslHandshake = false;

	m_premature_send = false;

	m_on_connect_called = false;
}

void CTransferSocket::Init(std::list<t_dirlisting> &dir, int nMode)
{
	ASSERT(nMode == TRANSFERMODE_LIST);
	m_bReady = true;
	m_status = transfer_status_t::success;
	if (m_pBuffer)
		delete [] m_pBuffer;
	m_pBuffer = 0;
	if (m_pBuffer2)
		delete [] m_pBuffer2;
	m_pBuffer2 = 0;

	std::swap( directory_listing_, dir );

	m_nMode = nMode;

	if (m_hFile != INVALID_HANDLE_VALUE)
		CloseHandle(m_hFile);
	m_nBufferPos = 0;
}

void CTransferSocket::Init(const CStdString& filename, int nMode, _int64 rest)
{
	ASSERT(nMode == TRANSFERMODE_SEND || nMode == TRANSFERMODE_RECEIVE);
	m_bReady = true;
	m_Filename = filename;
	m_nRest = rest;
	m_nMode = nMode;

	if (m_pBuffer)
		delete [] m_pBuffer;
	m_pBuffer = 0;
	if (m_pBuffer2)
		delete [] m_pBuffer2;
	m_pBuffer2 = 0;
}

CTransferSocket::~CTransferSocket()
{
	delete [] m_pBuffer;
	delete [] m_pBuffer2;
	CloseFile();

	RemoveAllLayers();
	delete m_pSslLayer;

	if (m_useZlib) {
		if (m_nMode == TRANSFERMODE_RECEIVE)
			inflateEnd(&m_zlibStream);
		else
			deflateEnd(&m_zlibStream);
	}
}


//Die folgenden Zeilen nicht bearbeiten. Sie werden vom Klassen-Assistenten benötigt.
#if 0
BEGIN_MESSAGE_MAP(CTransferSocket, CAsyncSocketEx)
	//{{AFX_MSG_MAP(CTransferSocket)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#endif	// 0

/////////////////////////////////////////////////////////////////////////////
// Member-Funktion CTransferSocket

void CTransferSocket::OnSend(int nErrorCode)
{
	if (nErrorCode) {
		EndTransfer(transfer_status_t::closed_aborted);
		return;
	}

	if (m_nMode == TRANSFERMODE_LIST)
	{ //Send directory listing
		if (!m_bStarted)
			if (!InitTransfer(TRUE))
				return;

		if (m_useZlib)
		{
			if (!m_pBuffer)
			{
				m_pBuffer = new char[m_nBufSize];
				m_nBufferPos = 0;

				m_zlibStream.next_in = (Bytef *)m_pBuffer; // Make sure next_in is not 0 in all cases

				m_zlibStream.next_out = (Bytef *)m_pBuffer;
				m_zlibStream.avail_out = m_nBufSize;
			}

			while (true) {
				int numsend;
				if (!m_zlibStream.avail_in)
				{
					if (!directory_listing_.empty()) {
						m_zlibStream.next_in = (Bytef *)directory_listing_.front().buffer;
						m_zlibStream.avail_in = directory_listing_.front().len;
					}
				}
				if (!m_zlibStream.avail_out)
				{
					if (m_nBufferPos >= m_nBufSize)
					{
						m_nBufferPos = 0;
						m_zlibStream.next_out = (Bytef *)m_pBuffer;
						m_zlibStream.avail_out = m_nBufSize;
					}
				}

				int res = Z_OK;
				if (m_zlibStream.avail_out) {
					m_zlibStream.total_in = 0;
					m_zlibStream.total_out = 0;
					res = deflate(&m_zlibStream, directory_listing_.size() > 1 ? 0 : Z_FINISH);
					m_currentFileOffset += m_zlibStream.total_in;
					m_zlibBytesIn += m_zlibStream.total_in;
					m_zlibBytesOut += m_zlibStream.total_out;
					if (res == Z_STREAM_END)
					{
						if (directory_listing_.size() > 1 ) {
							ShutDown();
							EndTransfer(transfer_status_t::zlib);
							return;
						}
						if (!(m_nBufSize - m_nBufferPos - m_zlibStream.avail_out))
							break;
					}
					else if (res != Z_OK)
					{
						ShutDown();
						EndTransfer(transfer_status_t::zlib);
						return;
					}
					if (!m_zlibStream.avail_in && !directory_listing_.empty()) {
						directory_listing_.pop_front();
					}
				}

				numsend = m_nBufSize;
				unsigned int len = m_nBufSize - m_nBufferPos - m_zlibStream.avail_out;
				if (!len)
					continue;

				if (len < m_nBufSize)
					numsend = len;

				long long nLimit = m_pOwner->GetSpeedLimit(download);
				if (nLimit > -1 && GetState() != aborted && numsend > nLimit)
					numsend = static_cast<int>(nLimit);

				if (!numsend)
					return;

				int numsent = Send(m_pBuffer + m_nBufferPos, numsend);
				if (numsent == SOCKET_ERROR) {
					if (GetLastError() != WSAEWOULDBLOCK)
						EndTransfer(transfer_status_t::closed_aborted);
					return;
				}

				if (nLimit > -1 && GetState() != aborted)
					m_pOwner->m_SlQuotas[download].nTransferred += numsent;

				m_pOwner->m_owner.IncSendCount(numsent);
				m_wasActiveSinceCheck = true;
				m_nBufferPos += numsent;

				if (!m_zlibStream.avail_in && !m_pDirListing && m_zlibStream.avail_out &&
					m_zlibStream.avail_out + m_nBufferPos == m_nBufSize && res == Z_STREAM_END)
				{
					break;
				}

				//Check if there are other commands in the command queue.
				MSG msg;
				if (PeekMessage(&msg,0, 0, 0, PM_NOREMOVE)) {
					TriggerEvent(FD_WRITE);
					return;
				}
			}
		}
		else {
			while (!directory_listing_.empty()) {
				int numsend = m_nBufSize;
				if ((directory_listing_.front().len - m_nBufferPos) < m_nBufSize)
					numsend = directory_listing_.front().len - m_nBufferPos;

				long long nLimit = m_pOwner->GetSpeedLimit(download);
				if (nLimit > -1 && GetState() != aborted && numsend > nLimit)
					numsend = static_cast<int>(nLimit);

				if (!numsend)
					return;

				int numsent = Send(directory_listing_.front().buffer + m_nBufferPos, numsend);
				if (numsent == SOCKET_ERROR) {
					int error = GetLastError();
					if (error != WSAEWOULDBLOCK)
						EndTransfer(transfer_status_t::closed_aborted);
					return;
				}

				if (nLimit > -1 && GetState() != aborted)
					m_pOwner->m_SlQuotas[download].nTransferred += numsent;

				m_pOwner->m_owner.IncSendCount(numsent);
				m_wasActiveSinceCheck = true;
				if (numsent < numsend)
					m_nBufferPos += numsent;
				else
					m_nBufferPos += numsend;

				m_currentFileOffset += numsent;

				ASSERT(m_nBufferPos <= directory_listing_.front().len);
				if (m_nBufferPos >= directory_listing_.front().len) {
					directory_listing_.pop_front();
					m_nBufferPos = 0;

					if( directory_listing_.empty() ) {
						break;
					}
				}

				//Check if there are other commands in the command queue.
				MSG msg;
				if (PeekMessage(&msg,0, 0, 0, PM_NOREMOVE)) {
					TriggerEvent(FD_WRITE);
					return;
				}
			}
		}

		if (m_waitingForSslHandshake) {
			// Don't yet issue a shutdown
			return;
		}

		if (m_pSslLayer) {
			if (!ShutDown() && GetLastError() == WSAEWOULDBLOCK)
				return;
		}
		else
			ShutDown();
		EndTransfer(transfer_status_t::success);
	}
	else if (m_nMode == TRANSFERMODE_SEND)
	{ //Send file
		if (!m_bStarted)
			if (!InitTransfer(TRUE))
				return;
		if (m_useZlib) {
			if (!m_pBuffer2) {
				m_pBuffer2 = new char[m_nBufSize];

				m_zlibStream.next_in = (Bytef *)m_pBuffer2;
			}

			while (true) {
				int numsend;
				if (!m_zlibStream.avail_in) {
					if (m_hFile != INVALID_HANDLE_VALUE) {
						DWORD numread;
						if (!ReadFile(m_hFile, m_pBuffer2, m_nBufSize, &numread, 0)) {
							EndTransfer(transfer_status_t::noaccess); // TODO: Better reason
							return;
						}
						m_currentFileOffset += numread;

						m_zlibStream.next_in = (Bytef *)m_pBuffer2;
						m_zlibStream.avail_in = numread;

						if (numread < m_nBufSize)
						{
							CloseFile();

							if (m_waitingForSslHandshake)
								return;
						}
					}
				}
				if (!m_zlibStream.avail_out)
				{
					if (m_nBufferPos >= m_nBufSize)
					{
						m_nBufferPos = 0;
						m_zlibStream.next_out = (Bytef *)m_pBuffer;
						m_zlibStream.avail_out = m_nBufSize;
					}
				}

				int res = Z_OK;
				if (m_zlibStream.avail_out)
				{
					m_zlibStream.total_in = 0;
					m_zlibStream.total_out = 0;
					res = deflate(&m_zlibStream, (m_hFile != INVALID_HANDLE_VALUE) ? 0 : Z_FINISH);
					m_zlibBytesIn += m_zlibStream.total_in;
					m_zlibBytesOut += m_zlibStream.total_out;
					if (res == Z_STREAM_END)
					{
						if (m_hFile != INVALID_HANDLE_VALUE)
						{
							EndTransfer(transfer_status_t::zlib);
							return;
						}
						if (!(m_nBufSize - m_nBufferPos - m_zlibStream.avail_out))
							break;
					}
					else if (res != Z_OK)
					{
						EndTransfer(transfer_status_t::zlib);
						return;
					}
				}

				numsend = m_nBufSize;
				unsigned int len = m_nBufSize - m_nBufferPos - m_zlibStream.avail_out;
				if (!len)
					continue;

				if (len < m_nBufSize)
					numsend = len;

				long long nLimit = m_pOwner->GetSpeedLimit(download);
				if (nLimit > -1 && GetState() != aborted && numsend > nLimit)
					numsend = static_cast<int>(nLimit);

				if (!numsend)
					return;

				int numsent = Send(m_pBuffer + m_nBufferPos, numsend);
				if (numsent == SOCKET_ERROR) {
					if (GetLastError() != WSAEWOULDBLOCK)
						EndTransfer(transfer_status_t::closed_aborted);
					return;
				}

				if (nLimit > -1 && GetState() != aborted)
					m_pOwner->m_SlQuotas[download].nTransferred += numsent;

				m_pOwner->m_owner.IncSendCount(numsent);
				m_wasActiveSinceCheck = true;
				m_nBufferPos += numsent;

				if (!m_zlibStream.avail_in && m_hFile == INVALID_HANDLE_VALUE && m_zlibStream.avail_out &&
					m_zlibStream.avail_out + m_nBufferPos == m_nBufSize && res == Z_STREAM_END)
				{
					break;
				}

				//Check if there are other commands in the command queue.
				MSG msg;
				if (PeekMessage(&msg,0, 0, 0, PM_NOREMOVE))
				{
					TriggerEvent(FD_WRITE);
					return;
				}
			}
		}
		else {
			while (m_hFile != INVALID_HANDLE_VALUE || m_nBufferPos) {
				DWORD numread;
				if (m_nBufSize - m_nBufferPos && m_hFile != INVALID_HANDLE_VALUE) {
					if (!ReadFile(m_hFile, m_pBuffer+m_nBufferPos, m_nBufSize-m_nBufferPos, &numread, 0)) {
						EndTransfer(transfer_status_t::noaccess); //TODO: Better reason
						return;
					}

					if (!numread)
					{
						CloseFile();

						if (!m_nBufferPos)
						{
							if (m_waitingForSslHandshake)
								return;

							if (m_pSslLayer)
								if (!ShutDown() && GetLastError() == WSAEWOULDBLOCK)
									return;
							EndTransfer(transfer_status_t::success);
							return;
						}
					}
					else
						m_currentFileOffset += numread;

					numread += m_nBufferPos;
					m_nBufferPos = 0;
				}
				else
					numread = m_nBufferPos;
				m_nBufferPos = 0;

				if (numread < m_nBufSize) {
					CloseFile();
				}

				int numsend = numread;
				long long nLimit = m_pOwner->GetSpeedLimit(download);
				if (nLimit > -1 && GetState() != aborted && numsend > nLimit)
					numsend = static_cast<int>(nLimit);

				if (!numsend) {
					m_nBufferPos = numread;
					return;
				}

				int	numsent = Send(m_pBuffer, numsend);
				if (numsent == SOCKET_ERROR) {
					if (GetLastError() != WSAEWOULDBLOCK) {
						EndTransfer(transfer_status_t::closed_aborted);
						return;
					}
					m_nBufferPos=numread;
					return;
				}
				else if ((unsigned int)numsent < numread) {
					memmove(m_pBuffer, m_pBuffer + numsent, numread - numsent);
					m_nBufferPos=numread-numsent;
				}

				if (nLimit > -1 && GetState() != aborted)
					m_pOwner->m_SlQuotas[download].nTransferred += numsent;

				m_pOwner->m_owner.IncSendCount(numsent);
				m_wasActiveSinceCheck = true;

				//Check if there are other commands in the command queue.
				MSG msg;
				if (PeekMessage(&msg,0, 0, 0, PM_NOREMOVE))
				{
					TriggerEvent(FD_WRITE);
					return;
				}
			}
		}

		if (m_waitingForSslHandshake)
		{
			// Don't yet issue a shutdown
			return;
		}

		if (m_pSslLayer)
		{
			if (!ShutDown() && GetLastError() == WSAEWOULDBLOCK)
				return;
		}
		else
			ShutDown();
		Sleep(0); //Give the system the possibility to relay the data
				  //If not using Sleep(0), GetRight for example can't receive the last chunk.
		EndTransfer(transfer_status_t::success);
	}
	else if (m_nMode == TRANSFERMODE_NOTSET)
	{
		m_premature_send = true;
	}
}

void CTransferSocket::OnConnect(int nErrorCode)
{
	if (nErrorCode) {
		EndTransfer(transfer_status_t::noconn);
		return;
	}

	int size = (int)m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_BUFFERSIZE2);
	if (size > 0) {
		if (m_nMode == TRANSFERMODE_RECEIVE) {
			SetSockOpt(SO_RCVBUF, &size, sizeof(int));
		}
	}

	if (m_use_tls) {
		// Disable Nagle algorithm for duration of the handshake.
		SetNodelay(true);
		if (!m_pSslLayer) {
			m_pSslLayer = new CAsyncSslSocketLayer(m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_TLS_MINVERSION));
		}
		VERIFY(AddLayer(m_pSslLayer));

		int code = m_pSslLayer->InitSSLConnection(false, m_pOwner->GetSslLayer(), m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_TLS_REQUIRE_SESSION_RESUMPTION) != 0);
		if (code == SSL_FAILURE_LOADDLLS) {
			m_pOwner->SendStatus(_T("Failed to load TLS libraries"), 1);
		}
		else if (code == SSL_FAILURE_INITSSL) {
			m_pOwner->SendStatus(_T("Failed to initialize TLS library"), 1);
		}

		if (code) {
			EndTransfer(transfer_status_t::noconn);
			return;
		}
		m_waitingForSslHandshake = true;
	}

	m_on_connect_called = true;

	if (!m_bStarted) {
		InitTransfer(FALSE);
	}
}

void CTransferSocket::OnClose(int nErrorCode)
{
	if (nErrorCode)
	{
		EndTransfer(transfer_status_t::closed_aborted);
		return;
	}
	if (m_bReady)
	{
		if (m_nMode==TRANSFERMODE_RECEIVE)
		{
			//Receive all data still waiting to be recieve
			_int64 pos=0;
			do
			{
				if (m_hFile != INVALID_HANDLE_VALUE)
					pos=GetPosition64(m_hFile);
				OnReceive(0);
				if (m_hFile != INVALID_HANDLE_VALUE)
					if (pos == GetPosition64(m_hFile))
						break; //Leave loop when no data was written to file
			} while (m_hFile != INVALID_HANDLE_VALUE); //Or file was closed
			EndTransfer(transfer_status_t::success);
		}
		else
			EndTransfer((m_nMode == TRANSFERMODE_RECEIVE) ? transfer_status_t::success : transfer_status_t::closed_aborted);
	}
}

transfer_status_t CTransferSocket::GetStatus() const
{
	return m_status;
}

void CTransferSocket::OnAccept(int nErrorCode)
{
	CAsyncSocketEx tmp;
	Accept(tmp);
	SOCKET socket = tmp.Detach();
	Close();
	Attach(socket);
	m_bAccepted = TRUE;

	portLease_.SetConnected();

	int size = (int)m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_BUFFERSIZE2);
	if (size > 0) {
		if (m_nMode == TRANSFERMODE_RECEIVE)
			SetSockOpt(SO_RCVBUF, &size, sizeof(int));
		else
			SetSockOpt(SO_SNDBUF, &size, sizeof(int));
	}

	if (m_use_tls) {
		// Disable Nagle algorithm for duration of the handshake.
		SetNodelay(true);

		if (!m_pSslLayer)
			m_pSslLayer = new CAsyncSslSocketLayer(m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_TLS_MINVERSION));
		VERIFY(AddLayer(m_pSslLayer));

		int code = m_pSslLayer->InitSSLConnection(false, m_pOwner->GetSslLayer(), m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_TLS_REQUIRE_SESSION_RESUMPTION) != 0);
		if (code == SSL_FAILURE_LOADDLLS)
			m_pOwner->SendStatus(_T("Failed to load TLS libraries"), 1);
		else if (code == SSL_FAILURE_INITSSL)
			m_pOwner->SendStatus(_T("Failed to initialize TLS library"), 1);

		if (code) {
			EndTransfer(transfer_status_t::noconn);
			return;
		}
		m_waitingForSslHandshake = true;
	}

	if (m_bReady)
		if (!m_bStarted)
			InitTransfer(FALSE);
}

void CTransferSocket::OnReceive(int nErrorCode)
{
	bool obeySpeedLimit = true;
	if (nErrorCode == WSAESHUTDOWN)
		obeySpeedLimit = false;
	else if (nErrorCode)
	{
		EndTransfer(transfer_status_t::noaccess);
		return;
	}
	else if (GetState() == closed)
		obeySpeedLimit = false;

	if (m_nMode == TRANSFERMODE_RECEIVE)
	{
		if (!m_bStarted)
			if (!InitTransfer(FALSE))
				return;

		m_wasActiveSinceCheck = true;

		int len = m_nBufSize;
		long long nLimit = -1;
		if (obeySpeedLimit) {
			nLimit = m_pOwner->GetSpeedLimit(upload);
			if (nLimit != -1 && GetState() != aborted && len > nLimit)
				len = static_cast<int>(nLimit);
		}

		if (!len)
			return;

		int numread = Receive(m_pBuffer, len);

		if (numread == SOCKET_ERROR) {
			const int error = GetLastError();
			if (m_pSslLayer && error == WSAESHUTDOWN)
			{
				// Don't do anything at this point, we should get OnClose soon
				return;
			}
			else if (error != WSAEWOULDBLOCK)
			{
				EndTransfer(transfer_status_t::closed_aborted);
			}
			return;
		}
		if (!numread)
		{
			EndTransfer(transfer_status_t::success);
			return;
		}
		m_pOwner->m_owner.IncRecvCount(numread);

		if (nLimit != -1 && GetState() != aborted)
			m_pOwner->m_SlQuotas[upload].nTransferred += numread;

		if (m_useZlib)
		{
			if (!m_pBuffer2)
				m_pBuffer2 = new char[m_nBufSize];

			m_zlibStream.next_in = (Bytef *)m_pBuffer;
			m_zlibStream.avail_in = numread;
			m_zlibStream.next_out = (Bytef *)m_pBuffer2;
			m_zlibStream.avail_out = m_nBufSize;

			m_zlibStream.total_in = 0;
			m_zlibStream.total_out = 0;
			int res = inflate(&m_zlibStream, 0);
			m_zlibBytesIn += m_zlibStream.total_in;
			m_zlibBytesOut += m_zlibStream.total_out;

			while (res == Z_OK)
			{
				DWORD numwritten;
				if (!WriteFile(m_hFile, m_pBuffer2, m_nBufSize - m_zlibStream.avail_out, &numwritten, 0) || numwritten != m_nBufSize - m_zlibStream.avail_out)
				{
					EndTransfer(transfer_status_t::noaccess); // TODO: Better reason
					return;
				}
				m_currentFileOffset += numwritten;

				m_zlibStream.next_out = (Bytef *)m_pBuffer2;
				m_zlibStream.avail_out = m_nBufSize;
				res = inflate(&m_zlibStream, 0);
			}
			if (res == Z_STREAM_END)
			{
				DWORD numwritten;
				if (!WriteFile(m_hFile, m_pBuffer2, m_nBufSize - m_zlibStream.avail_out, &numwritten, 0) || numwritten != m_nBufSize - m_zlibStream.avail_out)
				{
					EndTransfer(transfer_status_t::noaccess); // TODO: Better reason
					return;
				}
				m_currentFileOffset += numwritten;
			}
			else if (res != Z_OK && res != Z_BUF_ERROR)
			{
				EndTransfer(transfer_status_t::zlib);
				return;
			}
		}
		else
		{
			DWORD numwritten;
			if (!WriteFile(m_hFile, m_pBuffer, numread, &numwritten, 0) || numwritten!=(unsigned int)numread)
			{
				EndTransfer(transfer_status_t::noaccess); //TODO: Better reason
				return;
			}
			m_currentFileOffset += numwritten;
		}
	}
}

void CTransferSocket::PasvTransfer()
{
	if (m_bAccepted)
		if (!m_bStarted)
			InitTransfer(FALSE);
	if (m_premature_send) {
		m_premature_send = false;
		OnSend(0);
	}
}

bool CTransferSocket::IsAllowedDataConnectionIP(CStdString controlIP, CStdString dataIP, int family, COptions & options)
{
	auto const checkDataIP = options.GetOptionVal(OPTION_CHECK_DATA_CONNECTION_IP);
	if (checkDataIP) {
		//Check if the IP of the remote machine is valid
		UINT port = 0;

		if (family == AF_INET6) {
			controlIP = GetIPV6LongForm(controlIP);
			dataIP = GetIPV6LongForm(dataIP);
		}

		if (checkDataIP == 1) {
			if (family == AF_INET6) {
				// Assume a /64
				controlIP = controlIP.Left(20);
				dataIP = dataIP.Left(20);
			}
			else {
				// Assume a /24
				controlIP = controlIP.Left(controlIP.ReverseFind('.'));
				dataIP = dataIP.Left(dataIP.ReverseFind('.'));
			}
		}

		if (controlIP != dataIP) {
			return false;
		}
	}

	return true;
}

BOOL CTransferSocket::InitTransfer(BOOL bCalledFromSend)
{
	//Check if the IP of the remote machine is valid
	CStdString OwnerIP, TransferIP;
	UINT port = 0;
	if (!m_pOwner->GetPeerName(OwnerIP, port)) {
		EndTransfer(transfer_status_t::ip_mismatch);
		return FALSE;
	}
	if (!GetPeerName(TransferIP, port)) {
		EndTransfer(transfer_status_t::ip_mismatch);
		return FALSE;
	}
	if (!IsAllowedDataConnectionIP(OwnerIP, TransferIP, GetFamily(), *m_pOwner->m_owner.m_pOptions)) {
		EndTransfer(transfer_status_t::ip_mismatch);
		return FALSE;
	}


	if (m_nMode == TRANSFERMODE_RECEIVE)
		AsyncSelect(FD_READ|FD_CLOSE);
	else
		AsyncSelect(FD_WRITE|FD_CLOSE);

	if (m_bAccepted)
		m_pOwner->SendTransferPreliminary();

	m_bStarted = true;
	if (m_nMode == TRANSFERMODE_SEND) {
		ASSERT(m_Filename != _T(""));
		int shareMode = FILE_SHARE_READ;
		if (m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_SHAREDWRITE))
			shareMode |= FILE_SHARE_WRITE;
		m_hFile = CreateFile(m_Filename, GENERIC_READ, shareMode, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
		if (m_hFile == INVALID_HANDLE_VALUE) {
			EndTransfer(transfer_status_t::noaccess);
			return FALSE;
		}
		DWORD low=(DWORD)(m_nRest&0xFFFFFFFF);
		LONG high=(LONG)(m_nRest>>32);
		if ((low = SetFilePointer(m_hFile, low, &high, FILE_BEGIN)) == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
			high = 0;
			low = SetFilePointer(m_hFile, 0, &high, FILE_END);
			if (low == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
				EndTransfer(transfer_status_t::noaccess);
				return FALSE;
			}
		}
		m_currentFileOffset = (((__int64)high) << 32) + low;

		if (!m_pBuffer) {
			m_pBuffer = new char[m_nBufSize];
			m_nBufferPos = 0;

			if (m_useZlib) {
				m_zlibStream.next_out = (Bytef *)m_pBuffer;
				m_zlibStream.avail_out = m_nBufSize;
			}
		}
	}
	else if (m_nMode == TRANSFERMODE_RECEIVE) {
		unsigned int buflen = 0;
		int varlen = sizeof(buflen);
		if (GetSockOpt(SO_RCVBUF, &buflen, &varlen)) {
			if (buflen < m_nBufSize) {
				buflen = m_nBufSize;
				SetSockOpt(SO_RCVBUF, &buflen, varlen);
			}
		}

		if (m_hFile == INVALID_HANDLE_VALUE) {
			ASSERT(m_Filename != _T(""));
			int shareMode = FILE_SHARE_READ;
			if (m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_SHAREDWRITE))
				shareMode |= FILE_SHARE_WRITE;
			m_hFile = CreateFile(m_Filename, GENERIC_WRITE, shareMode, 0, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, 0);
			if (m_hFile == INVALID_HANDLE_VALUE) {
				EndTransfer(transfer_status_t::noaccess);
				return FALSE;
			}
			DWORD low = (DWORD)(m_nRest&0xFFFFFFFF);
			LONG high = (LONG)(m_nRest>>32);
			low = SetFilePointer(m_hFile, low, &high, FILE_BEGIN);
			if (low == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
				EndTransfer(transfer_status_t::noaccess);
				return FALSE;
			}
			SetEndOfFile(m_hFile);
			m_currentFileOffset = (((__int64)high) << 32) + low;
		}

		if (!m_pBuffer)
			m_pBuffer = new char[m_nBufSize];
	}

	GetSystemTime(&m_LastActiveTime);
	return TRUE;
}

BOOL CTransferSocket::CheckForTimeout()
{
	if (!m_bReady) {
		return FALSE;
	}

	// CheckForTimeout is called once per second. Misuse it to also trigger updating of the send buffer size.
	UpdateSendBufferSize();

	_int64 timeout = m_pOwner->m_owner.m_pOptions->GetOptionVal(OPTION_TIMEOUT);

	SYSTEMTIME sCurrentTime;
	GetSystemTime(&sCurrentTime);
	FILETIME fCurrentTime;
	if (!SystemTimeToFileTime(&sCurrentTime, &fCurrentTime)) {
		return FALSE;
	}
	FILETIME fLastTime;
	if (m_wasActiveSinceCheck) {
		m_wasActiveSinceCheck = false;
		GetSystemTime(&m_LastActiveTime);
		return TRUE;
	}

	if (!SystemTimeToFileTime(&m_LastActiveTime, &fLastTime)) {
		return FALSE;
	}
	_int64 elapsed = ((_int64)(fCurrentTime.dwHighDateTime - fLastTime.dwHighDateTime) << 32) + fCurrentTime.dwLowDateTime - fLastTime.dwLowDateTime;
	if (timeout && elapsed > (timeout*10000000)) {
		EndTransfer(transfer_status_t::timeout);
		return TRUE;
	}
	else if (!m_bStarted && elapsed > (10 * 10000000)) {
		EndTransfer(transfer_status_t::noconn);
		return TRUE;
	}
	else if (!timeout) {
		return FALSE;
	}

	return TRUE;
}

bool CTransferSocket::Started() const
{
	return m_bStarted;
}

int CTransferSocket::GetMode() const
{
	return m_nMode;
}

bool CTransferSocket::UseSSL(bool use)
{
	if (m_pSslLayer)
		return false;

	m_use_tls = use;

	return true;
}

int CTransferSocket::OnLayerCallback(std::list<t_callbackMsg> const& callbacks)
{
	for (auto const& cb : callbacks) {
		if (m_pSslLayer && cb.pLayer == m_pSslLayer) {
			if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_INFO && cb.nParam2 == SSL_INFO_SHUTDOWNCOMPLETE) {
				EndTransfer(transfer_status_t::success);
				return 0;
			}
			else if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_VERBOSE_WARNING) {
				if (cb.str) {
					CStdString str = "Data connection TLS warning: ";
					str += cb.str;

					m_pOwner->SendStatus(str, 1);
				}
			}
#if SSL_VERBOSE_INFO
			// Verbose info for debugging
			else if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_VERBOSE_INFO) {
				if (cb.str) {
					CStdString str = "TLS info: ";
					str += cb.str;

					m_pOwner->SendStatus(str, 0);
				}
			}
#endif
			else if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_INFO_ESTABLISHED) {
				m_waitingForSslHandshake = false;
				m_pOwner->SendStatus(_T("TLS connection for data connection established"), 0);

				// Re-enable Nagle algorithm
				SetNodelay(false);
				return 0;
			}
			else if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_FAILURE) {
				if (cb.nParam2 == SSL_FAILURE_NO_SESSIONREUSE) {
					EndTransfer(transfer_status_t::tls_no_resume);
				}
				else {
					EndTransfer(transfer_status_t::tls_unknown);
				}
			}
		}
	}
	return 0;
}

bool CTransferSocket::InitZLib(int level)
{
	int res;
	if (m_nMode == TRANSFERMODE_RECEIVE)
		res = inflateInit2(&m_zlibStream, 15);
	else
		res = deflateInit2(&m_zlibStream, level, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);

	if (res == Z_OK)
		m_useZlib = true;

	return res == Z_OK;
}

bool CTransferSocket::GetZlibStats(_int64 &bytesIn, _int64 &bytesOut) const
{
	bytesIn = m_zlibBytesIn;
	bytesOut = m_zlibBytesOut;

	return true;
}

void CTransferSocket::EndTransfer(transfer_status_t status)
{
	Close();

	CloseFile();

	if (m_bSentClose)
		return;

	m_bSentClose = true;
	m_status = status;
	m_pOwner->m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_TRANSFERMSG, m_pOwner->m_userid);
}

void CTransferSocket::CloseFile()
{
	if (m_hFile != INVALID_HANDLE_VALUE) {
		if (m_nMode == TRANSFERMODE_RECEIVE)
			FlushFileBuffers(m_hFile);

		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

bool CTransferSocket::CreateListenSocket(PortLease&& port, int family)
{
	portLease_ = std::move(port);
	return Create(portLease_.GetPort(), SOCK_STREAM, FD_ACCEPT, 0, family);
}

void CTransferSocket::UpdateSendBufferSize()
{
	if (m_bStarted && (m_nMode == TRANSFERMODE_SEND || m_nMode == TRANSFERMODE_LIST)) {
		auto fd = GetSocketHandle();
		if (fd != INVALID_SOCKET) {
			ULONG v{};
			DWORD outlen{};
			if (!WSAIoctl(fd, SIO_IDEAL_SEND_BACKLOG_QUERY, 0, 0, &v, sizeof(v), &outlen, 0, 0)) {
				int size = static_cast<int>(v);
				SetSockOpt(SO_SNDBUF, &size, sizeof(int));
			}
		}
	}
}
