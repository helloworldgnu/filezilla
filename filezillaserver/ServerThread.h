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

#if !defined(AFX_SERVERTHREAD_H__4F566540_62DF_4338_85DE_EC699EB6640C__INCLUDED_)
#define AFX_SERVERTHREAD_H__4F566540_62DF_4338_85DE_EC699EB6640C__INCLUDED_

#include "Thread.h"

class CControlSocket;
class CServerThread;
class COptions;
class CPermissions;
class t_user;
class CExternalIpCheck;
class CAutoBanManager;
class CHashThread;

struct t_socketdata
{
	CControlSocket *pSocket{};
	CServerThread *pThread{};
};

/////////////////////////////////////////////////////////////////////////////
// Thread CServerThread

class CServerThread final : public CThread
{
public:
	virtual CStdString GetExternalIP(const CStdString& localIP);
	virtual void ExternalIPFailed();
	explicit CServerThread(int nNotificationMessageId);

	CPermissions *m_pPermissions{};
	COptions *m_pOptions{};
	CAutoBanManager* m_pAutoBanManager{};

	void IncRecvCount(int count);
	void IncSendCount(int count);
	void IncIpCount(const CStdString &ip);
	void DecIpCount(const CStdString &ip);
	int GetIpCount(const CStdString &ip) const;
	bool IsReady();
	static const int GetGlobalNumConnections();
	void AddSocket(SOCKET sockethandle, bool ssl);
	const int GetNumConnections();

	struct t_Notification
	{
		WPARAM wParam{};
		LPARAM lParam{};
	};

	void SendNotification(WPARAM wParam, LPARAM lParam);

	/*
	 * The parameter should be an empty list, since m_pendingNotifications and
	 * list get swapped to increase performance.
	 */
	void GetNotifications(std::list<CServerThread::t_Notification>& list);

	void AntiHammerIncrease(CStdString const& ip);
	void AntiHammerDecrease(CStdString const& ip);

	CHashThread& GetHashThread();

	long long GetInitialSpeedLimit(int mode);

protected:
	virtual ~CServerThread();

	void GatherTransferedBytes();
	void ProcessNewSlQuota();
	virtual BOOL InitInstance();
	virtual DWORD ExitInstance();

	void OnPermissionsUpdated();

	virtual int OnThreadMessage(UINT Msg, WPARAM wParam, LPARAM lParam);
	void OnTimer(WPARAM wParam, LPARAM lParam);

	void ProcessControlMessage(t_controlmessage *msg);
	CControlSocket * GetControlSocket(int userid);
	std::map<int, CControlSocket *> m_LocalUserIDs;
	void AddNewSocket(SOCKET sockethandle, bool ssl);
	static int CalcUserID();
	static std::map<int, t_socketdata> m_userids;
	static std::map<CStdString, int> m_userIPs;
	void AntiHammerDecay();

	int m_nRecvCount{};
	int m_nSendCount{};
	UINT m_nRateTimer{};
	bool m_bQuit{};

	std::recursive_mutex m_mutex;
	static std::recursive_mutex m_global_mutex;
	unsigned int m_timerid{};

	//Speed limit code
	static std::vector<CServerThread *> m_sInstanceList; //First instance is the SL master
	BOOL m_bIsMaster{};
	int m_nLoopCount{};

	int m_lastLimits[2];
	struct t_Quota {
		long long nBytesAllowedToTransfer{-1};
		long long nTransferred{};
	};
	t_Quota m_SlQuotas[2];

	CStdString m_RawWelcomeMessage;
	std::vector<CStdString> m_ParsedWelcomeMessage;
	CExternalIpCheck *m_pExternalIpCheck{};

	std::list<t_Notification> m_pendingNotifications;
	int m_throttled{};

	int m_nNotificationMessageId{};

	static std::map<CStdString, int> m_antiHammerInfo;
	int m_antiHammerTimer{};

	static CHashThread* m_hashThread;

	CAsyncSocketEx* threadSocketData_{};
};

#endif // AFX_SERVERTHREAD_H__4F566540_62DF_4338_85DE_EC699EB6640C__INCLUDED_
