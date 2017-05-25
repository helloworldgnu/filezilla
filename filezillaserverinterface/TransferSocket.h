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

#if !defined(AFX_TRANSFERSOCKET_H__38ADA982_DD96_4607_B7D2_982011F162FE__INCLUDED_)
#define AFX_TRANSFERSOCKET_H__38ADA982_DD96_4607_B7D2_982011F162FE__INCLUDED_

#include "pasv_port_randomizer.h"

class CControlSocket;

#define TRANSFERMODE_NOTSET 0
#define TRANSFERMODE_LIST 1
#define TRANSFERMODE_RECEIVE 2
#define TRANSFERMODE_SEND 3

struct t_dirlisting;

#include <zlib.h>

enum class transfer_status_t
{
	success,
	closed_aborted,
	noconn,
	noaccess,
	timeout,
	ip_mismatch,
	zlib,
	tls_no_resume,
	tls_unknown
};

class CAsyncSslSocketLayer;
class CTransferSocket final : public CAsyncSocketEx
{
public:
	CTransferSocket(CControlSocket *pOwner);
	void Init(std::list<t_dirlisting> &dir, int nMode);
	void Init(const CStdString& filename, int nMode, _int64 rest);
	inline bool InitCalled() { return m_bReady; }
	bool UseSSL(bool use);
	virtual ~CTransferSocket();
	void CloseFile();

	int GetMode() const;
	bool Started() const;
	BOOL CheckForTimeout();
	void PasvTransfer();
	transfer_status_t GetStatus() const;
	bool InitZLib(int level);
	bool GetZlibStats(_int64 &bytesIn, _int64 &bytesOut) const;
	__int64 GetCurrentFileOffset() const { return m_currentFileOffset; }
	bool WasActiveSinceCheck() const { return m_wasActiveSinceCheck; }

	bool WasOnConnectCalled() const { return m_on_connect_called; }

	bool CreateListenSocket(PortLease&& port, int family);

	static bool IsAllowedDataConnectionIP(CStdString controlIP, CStdString dataIP, int family, COptions& options);

protected:
	virtual void OnSend(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void OnAccept(int nErrorCode);
	virtual void OnReceive(int nErrorCode);

	void UpdateSendBufferSize();

	virtual int OnLayerCallback(std::list<t_callbackMsg> const& callbacks);

	void EndTransfer(transfer_status_t status);

	std::list<t_dirlisting> directory_listing_;
	t_dirlisting *m_pDirListing;
	bool m_bSentClose{};
	CStdString m_Filename;
	bool m_bReady{};
	bool m_bStarted{};
	BOOL InitTransfer(BOOL bCalledFromSend);
	int m_nMode;
	transfer_status_t m_status{transfer_status_t::success};
	CControlSocket *m_pOwner;
	_int64 m_nRest;
	HANDLE m_hFile;
	char *m_pBuffer;
	char *m_pBuffer2; // Used by zlib transfers
	unsigned int m_nBufferPos;
	BOOL m_bAccepted;
	SYSTEMTIME m_LastActiveTime;
	bool m_wasActiveSinceCheck;

	CAsyncSslSocketLayer* m_pSslLayer;
	bool m_use_tls{};

	unsigned int m_nBufSize;
	bool m_useZlib;
	z_stream m_zlibStream;
	__int64 m_zlibBytesIn;
	__int64 m_zlibBytesOut;

	__int64 m_currentFileOffset;

	bool m_waitingForSslHandshake;

	bool m_premature_send;

	bool m_on_connect_called;

	PortLease portLease_;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_TRANSFERSOCKET_H__38ADA982_DD96_4607_B7D2_982011F162FE__INCLUDED_
