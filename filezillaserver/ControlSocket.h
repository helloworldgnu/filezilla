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

#if !defined(AFX_CONTROLSOCKET_H__17DD46FD_8A4A_4394_9F90_C14BA65F6BF6__INCLUDED_)
#define AFX_CONTROLSOCKET_H__17DD46FD_8A4A_4394_9F90_C14BA65F6BF6__INCLUDED_

#include "hash_thread.h"
#include "Permissions.h"

class CAsyncSslSocketLayer;
class CTransferSocket;

#ifndef TRANSFERMODE_NOTSET
#define TRANSFERMODE_NOTSET 0
#endif

/////////////////////////////////////////////////////////////////////////////
// Befehlsziel CControlSocket

enum class commands;
struct t_command
{
	commands id;
	bool bHasargs;
	bool bValidBeforeLogon;
};

class CControlSocket final : public CAsyncSocketEx
{
// Attribute
public:

// Operationen
public:
	CControlSocket(CServerThread & owner);
	virtual ~CControlSocket();

// Überschreibungen
public:
	CServerThread & m_owner;
	CStdString m_RemoteIP;
	void WaitGoOffline();
	bool m_bWaitGoOffline{};
	void CheckForTimeout();
	void ForceClose(int nReason);
	CTransferSocket* GetTransferSocket();
	void ProcessTransferMsg();
	void ParseCommand();
	t_command const& MapCommand(CStdString const& command, CStdString const& args);
	int m_userid{};
	BOOL Send(LPCTSTR str, bool sendStatus = true, bool newline = true);
	void SendStatus(LPCTSTR status,int type);
	CStdString PrepareSend(CStdString const& str, bool sendStatus = true);
	bool GetCommand(CStdString &command, CStdString &args);
	bool InitImplicitSsl();

	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	void AntiHammerIncrease(int amount = 1);

	void Continue();

	void ProcessHashResult(int hash_id, int res, CHashThread::_algorithm alg, const CStdString& hash, const CStdString& file);

	void SendTransferPreliminary();

	void UpdateUser();

	CAsyncSslSocketLayer * GetSslLayer() { return m_pSslLayer; }

protected:
	BOOL DoUserLogin(LPCTSTR password);
	BOOL UnquoteArgs(CStdString &args);
	static int GetUserCount(const CStdString &user);
	static void IncUserCount(const CStdString &user);
	static void DecUserCount(const CStdString &user);
	void ResetTransferstatus( bool send_info = true );
	void ResetTransferSocket( bool send_info = true );
	BOOL CreateTransferSocket(CTransferSocket *pTransferSocket);
	bool CheckIpForZlib();
	void SendTransferinfoNotification(const char transfermode = TRANSFERMODE_NOTSET, const CStdString& physicalFile = "", const CStdString& logicalFile = "", __int64 startOffset = 0, __int64 totalSize = -1);
	bool CanQuit();
	CStdString GetPassiveIP();
	bool CreatePassiveTransferSocket();
	bool VerifyIPFromPortCommand(CStdString & ip);

	virtual int OnLayerCallback(std::list<t_callbackMsg> const& callbacks);

	CAsyncSslSocketLayer *m_pSslLayer{};

	std::list<CStdStringA> m_RecvLineBuffer;
	char m_RecvBuffer[2048];
	int m_nRecvBufferPos{};
	char *m_pSendBuffer{};
	int m_nSendBufferLen{};

	int m_nTelnetSkip{};
	bool m_bQuitCommand{};
	SYSTEMTIME m_LastCmdTime, m_LastTransferTime, m_LoginTime;
	static std::map<CStdString, int> m_UserCount;
	CStdString m_CurrentServerDir;
	static std::recursive_mutex m_mutex;

	struct t_status
	{
		BOOL loggedon{};
		CStdString username;
		CUser user;
		CStdString ip;

		int hammerValue{};
	} m_status;

	struct t_transferstatus
	{
		int pasv{-1};
		_int64 rest{0};
		int type{-1};
		CStdString ip;
		int port{-1};
		CTransferSocket *socket{};
		bool usedResolvedIP{};
		int family{AF_UNSPEC};
		CStdString resource;
	} m_transferstatus;

	CStdString RenName;
	bool bRenFile{};

	enum TransferMode
	{
		mode_stream,
		mode_zlib
	};

	TransferMode m_transferMode{mode_stream};
	int m_zlibLevel{};

	int m_antiHammeringWaitTime{};

	bool m_bProtP{};

	void ParseMlstOpts(CStdString args);
	void ParseHashOpts(CStdString args);

	// Enabled MLST facts
	bool m_facts[4];

	bool m_shutdown{};

	int m_hash_id{};

	enum CHashThread::_algorithm m_hash_algorithm;

public:
	long long GetSpeedLimit(enum sltype);

	typedef struct {
		bool bContinue;
		long long nBytesAllowedToTransfer;
		long long nTransferred;
		bool bBypassed;
	} t_Quota;
	t_Quota m_SlQuotas[2];
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ fügt unmittelbar vor der vorhergehenden Zeile zusätzliche Deklarationen ein.

#endif // AFX_CONTROLSOCKET_H__17DD46FD_8A4A_4394_9F90_C14BA65F6BF6__INCLUDED_
