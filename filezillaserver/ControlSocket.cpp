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

// ControlSocket.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "ControlSocket.h"
#include "transfersocket.h"
#include "ServerThread.h"
#include "Options.h"
#include "Permissions.h"
#include "AsyncSslSocketLayer.h"
#include <math.h>
#include "iputils.h"
#include "autobanmanager.h"
#include "pasv_port_randomizer.h"

/////////////////////////////////////////////////////////////////////////////
// CControlSocket

std::map<CStdString, int> CControlSocket::m_UserCount;
std::recursive_mutex CControlSocket::m_mutex;

CControlSocket::CControlSocket(CServerThread & owner)
	: m_owner(owner)
	, m_hash_algorithm(CHashThread::SHA1)
{
	GetSystemTime(&m_LastTransferTime);
	GetSystemTime(&m_LastCmdTime);
	GetSystemTime(&m_LoginTime);

	for (int i = 0; i < 2; ++i) {
		m_SlQuotas[i].bContinue = false;
		m_SlQuotas[i].nBytesAllowedToTransfer = owner.GetInitialSpeedLimit(i);
		m_SlQuotas[i].nTransferred = 0;
		m_SlQuotas[i].bBypassed = false;
	}

	for (int i = 0; i < 3; i++)
		m_facts[i] = true;
	m_facts[fact_perm] = false;
}

CControlSocket::~CControlSocket()
{
	if (m_status.loggedon) {
		DecUserCount(m_status.username);
		m_owner.DecIpCount(m_status.ip);
		m_status.loggedon = FALSE;
	}
	t_connop *op = new t_connop;
	op->data = 0;
	op->op = USERCONTROL_CONNOP_REMOVE;
	op->userid = m_userid;
	m_owner.SendNotification(FSM_CONNECTIONDATA, (LPARAM)op);
	ResetTransferstatus(false);

	delete [] m_pSendBuffer;
	m_nSendBufferLen = 0;

	RemoveAllLayers();
	delete m_pSslLayer;
}

/////////////////////////////////////////////////////////////////////////////
// Member-Funktion CControlSocket

#define BUFFERSIZE 500
void CControlSocket::OnReceive(int nErrorCode)
{
	if (m_antiHammeringWaitTime) {
		if (nErrorCode) {
			//Control connection has been closed
			Close();
			SendStatus(_T("disconnected."), 0);
			m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);
		}
		return;
	}

	int len = BUFFERSIZE;
	long long nLimit = GetSpeedLimit(upload);
	if (!nLimit) {
		ParseCommand();
		return;
	}
	if (len > nLimit && nLimit > -1)
		len = static_cast<int>(nLimit);

	unsigned char *buffer = new unsigned char[BUFFERSIZE];
	int numread = Receive(buffer, len);
	if (numread != SOCKET_ERROR && numread) {
		if (nLimit > -1)
			m_SlQuotas[upload].nTransferred += numread;

		m_owner.IncRecvCount(numread);
		//Parse all received bytes
		for (int i = 0; i < numread; i++)
		{
			if (!m_nRecvBufferPos)
			{
				//Remove telnet characters
				if (m_nTelnetSkip) {
					if (buffer[i] < 240)
						m_nTelnetSkip = 0;
					else
						continue;
				}
				else if (buffer[i] == 255) {
					m_nTelnetSkip = 1;
					continue;
				}
			}

			//Check for line endings
			if ((buffer[i] == '\r')||(buffer[i] == 0)||(buffer[i] == '\n'))
			{
				//If input buffer is not empty...
				if (m_nRecvBufferPos)
				{
					m_RecvBuffer[m_nRecvBufferPos] = 0;
					m_RecvLineBuffer.push_back(m_RecvBuffer);
					m_nRecvBufferPos = 0;

					//Signal that there is a new command waiting to be processed.
					GetSystemTime(&m_LastCmdTime);
				}
			}
			else
				//The command may only be 2000 chars long. This ensures that a malicious user can't
				//send extremely large commands to fill the memory of the server
				if (m_nRecvBufferPos < 2000)
					m_RecvBuffer[m_nRecvBufferPos++] = buffer[i];
		}
	}
	else
	{
		if (!numread || GetLastError() != WSAEWOULDBLOCK)
		{
			//Control connection has been closed
			Close();
			SendStatus(_T("disconnected."), 0);
			m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);

			delete [] buffer;
			return;
		}
	}

	ParseCommand();

	delete [] buffer;
}

bool CControlSocket::GetCommand(CStdString &command, CStdString &args)
{
	//Get first command from input buffer
	CStdStringA str;
	if (m_RecvLineBuffer.empty()) {
		return false;
	}
	str = m_RecvLineBuffer.front();
	m_RecvLineBuffer.pop_front();

	//Output command in status window
	CStdString str2 = ConvFromNetwork(str);

	//Hide passwords if the server admin wants to.
	if (!str2.Left(5).CompareNoCase(_T("PASS "))) {
		if (m_owner.m_pOptions->GetOptionVal(OPTION_LOGSHOWPASS)) {
			SendStatus(str2, 2);
		}
		else {
			CStdString msg = str2;
			for (size_t i = 5; i < msg.size(); ++i) {
				msg[i] = '*';
			}
			SendStatus(msg, 2);
		}
	}
	else {
		SendStatus(str2, 2);
	}

	// Split command and arguments
	int pos = str2.Find(_T(" "));
	if (pos != -1) {
		command = str2.Left(pos);
		if (pos == str2.GetLength() - 1) {
			args.clear();
		}
		else {
			args = str2.Mid(pos + 1);
			if (args.empty()) {
				Send(_T("501 Syntax error, failed to decode string"));
				return false;
			}
		}
	}
	else {
		args.clear();
		command = str2;
	}
	if (command.empty()) {
		return false;
	}
	command.MakeUpper();
	return true;
}

void CControlSocket::SendStatus(LPCTSTR status, int type)
{
	t_statusmsg *msg = new t_statusmsg;
	_tcsncpy(msg->ip,  m_RemoteIP, sizeof(msg->ip) / sizeof(TCHAR));
	msg->ip[sizeof(msg->ip) / sizeof(TCHAR) - 1] = 0;
	GetLocalTime(&msg->time);
	if (!m_status.loggedon) {
		msg->user = new TCHAR[16];
		_tcscpy(msg->user, _T("(not logged in)"));
	}
	else {
		msg->user = new TCHAR[_tcslen(m_status.username) + 1];
		_tcscpy(msg->user, m_status.username);
	}
	msg->userid = m_userid;
	msg->type = type;
	msg->status = new TCHAR[_tcslen(status) + 1];
	_tcscpy(msg->status, status);
	m_owner.SendNotification(FSM_STATUSMESSAGE, (LPARAM)msg);
}

BOOL CControlSocket::Send(LPCTSTR str, bool sendStatus, bool newline)
{
	if (!*str)
		return false;

	if (sendStatus)
		SendStatus(str, 3);

	char* buffer;
	int len;
	{
		auto utf8 = ConvToNetwork(str);
		if (utf8.empty()) {
			Close();
			SendStatus(_T("Failed to convert reply to UTF-8"), 1);
			m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);

			return false;
		}

		buffer = new char[utf8.size() + 3];
		strcpy(buffer, utf8.c_str());
		if (newline) {
			strcpy(buffer + utf8.size(), "\r\n");
			len = utf8.size() + 2;
		}
		else {
			len = utf8.size();
		}
	}

	//Add line to back of send buffer if it's not empty
	if (m_pSendBuffer) {
		char *tmp = m_pSendBuffer;
		m_pSendBuffer = new char[m_nSendBufferLen + len];
		memcpy(m_pSendBuffer, tmp, m_nSendBufferLen);
		memcpy(m_pSendBuffer + m_nSendBufferLen, buffer, len);
		delete [] tmp;
		m_nSendBufferLen += len;
		delete [] buffer;
		return TRUE;
	}

	long long nLimit = GetSpeedLimit(download);
	if (!nLimit) {
		if (!m_pSendBuffer) {
			m_pSendBuffer = buffer;
			m_nSendBufferLen = len;
		}
		else {
			char *tmp = m_pSendBuffer;
			m_pSendBuffer = new char[m_nSendBufferLen + len];
			memcpy(m_pSendBuffer, tmp, m_nSendBufferLen);
			memcpy(m_pSendBuffer + m_nSendBufferLen, buffer, len);
			delete [] tmp;
			m_nSendBufferLen += len;
			delete [] buffer;
		}
		return TRUE;
	}
	int numsend = len;
	if (nLimit > -1 && numsend > nLimit)
		numsend = static_cast<int>(nLimit);

	int res = CAsyncSocketEx::Send(buffer, numsend);
	if (res == SOCKET_ERROR && GetLastError() == WSAEWOULDBLOCK) {
		res = 0;
	}
	else if (!res || res == SOCKET_ERROR) {
		delete [] buffer;
		Close();
		SendStatus(_T("could not send reply, disconnected."), 0);
		m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);
		return FALSE;
	}

	if (nLimit > -1)
		m_SlQuotas[download].nTransferred += res;

	if (res != len) {
		if (!m_pSendBuffer) {
			m_pSendBuffer = new char[len - res];
			memcpy(m_pSendBuffer, buffer + res, len - res);
			m_nSendBufferLen = len - res;
		}
		else {
			char *tmp = m_pSendBuffer;
			m_pSendBuffer = new char[m_nSendBufferLen + len - res];
			memcpy(m_pSendBuffer, tmp, m_nSendBufferLen);
			memcpy(m_pSendBuffer + m_nSendBufferLen, buffer + res, len - res);
			delete [] tmp;
			m_nSendBufferLen += len - res;
		}
		TriggerEvent(FD_WRITE);
	}
	delete [] buffer;

	m_owner.IncSendCount(res);
	return TRUE;
}

void CControlSocket::OnClose(int nErrorCode)
{
	Close();
	SendStatus(_T("disconnected."), 0);
	m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);
	CAsyncSocketEx::OnClose(nErrorCode);
}

enum class commands {
	invalid = -1,
	USER,
	PASS,
	QUIT,
	CWD,
	PWD,
	PORT,
	PASV,
	TYPE,
	LIST,
	REST,
	CDUP,
	RETR,
	STOR,
	SIZE,
	DELE,
	RMD,
	MKD,
	RNFR,
	RNTO,
	ABOR,
	SYST,
	NOOP,
	APPE,
	NLST,
	MDTM,
	NOP,
	EPSV,
	EPRT,
	AUTH,
	ADAT,
	PBSZ,
	PROT,
	FEAT,
	MODE,
	OPTS,
	HELP,
	ALLO,
	MLST,
	MLSD,
	SITE,
	STRU,
	CLNT,
	MFMT,
	HASH
};

t_command const invalid_command = {commands::invalid, false, false};
std::map<CStdString, t_command> const command_map = {
	{_T("USER"), {commands::USER, true,  true}},
	{_T("PASS"), {commands::PASS, false, true}},
	{_T("QUIT"), {commands::QUIT, false, true}},
	{_T("CWD"),  {commands::CWD,  false, false}},
	{_T("PWD"),  {commands::PWD,  false, false}},
	{_T("PORT"), {commands::PORT, true,  false}},
	{_T("PASV"), {commands::PASV, false, false}},
	{_T("TYPE"), {commands::TYPE, true,  false}},
	{_T("LIST"), {commands::LIST, false, false}},
	{_T("REST"), {commands::REST, true,  false}},
	{_T("CDUP"), {commands::CDUP, false, false}},
	{_T("RETR"), {commands::RETR, true,  false}},
	{_T("STOR"), {commands::STOR, true,  false}},
	{_T("SIZE"), {commands::SIZE, true,  false}},
	{_T("DELE"), {commands::DELE, true,  false}},
	{_T("RMD"),  {commands::RMD,  true,  false}},
	{_T("MKD"),  {commands::MKD,  true,  false}},
	{_T("RNFR"), {commands::RNFR, true,  false}},
	{_T("RNTO"), {commands::RNTO, true,  false}},
	{_T("ABOR"), {commands::ABOR, false, false}},
	{_T("SYST"), {commands::SYST, false, true}},
	{_T("NOOP"), {commands::NOOP, false, false}},
	{_T("APPE"), {commands::APPE, true,  false}},
	{_T("NLST"), {commands::NLST, false, false}},
	{_T("MDTM"), {commands::MDTM, true,  false}},
	{_T("XPWD"), {commands::PWD,  false, false}},
	{_T("XCUP"), {commands::CDUP, false, false}},
	{_T("XMKD"), {commands::MKD,  true,  false}},
	{_T("XRMD"), {commands::RMD,  true,  false}},
	{_T("XCWD"), {commands::CWD,  true,  false}},
	{_T("NOP"),  {commands::NOP,  false, false}},
	{_T("EPSV"), {commands::EPSV, false, false}},
	{_T("EPRT"), {commands::EPRT, true,  false}},
	{_T("AUTH"), {commands::AUTH, true,  true}},
	{_T("ADAT"), {commands::ADAT, true,  true}},
	{_T("PBSZ"), {commands::PBSZ, true,  true}},
	{_T("PROT"), {commands::PROT, true,  true}},
	{_T("FEAT"), {commands::FEAT, false, true}},
	{_T("MODE"), {commands::MODE, true,  false}},
	{_T("OPTS"), {commands::OPTS, true,  true}},
	{_T("HELP"), {commands::HELP, false, true}},
	{_T("ALLO"), {commands::ALLO, false, false}},
	{_T("MLST"), {commands::MLST, false, false}},
	{_T("MLSD"), {commands::MLSD, false, false}},
	{_T("SITE"), {commands::SITE, true,  true}},
	{_T("STRU"), {commands::STRU, true, false}},
	{_T("CLNT"), {commands::CLNT, true, true}},
	{_T("MFMT"), {commands::MFMT, true, false}},
	{_T("HASH"), {commands::HASH, true, false}}
};

t_command const& CControlSocket::MapCommand(CStdString const& command, CStdString const& args)
{
	auto const& it = command_map.find(command);
	if( it != command_map.end() ) {
		//Does the command needs an argument?
		if( it->second.bHasargs && args.empty() ) {
			Send(_T("501 Syntax error"));
			if (!m_RecvLineBuffer.empty()) {
				m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_COMMAND, m_userid);
			}
		}
		//Can it be issued before logon?
		else if( !m_status.loggedon && !it->second.bValidBeforeLogon) {
			Send(_T("530 Please log in with USER and PASS first."));
			if (!m_RecvLineBuffer.empty()) {
				m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_COMMAND, m_userid);
			}
		}
		else {
			// Valid command!
			return it->second;
		}
	}
	else {
		//Command not recognized
		Send(_T("500 Syntax error, command unrecognized."));
		if (!m_RecvLineBuffer.empty()) {
			m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_COMMAND, m_userid);
		}
	}

	return invalid_command;
}

void CControlSocket::ParseCommand()
{
	if (m_antiHammeringWaitTime)
		return;

	//Get command
	CStdString command;
	CStdString args;
	if (!GetCommand(command, args))
		return;

	t_command const& cmd = MapCommand(command, args);
	if (cmd.id == commands::invalid) {
		return;
	}

	//Now process the commands
	switch (cmd.id)
	{
	case commands::USER:
		{
			AntiHammerIncrease();

			if (m_status.loggedon) {
				GetSystemTime(&m_LoginTime);
				DecUserCount(m_status.username);
				m_owner.DecIpCount(m_status.ip);
				t_connop *op = new t_connop;
				op->op = USERCONTROL_CONNOP_CHANGEUSER;
				t_connectiondata_changeuser *conndata = new t_connectiondata_changeuser;
				op->data = conndata;
				op->userid = m_userid;
				m_owner.SendNotification(FSM_CONNECTIONDATA, (LPARAM)op);

				m_status.loggedon = FALSE;
				m_CurrentServerDir = _T("");
			}
			if (m_owner.m_pOptions->GetOptionVal(OPTION_ENABLETLS) &&
				m_owner.m_pOptions->GetOptionVal(OPTION_TLSFORCEEXPLICIT) && !m_pSslLayer)
			{
				Send(_T("530 This server does not allow plain FTP. You have to use FTP over TLS."));
				break;
			}
			RenName.clear();
			args.MakeLower();
			m_status.username = args;
			UpdateUser();
			if (!m_pSslLayer) {
				if (m_owner.m_pPermissions->CheckUserLogin(m_status.user, _T(""), true) && m_status.user.ForceSsl()) {
					m_status.username.clear();
					UpdateUser();
					Send(_T("530 TLS required"));
					break;
				}
			}
			Send(_T("331 Password required for ") + args);
		}
		break;
	case commands::PASS:
		AntiHammerIncrease();

		if (m_status.loggedon)
			Send(_T("503 Bad sequence of commands."));
		else if (DoUserLogin(args))
			Send(_T("230 Logged on"));
		break;
	case commands::QUIT:
		m_bQuitCommand = true;
		if (!m_transferstatus.socket || !m_transferstatus.socket->InitCalled())
		{
			Send(_T("221 Goodbye"));
			if (m_pSslLayer)
			{
				if (ShutDown() || WSAGetLastError() != WSAEWOULDBLOCK)
					ForceClose(5);
			}
			else if (CanQuit())
				ForceClose(5);
		}
		break;
	case commands::CWD:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send(_T("501 Syntax error"));
				break;
			}

			if (args == _T("")) {
				CStdString str;
				str.Format(_T("250 Broken client detected, missing argument to CWD. \"%s\" is current directory."), m_CurrentServerDir);
				Send(str);
				break;
			}

			int res = m_owner.m_pPermissions->ChangeCurrentDir(m_status.user, m_CurrentServerDir, args);
			if (!res) {
				CStdString str;
				str.Format(_T("250 CWD successful. \"%s\" is current directory."), m_CurrentServerDir);
				Send(str);
			}
			else if (res & PERMISSION_DENIED) {
				CStdString str;
				str.Format(_T("550 CWD failed. \"%s\": Permission denied."), args);
				Send(str);
			}
			else if (res & PERMISSION_INVALIDNAME) {
				CStdString str;
				str.Format(_T("550 CWD failed. \"%s\": Filename invalid."), args);
				Send(str);
			}
			else if (res) {
				CStdString str;
				str.Format(_T("550 CWD failed. \"%s\": directory not found."), args);
				Send(str);
			}
		}
		break;
	case commands::PWD:
		{
			CStdString str;
			str.Format(_T("257 \"%s\" is current directory."), m_CurrentServerDir);
			Send(str);
		}
		break;
	case commands::PORT:
		{
			if (GetFamily() != AF_INET) {
				Send(_T("500 You are connected using IPv6. PORT is only for IPv4. You have to use the EPRT command instead."));
				break;
			}

			ResetTransferstatus();

			int count = 0;
			int pos = 0;
			//Convert commas to dots
			args.Replace(_T(","), _T("."));
			while (1) {
				pos = args.Find(_T("."), pos);
				if (pos != -1)
					count++;
				else
					break;
				pos++;
			}
			if (count != 5) {
				Send(_T("501 Syntax error"));
				m_transferstatus.pasv = -1;
				break;
			}
			CStdString ip;
			int port = 0;
			int i = args.ReverseFind('.');
			port = _ttoi(args.Right(args.GetLength() - (i + 1))); //get ls byte of server socket
			args = args.Left(i);
			i = args.ReverseFind('.');
			port += 256 * _ttoi(args.Right(args.GetLength() - (i + 1))); // add ms byte to server socket
			ip = args.Left(i);

			// Fix: Convert IP in PORT command to int and back to string (strips
			// leading zeros) because some FTP clients prepend zeros to it.
			// inet_addr() thinks this is an octal number and will return INADDR_NONE
			// if 8 or 9 are encountered.
			//
			// As per RFC 959: "the	value of each field is transmitted as a decimal number"
			CStdString decIP;
			ip += _T(".");
			pos = ip.Find('.');
			while (pos != -1) {
				CStdString tmp;
				tmp.Format(_T("%d."), _ttoi(ip.Left(pos)));
				decIP += tmp;

				ip = ip.Mid(pos + 1);
				pos = ip.Find('.');
			}

			ip = decIP.Left(decIP.GetLength() - 1);
			int res = inet_addr(ConvToLocal(ip));

			if (res == INADDR_NONE || port < 1 || port > 65535) {
				Send(_T("501 Syntax error"));
				break;
			}

			if (!VerifyIPFromPortCommand(ip)) {
				// Function already sends errors
				break;
			}
	
			m_transferstatus.ip = ip;
			m_transferstatus.port = port;
			m_transferstatus.pasv = 0;
			m_transferstatus.family = AF_INET;
			Send(_T("200 Port command successful"));
			break;
		}
	case commands::PASV:
		{
			if (GetFamily() != AF_INET) {
				Send(_T("500 You are connected using IPv6. PASV is only for IPv4. You have to use the EPSV command instead."));
				break;
			}

			ResetTransferstatus();

			CStdString pasvIP = GetPassiveIP();
			if (pasvIP == _T("")) {
				Send(_T("421 Could not create socket, no usable IP address found."));
				m_transferstatus.pasv = -1;
				break;
			}

			if( !CreatePassiveTransferSocket() ) {
				break;
			}

			//Now retrieve the port
			CStdString dummy;
			UINT port = 0;
			if (m_transferstatus.socket->GetSockName(dummy, port)) {
				//Reformat the ip
				pasvIP.Replace(_T("."), _T(","));
				//Put the answer together
				CStdString str;
				str.Format(_T("227 Entering Passive Mode (%s,%d,%d)"), pasvIP, port / 256, port % 256);
				Send(str);
			}
			else {
				ResetTransferstatus(false);
				Send(_T("421 Could not create socket, unable to query socket for used port."));
				break;
			}

			break;
		}
	case commands::TYPE:
		{
			args.MakeUpper();
			if (args[0] != 'I' && args[0] != 'A' && args != _T("L 8"))
			{
				Send(_T("501 Unsupported type. Supported types are I, A and L 8"));
				break;
			}
			m_transferstatus.type = (args[0] == 'A') ? 1 : 0;
			CStdString str;
			str.Format(_T("200 Type set to %s"), args);
			Send(str);
		}
		break;
	case commands::LIST:
	case commands::MLSD:
	case commands::NLST:
		if (m_transferstatus.pasv == -1) {
			Send(_T("503 Bad sequence of commands."));
		}
		else if (!m_transferstatus.pasv && (m_transferstatus.ip == _T("") || m_transferstatus.port == -1)) {
			Send(_T("503 Bad sequence of commands."));
		}
		else if (m_pSslLayer && m_owner.m_pOptions->GetOptionVal(OPTION_FORCEPROTP) && !m_bProtP) {
			Send(_T("521 PROT P required"));
		}
		else {
			if( cmd.id != commands::MLSD ) {
				//Check args, currently only supported argument is the directory which will be listed.
				args.TrimLeft(_T(" "));
				args.TrimRight(_T(" "));
				if (args != _T(""))
				{
					BOOL bBreak = FALSE;
					while (args[0] == '-') //No parameters supported
					{
						if (args.GetLength() < 2)
						{ //Dash without param
							Send(_T("501 Syntax error"));
							bBreak = TRUE;
							break;
						}

						int pos = args.Find(' ');
						CStdString params;
						if (pos != -1)
						{
							params = args.Left(1);
							params = params.Left(pos - 1);
							args = args.Mid(pos + 1);
							args.TrimLeft(_T(" "));
						}
						else
							args = _T("");
						while (params != _T(""))
						{
							//Some parameters are not support
							if (params[0] == 'R')
							{
								Send(_T("504 Command not implemented for that parameter"));
								bBreak = TRUE;
								break;
							}
							//Ignore other parameters
							params = params.Mid(1);
						}

						if (args == _T(""))
							break;
					}
					if (bBreak)
						break;
				}
			}

			//Unquote args
			if (!UnquoteArgs(args)) {
				Send(_T("501 Syntax error"));
				break;
			}

			CPermissions::addFunc_t addFunc = CPermissions::AddFactsListingEntry;
			if( cmd.id == commands::LIST ) {
				addFunc = CPermissions::AddLongListingEntry;
			}
			else if( cmd.id == commands::NLST ) {
				addFunc = CPermissions::AddShortListingEntry;
			}

			std::list<t_dirlisting> result;
			CStdString physicalDir, logicalDir;
			int error = m_owner.m_pPermissions->GetDirectoryListing(m_status.user, m_CurrentServerDir, args, result, physicalDir, logicalDir
				, addFunc, m_facts);
			if (error & PERMISSION_DENIED) {
				Send(_T("550 Permission denied."));
				ResetTransferstatus();
			}
			else if (error & PERMISSION_INVALIDNAME) {
				Send(_T("550 Filename invalid."));
				ResetTransferstatus();
			}
			else if (error) {
				Send(_T("550 Directory not found."));
				ResetTransferstatus();
			}
			else {
				m_transferstatus.resource = logicalDir;

				if (!m_transferstatus.pasv) {
					ResetTransferSocket();

					CTransferSocket *transfersocket = new CTransferSocket(this);
					m_transferstatus.socket = transfersocket;
					transfersocket->Init(result, TRANSFERMODE_LIST);
					if (m_transferMode == mode_zlib) {
						if (!transfersocket->InitZLib(m_zlibLevel))
						{
							Send(_T("550 could not initialize zlib, please use MODE S instead"));
							ResetTransferstatus();
							break;
						}
					}

					if (!CreateTransferSocket(transfersocket))
						break;
				}
				else {
					if (!m_transferstatus.socket) {
						Send(_T("503 Bad sequence of commands."));
						break;
					}
					m_transferstatus.socket->Init(result, TRANSFERMODE_LIST);
					if (m_transferMode == mode_zlib)
					{
						if (!m_transferstatus.socket->InitZLib(m_zlibLevel))
						{
							Send(_T("550 could not initialize zlib, please use MODE S instead"));
							ResetTransferstatus();
							break;
						}
					}

					m_transferstatus.socket->PasvTransfer();
				}
				SendTransferinfoNotification(TRANSFERMODE_LIST, physicalDir, logicalDir);
			}
		}
		break;
	case commands::REST:
		{
			BOOL error = FALSE;
			for (int i = 0; i < args.GetLength(); i++)
				if (args[i] < '0' || args[i] > '9')
				{
					error = TRUE;
					break;
				}
			if (error)
			{
				Send(_T("501 Bad parameter. Numeric value required"));
				break;
			}
			m_transferstatus.rest = _ttoi64(args);
			CStdString str;
			str.Format(_T("350 Rest supported. Restarting at %I64d"), m_transferstatus.rest);
			Send(str);
		}
		break;
	case commands::CDUP:
		{
			CStdString dir = _T("..");
			int res = m_owner.m_pPermissions->ChangeCurrentDir(m_status.user, m_CurrentServerDir, dir);
			if (!res) {
				CStdString str;
				str.Format(_T("200 CDUP successful. \"%s\" is current directory."), m_CurrentServerDir);
				Send(str);
			}
			else if (res & PERMISSION_DENIED) {
				Send(_T("550 CDUP failed, permission denied."));
			}
			else if (res & PERMISSION_INVALIDNAME) {
				Send(_T("550 CDUP failed, filename invalid."));
			}
			else if (res) {
				Send(_T("550 CDUP failed, directory not found."));
			}
		}
		break;
	case commands::RETR:
		if (m_transferstatus.pasv == -1) {
			Send(_T("503 Bad sequence of commands."));
		}
		else if (!m_transferstatus.pasv && (m_transferstatus.ip == _T("") || m_transferstatus.port == -1)) {
			Send(_T("503 Bad sequence of commands."));
		}
		else if (m_pSslLayer && m_owner.m_pOptions->GetOptionVal(OPTION_FORCEPROTP) && !m_bProtP) {
			Send(_T("521 PROT P required"));
		}
		else if (!UnquoteArgs(args)) {
			Send( _T("501 Syntax error") );
		}
		else {
			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_READ, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED) {
				Send(_T("550 Permission denied"));
				ResetTransferstatus();
			}
			else if (error & PERMISSION_INVALIDNAME) {
				Send(_T("550 Filename invalid."));
				ResetTransferstatus();
			}
			else if (error) {
				Send(_T("550 File not found"));
				ResetTransferstatus();
			}
			else {
				m_transferstatus.resource = logicalFile;
				if (!m_transferstatus.pasv) {
					ResetTransferSocket();

					CTransferSocket *transfersocket = new CTransferSocket(this);
					m_transferstatus.socket = transfersocket;
					transfersocket->Init(physicalFile, TRANSFERMODE_SEND, m_transferstatus.rest);
					if (m_transferMode == mode_zlib) {
						if (!transfersocket->InitZLib(m_zlibLevel)) {
							Send(_T("550 could not initialize zlib, please use MODE S instead"));
							ResetTransferstatus();
							break;
						}
					}

					if (!CreateTransferSocket(transfersocket))
						break;
				}
				else {
					if (!m_transferstatus.socket) {
						Send(_T("503 Bad sequence of commands."));
						break;
					}

					m_transferstatus.socket->Init(physicalFile, TRANSFERMODE_SEND, m_transferstatus.rest);
					if (m_transferMode == mode_zlib) {
						if (!m_transferstatus.socket->InitZLib(m_zlibLevel)) {
							Send(_T("550 could not initialize zlib, please use MODE S instead"));
							ResetTransferstatus();
							break;
						}
					}

					m_transferstatus.socket->PasvTransfer();
				}

				__int64 totalSize = GetLength64(physicalFile);
				SendTransferinfoNotification(TRANSFERMODE_SEND, physicalFile, logicalFile, m_transferstatus.rest, totalSize);
				GetSystemTime(&m_LastTransferTime);
			}
		}
		break;
	case commands::STOR:
	case commands::APPE:
		if (m_transferstatus.pasv == -1) {
			Send(_T("503 Bad sequence of commands."));
		}
		else if (!m_transferstatus.pasv && (m_transferstatus.ip == _T("") || m_transferstatus.port == -1)) {
			Send(_T("503 Bad sequence of commands."));
		}
		else if (m_pSslLayer && m_owner.m_pOptions->GetOptionVal(OPTION_FORCEPROTP) && !m_bProtP) {
			Send(_T("521 PROT P required"));
		}
		else if (!UnquoteArgs(args)) {
			Send( _T("501 Syntax error") );
		}
		else {
			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, (m_transferstatus.rest || cmd.id == commands::APPE) ? FOP_APPEND : FOP_WRITE, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED) {
				Send(_T("550 Permission denied"));
				ResetTransferstatus();
			}
			else if (error & PERMISSION_INVALIDNAME) {
				Send(_T("550 Filename invalid."));
				ResetTransferstatus();
			}
			else if (error) {
				Send(_T("550 Filename invalid"));
				ResetTransferstatus();
			}
			else {
				if( cmd.id == commands::APPE ) {
					m_transferstatus.rest = GetLength64(physicalFile);
					if( m_transferstatus.rest < 0 ) {
						m_transferstatus.rest = 0;
					}
				}

				m_transferstatus.resource = logicalFile;
				if (!m_transferstatus.pasv) {
					CTransferSocket *transfersocket = new CTransferSocket(this);
					transfersocket->Init(physicalFile, TRANSFERMODE_RECEIVE, m_transferstatus.rest);
					m_transferstatus.socket = transfersocket;
					if (m_transferMode == mode_zlib) {
						if (!transfersocket->InitZLib(m_zlibLevel)) {
							Send(_T("550 could not initialize zlib, please use MODE S instead"));
							ResetTransferstatus();
							break;
						}
					}

					if (!CreateTransferSocket(transfersocket))
						break;
				}
				else {
					if (!m_transferstatus.socket) {
						Send(_T("503 Bad sequence of commands."));
						break;
					}

					m_transferstatus.socket->Init(physicalFile, TRANSFERMODE_RECEIVE, m_transferstatus.rest);
					if (m_transferMode == mode_zlib) {
						if (!m_transferstatus.socket->InitZLib(m_zlibLevel)) {
							Send(_T("550 could not initialize zlib, please use MODE S instead"));
							ResetTransferstatus();
							break;
						}
					}
					m_transferstatus.socket->PasvTransfer();
				}
				SendTransferinfoNotification(TRANSFERMODE_RECEIVE, physicalFile, logicalFile, m_transferstatus.rest);
				GetSystemTime(&m_LastTransferTime);
			}
		}
		break;
	case commands::SIZE:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_LIST, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
				Send(_T("550 Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else if (error)
				Send(_T("550 File not found"));
			else
			{
				CStdString str;
				_int64 length = GetLength64(physicalFile);
				if (length >= 0)
					str.Format(_T("213 %I64d"), length);
				else
					str = _T("550 File not found");
				Send(str);
			}
		}
		break;
	case commands::DELE:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send(_T("501 Syntax error"));
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_DELETE, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
				Send(_T("550 Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else if (error)
				Send(_T("550 File not found"));
			else
			{
				bool success = DeleteFile(physicalFile) == TRUE;
				if (!success && GetLastError() == ERROR_ACCESS_DENIED)
				{
					DWORD attr = GetFileAttributes(physicalFile);
					if (attr != INVALID_FILE_ATTRIBUTES && attr & FILE_ATTRIBUTE_READONLY)
					{
						attr &= ~FILE_ATTRIBUTE_READONLY;
						SetFileAttributes(physicalFile, attr);

						success = DeleteFile(physicalFile) == TRUE;
					}
				}
				if (!success)
					Send(_T("500 Failed to delete the file."));
				else
					Send(_T("250 File deleted successfully"));
			}
		}
		break;
	case commands::RMD:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckDirectoryPermissions(m_status.user, args, m_CurrentServerDir, DOP_DELETE, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
				Send(_T("550 Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else if (error)
				Send(_T("550 Directory not found"));
			else
			{
				if (!RemoveDirectory(physicalFile))
				{
					if (GetLastError() == ERROR_DIR_NOT_EMPTY)
						Send(_T("550 Directory not empty."));
					else
						Send(_T("450 Internal error deleting the directory."));
				}
				else
					Send(_T("250 Directory deleted successfully"));
			}
		}
		break;
	case commands::MKD:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckDirectoryPermissions(m_status.user, args, m_CurrentServerDir, DOP_CREATE, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
				Send(_T("550 Can't create directory. Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else if (error & PERMISSION_DOESALREADYEXIST && (error & PERMISSION_FILENOTDIR)!=PERMISSION_FILENOTDIR)
				Send(_T("550 Directory already exists"));
			else if (error & PERMISSION_FILENOTDIR)
				Send(_T("550 File with same name already exists"));
			else if (error)
				Send(_T("550 Directoryname not valid"));
			else
			{
				CStdString str;
				BOOL res = FALSE;
				BOOL bReplySent = FALSE;
				physicalFile += _T("\\");
				while (physicalFile != _T(""))
				{
					CStdString piece = physicalFile.Left(physicalFile.Find('\\')+1);
					if (piece.Right(2) == _T(".\\"))
					{
						Send(_T("550 Directoryname not valid"));
						bReplySent = TRUE;
						break;
					}
					str += piece;
					physicalFile = physicalFile.Mid(physicalFile.Find('\\') + 1);
					res = CreateDirectory(str, 0);
				}
				if (!bReplySent)
					if (!res)//CreateDirectory(result+"\\",0))
					{
						int error = GetLastError();
						if (error == ERROR_ALREADY_EXISTS)
							Send(_T("550 Directory already exists"));
						else
							Send(_T("450 Internal error creating the directory."));
					}
					else
						Send(_T("257 \"") + logicalFile + _T("\" created successfully"));
			}
		}
		break;
	case commands::RNFR:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			RenName = _T("");

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_DELETE, physicalFile, logicalFile);
			if (!error) {
				RenName = physicalFile;
				bRenFile = true;
				Send(_T("350 File exists, ready for destination name."));
				break;
			}
			else if (error & PERMISSION_DENIED)
				Send(_T("550 Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else {
				int error2 = m_owner.m_pPermissions->CheckDirectoryPermissions(m_status.user, args, m_CurrentServerDir, DOP_DELETE, physicalFile, logicalFile);
				if (!error2) {
					RenName = physicalFile;
					bRenFile = false;
					Send(_T("350 Directory exists, ready for destination name."));
				}
				else if (error2 & PERMISSION_DENIED)
					Send(_T("550 Permission denied"));
				else if (error2 & PERMISSION_INVALIDNAME)
					Send(_T("550 Filename invalid."));
				else
					Send(_T("550 file/directory not found"));
				break;
			}
		}
		break;
	case commands::RNTO:
		{
			if (RenName == _T(""))
			{
				Send(_T("503 Bad sequence of commands!"));
				break;
			}

			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			if (bRenFile) {
				CStdString physicalFile, logicalFile;
				int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_CREATENEW, physicalFile, logicalFile);
				if (error)
					RenName = _T("");
				if (error & PERMISSION_DENIED)
					Send(_T("550 Permission denied"));
				else if (error & PERMISSION_INVALIDNAME)
					Send(_T("553 Filename invalid."));
				else if (error & PERMISSION_DOESALREADYEXIST && (error & PERMISSION_DIRNOTFILE)!=PERMISSION_DIRNOTFILE)
					Send(_T("553 file exists"));
				else if (error)
					Send(_T("553 Filename invalid"));
				else
				{
					if (!MoveFile(RenName, physicalFile))
						Send(_T("450 Internal error renaming the file"));
					else
						Send(_T("250 file renamed successfully"));
				}
			}
			else {
				CStdString physicalFile, logicalFile;
				int error = m_owner.m_pPermissions->CheckDirectoryPermissions(m_status.user, args, m_CurrentServerDir, DOP_CREATE, physicalFile, logicalFile);
				if (error)
					RenName = _T("");
				if (error & PERMISSION_DENIED)
					Send(_T("550 Permission denied"));
				else if (error & PERMISSION_INVALIDNAME)
					Send(_T("550 Filename invalid."));
				else if (error & PERMISSION_DOESALREADYEXIST && (error & PERMISSION_FILENOTDIR)!=PERMISSION_FILENOTDIR)
					Send(_T("550 file exists"));
				else if (error)
					Send(_T("550 Filename invalid"));
				else
				{
					if (!MoveFile(RenName, physicalFile))
						Send(_T("450 Internal error renaming the file"));
					else
						Send(_T("250 file renamed successfully"));
				}
			}
		}
		break;
	case commands::ABOR:
		{
			if (m_transferstatus.socket)
			{
				if (m_transferstatus.socket->Started())
					Send(_T("426 Connection closed; transfer aborted."));
			}
			Send(_T("226 ABOR command successful"));
			ResetTransferstatus();
		break;
		}
	case commands::SYST:
		Send(_T("215 UNIX emulated by FileZilla"));
		break;
	case commands::NOOP:
	case commands::NOP:
		Send(_T("200 OK"));
		break;
	case commands::MDTM:
		{
			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_LIST, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
				Send(_T("550 Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else if (error & 2)
				Send(_T("550 File not found"));
			else {
				WIN32_FILE_ATTRIBUTE_DATA status{};
				if (GetStatus64(physicalFile, status)) {
					CStdString str;
					SYSTEMTIME time;
					FileTimeToSystemTime(&status.ftLastWriteTime, &time);
					str.Format(_T("213 %04d%02d%02d%02d%02d%02d"),
								time.wYear,
								time.wMonth,
								time.wDay,
								time.wHour,
								time.wMinute,
								time.wSecond);
					Send(str);
				}
				else {
					Send(_T("550 Could not get file attributes"));
				}
			}
		}
		break;
	case commands::EPSV:
		{
			ResetTransferstatus();

			if( !CreatePassiveTransferSocket() ) {
				break;
			}

			//Now retrieve the port
			CStdString dummy;
			UINT port = 0;
			if (m_transferstatus.socket->GetSockName(dummy, port))
			{
				//Put the answer together
				CStdString str;
				str.Format(_T("229 Entering Extended Passive Mode (|||%d|)"), port);
				Send(str);
			}
			else
			{
				ResetTransferstatus(false);
				Send(_T("421 Could not create socket, unable to query socket for used port."));
			}
			break;
		}
	case commands::EPRT:
		{
			ResetTransferstatus();

			if (args[0] != '|') {
				Send(_T("501 Syntax error"));
				break;
			}
			args = args.Mid(1);

			int pos = args.Find('|');
			if (pos < 1 || (pos>=(args.GetLength()-1))) {
				Send(_T("501 Syntax error"));
				break;
			}
			int protocol = _ttoi(args.Left(pos));

			bool const ipv6Allowed = m_owner.m_pOptions->GetOptionVal(OPTION_DISABLE_IPV6) == 0;
			if (protocol != 1 && (protocol != 2 || !ipv6Allowed)) {
				if (ipv6Allowed)
					Send(_T("522 Extended Port Failure - unknown network protocol. Supported protocols: (1,2)"));
				else
					Send(_T("522 Extended Port Failure - unknown network protocol. Supported protocols: (1)"));
				break;
			}
			args = args.Mid(pos + 1);

			pos = args.Find('|');
			if (pos < 1 || (pos>=(args.GetLength()-1))) {
				Send(_T("501 Syntax error"));
				break;
			}
			CStdString ip = args.Left(pos);
			if (protocol == 1) {
				if (inet_addr(ConvToLocal(ip)) == INADDR_NONE) {
					Send(_T("501 Syntax error, not a valid IPv4 address"));
					break;
				}
			}
			else {
				ip = GetIPV6LongForm(ip);
				if (ip.IsEmpty()) {
					Send(_T("501 Syntax error, not a valid IPv6 address"));
					break;
				}
			}
			args = args.Mid(pos + 1);

			pos = args.Find('|');
			if (pos < 1) {
				Send(_T("501 Syntax error"));
				m_transferstatus.pasv = -1;
				break;
			}
			int port = _ttoi(args.Left(pos));
			if (port < 1 || port > 65535) {
				Send(_T("501 Syntax error"));
				m_transferstatus.pasv = -1;
				break;
			}

			if (!VerifyIPFromPortCommand(ip)) {
				break;
			}

			m_transferstatus.port = port;
			m_transferstatus.ip = ip;
			m_transferstatus.family = (protocol == 1) ? AF_INET : AF_INET6;

			m_transferstatus.pasv = 0;
			Send(_T("200 Port command successful"));
			break;
		}
	case commands::AUTH:
		{
			if (m_nRecvBufferPos || m_RecvLineBuffer.size() ) {
				Send(_T("503 Bad sequence of commands. Received additional data after the AUTH command before this reply could be sent."));
				ForceClose(-1);
				break;
			}

			if (m_pSslLayer) {
				Send(_T("534 Authentication type already set to TLS"));
				break;
			}
			args.MakeUpper();

			if (args == _T("SSL") || args == _T("TLS")) {
				if (m_pSslLayer) {
					Send(_T("503 Already using TLS"));
					break;
				}

				if (!m_owner.m_pOptions->GetOptionVal(OPTION_ENABLETLS) || !m_owner.m_pOptions->GetOptionVal(OPTION_ALLOWEXPLICITTLS)) {
					Send(_T("502 Explicit TLS authentication not allowed"));
					break;
				}

				m_pSslLayer = new CAsyncSslSocketLayer(static_cast<int>(m_owner.m_pOptions->GetOptionVal(OPTION_TLS_MINVERSION)));
				BOOL res = AddLayer(m_pSslLayer);

				if (res) {
					CString error;
					int res = 0;//m_pSslLayer->SetCertKeyFile(m_owner.m_pOptions->GetOption(OPTION_TLSCERTFILE), m_owner.m_pOptions->GetOption(OPTION_TLSKEYFILE), m_owner.m_pOptions->GetOption(OPTION_TLSKEYPASS), &error);
					if (res == SSL_FAILURE_LOADDLLS)
						SendStatus(_T("Failed to load TLS libraries"), 1);
					else if (res == SSL_FAILURE_INITSSL)
						SendStatus(_T("Failed to initialize TLS libraries"), 1);
					else if (res == SSL_FAILURE_VERIFYCERT) {
						if (error != _T(""))
							SendStatus(error, 1);
						else
							SendStatus(_T("Failed to set certificate and private key"), 1);
					}
					if (res) {
						RemoveAllLayers();
						delete m_pSslLayer;
						m_pSslLayer = NULL;
						Send(_T("431 Could not initialize TLS connection"));
						break;
					}
				}

				if (res) {
					int code = m_pSslLayer->InitSSLConnection(false);
					if (code == SSL_FAILURE_LOADDLLS)
						SendStatus(_T("Failed to load TLS libraries"), 1);
					else if (code == SSL_FAILURE_INITSSL)
						SendStatus(_T("Failed to initialize TKS library"), 1);

					res = (code == 0);
				}

				if (res) {
					SendStatus(_T("234 Using authentication type TLS"), 3);
					static const char* reply = "234 Using authentication type TLS\r\n";
					const int len = strlen(reply);
					res = (m_pSslLayer->SendRaw(reply, len) == len);
				}

				if (!res) {
					RemoveAllLayers();
					delete m_pSslLayer;
					m_pSslLayer = NULL;
					Send(_T("431 Could not initialize TLS connection"));
					break;
				}
			}
			else {
				Send(_T("504 Auth type not supported"));
				break;
			}

			break;
		}
	case commands::ADAT:
		Send(_T("502 Command not implemented for this authentication type"));
		break;
	case commands::PBSZ:
		if (m_pSslLayer)
			Send(_T("200 PBSZ=0"));
		else
			Send(_T("502 Command not implemented for this authentication type"));
		break;
	case commands::PROT:
		if (m_pSslLayer) {
			args.MakeUpper();
			if (args == _T("C")) {
				if (m_owner.m_pOptions->GetOptionVal(OPTION_FORCEPROTP))
					Send(_T("534 This server requires an encrypted data connection with PROT P"));
				else {
					if (m_transferstatus.socket)
						m_transferstatus.socket->UseSSL(false);

					Send(_T("200 Protection level set to C"));
					m_bProtP = false;
				}
			}
			else if (args == _T("P")) {
				if (m_transferstatus.socket)
					m_transferstatus.socket->UseSSL(true);

				Send(_T("200 Protection level set to P"));
				m_bProtP = true;
			}
			else if (args == _T("S") || args == _T("E"))
				Send(_T("504 Protection level ") + args + _T(" not supported"));
			else
				Send(_T("504 Protection level ") + args + _T(" not recognized"));
		}
		else
			Send(_T("502 Command not implemented for this authentication type"));
		break;
	case commands::FEAT:
		{
		CStdString reply;
		reply += PrepareSend(_T("211-Features:"));
		reply += PrepareSend(_T(" MDTM"));
		reply += PrepareSend(_T(" REST STREAM"));
		reply += PrepareSend(_T(" SIZE"));
		if (m_owner.m_pOptions->GetOptionVal(OPTION_MODEZ_USE)) {
			reply += PrepareSend(_T(" MODE Z"));
		}
		reply += PrepareSend(_T(" MLST type*;size*;modify*;"));
		reply += PrepareSend(_T(" MLSD"));
		if (m_owner.m_pOptions->GetOptionVal(OPTION_ENABLETLS) && (m_owner.m_pOptions->GetOptionVal(OPTION_ALLOWEXPLICITTLS) || m_pSslLayer)) {
			reply += PrepareSend(_T(" AUTH SSL"));
			reply += PrepareSend(_T(" AUTH TLS"));
			reply += PrepareSend(_T(" PROT"));
			reply += PrepareSend(_T(" PBSZ"));
		}
		reply += PrepareSend(_T(" UTF8"));
		reply += PrepareSend(_T(" CLNT"));
		reply += PrepareSend(_T(" MFMT"));
		if (m_owner.m_pOptions->GetOptionVal(OPTION_ENABLE_HASH)) {
			CStdString hash = _T(" HASH ");
			hash += _T("SHA-1");
			if (m_hash_algorithm == CHashThread::SHA1)
				hash += _T("*");
			hash += _T(";SHA-512");
			if (m_hash_algorithm == CHashThread::SHA512)
				hash += _T("*");
			hash += _T(";MD5");
			if (m_hash_algorithm == CHashThread::MD5)
				hash += _T("*");
			reply += PrepareSend(hash);
		}
		reply += PrepareSend(_T(" EPSV"));
		reply += PrepareSend(_T(" EPRT"));
		reply += PrepareSend(_T("211 End"));

		Send(reply, false, false);
		break;
		}
	case commands::MODE:
		if (args == _T("S") || args == _T("s"))
		{
			m_transferMode = mode_stream;
			Send(_T("200 MODE set to S."));
		}
		else if (args == _T("Z") || args == _T("z"))
		{
			if (m_owner.m_pOptions->GetOptionVal(OPTION_MODEZ_USE) || m_transferMode == mode_zlib)
			{
				if (m_transferMode == mode_zlib || CheckIpForZlib())
				{
					m_transferMode = mode_zlib;
					Send(_T("200 MODE set to Z."));
				}
				else
					Send(_T("504 MODE Z not allowed from your IP"));
			}
			else
				Send(_T("504 MODE Z not enabled"));
		}
		else if (args == _T("C") || args == _T("c") || args == _T("B") || args == _T("b"))
			Send(_T("502 Unimplemented MODE type"));
		else
			Send(_T("504 Unknown MODE type"));
		break;
	case commands::OPTS:
		args.MakeUpper();
		if (args.Left(13) == _T("MODE Z LEVEL "))
		{
			int level = _ttoi(args.Mid(13));
			if (m_zlibLevel == level || (level >= m_owner.m_pOptions->GetOptionVal(OPTION_MODEZ_LEVELMIN) && level <= m_owner.m_pOptions->GetOptionVal(OPTION_MODEZ_LEVELMAX)))
			{
				m_zlibLevel = level;
				CString str;
				str.Format(_T("200 MODE Z LEVEL set to %d"), level);
				Send(str);
			}
			else
				Send(_T("501 can't change MODE Z LEVEL do desired value"));
		}
		else if (args == _T("UTF8 ON") || args == _T("UTF-8 ON"))
			Send(_T("202 UTF8 mode is always enabled. No need to send this command."));
		else if (args == _T("UTF8 OFF") || args == _T("UTF-8 OFF"))
			Send(_T("504 UTF8 mode cannot be disabled."));
		else if (args.Left(4) == _T("MLST"))
			ParseMlstOpts(args.Mid(4));
		else if (args.Left(4) == _T("HASH"))
			ParseHashOpts(args.Mid(4));
		else
			Send(_T("501 Option not understood"));
		break;
	case commands::HELP:
		if (args.empty()) {
			Send(_T("214-The following commands are recognized:"));
			CString str;
			int i = 0;
			for( auto const& it : command_map ) {
				CString cmd = it.first;
				while (cmd.GetLength() < 4)
					cmd += _T(" ");
				str += _T("   ") + cmd;
				if (!(++i % 8)) {
					Send(str);
					str = _T("");
				}
			}
			if (!str.empty())
				Send(str);
			Send(_T("214 Have a nice day."));
		}
		else {
			args.MakeUpper();

			auto const& it = command_map.find(args);
			if( it != command_map.end() ) {
				CStdString str;
				str.Format(_T("214 Command %s is supported by FileZilla Server"), args);
				Send(str);
				break;
			}
			else {
				CStdString str;
				str.Format(_T("502 Command %s is not recognized or supported by FileZilla Server"), args);
				Send(str);
			}
		}
		break;
	case commands::ALLO:
		Send(_T("202 No storage allocation neccessary."));
		break;
	case commands::MLST:
		{
			CStdString fact;
			CStdString logicalName;
			int res = m_owner.m_pPermissions->GetFact(m_status.user, m_CurrentServerDir, args, fact, logicalName, m_facts);
			if (res & PERMISSION_DENIED)
			{
				Send(_T("550 Permission denied."));
				break;
			}
			else if (res & PERMISSION_INVALIDNAME)
			{
				Send(_T("550 Filename invalid."));
				break;
			}
			else if (res)
			{
				Send(_T("550 File or directory not found."));
				break;
			}
			CStdString str;
			str.Format(_T("250-Listing %s"), logicalName);
			if (!Send(str))
				break;
			fact = _T(" ") + fact;
			if (!Send(fact))
				break;

			Send(_T("250 End"));
		}
		break;
	case commands::SITE:
		{
			CStdString cmd;

			args.MakeUpper();

			int pos = args.Find(' ');
			if (pos != -1)
			{
				cmd = args.Left(pos);
				args = args.Mid(pos + 1);
				args.TrimLeft(_T(" "));
			}
			else
			{
				cmd = args;
				args = _T("");
			}

			if (cmd == _T("NAMEFMT"))
			{
				if (args == _T("") || args == _T("1"))
					Send(_T("200 Now using naming format \"1\""));
				else
					Send(_T("504 Naming format not implemented"));
				break;
			}
			else
			{
				Send(_T("504 Command not implemented for that parameter"));
				break;
			}
			break;
		}
	case commands::STRU:
		args.MakeUpper();
		if (args == _T("F"))
			Send(_T("200 Using file structure 'File'"));
		else
			Send(_T("504 Command not implemented for that parameter"));
		break;
	case commands::CLNT:
		Send(_T("200 Don't care"));
		break;
	case commands::MFMT:
		{
			int pos = args.find(' ');
			if (pos < 1)
			{
				Send(_T("501 Syntax error"));
				break;
			}

			CStdString timeval = args.Left(pos);
			args = args.Mid(pos + 1);

			if (timeval.GetLength() < 14)
			{
				Send( _T("501 Syntax error") );
				break;
			}

			bool numbersOnly = true;
			for (int i = 0; i < 14; i++)
			{
				if (timeval[i] < '0' || timeval[i] > '9')
				{
					numbersOnly = false;
					break;
				}
			}
			if (!numbersOnly)
			{
				Send( _T("501 Syntax error") );
				break;
			}

			int year = (timeval[0] - '0') * 1000 +
					(timeval[1] - '0') * 100 +
					(timeval[2] - '0') * 10 +
					timeval[3] - '0';

			int month = (timeval[4] - '0') * 10 + timeval[5] - '0';
			int day = (timeval[6] - '0') * 10 + timeval[7] - '0';
			int hour = (timeval[8] - '0') * 10 + timeval[9] - '0';
			int minute = (timeval[10] - '0') * 10 + timeval[11] - '0';
			int second = (timeval[12] - '0') * 10 + timeval[13] - '0';

			if (year < 1000 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
				minute > 59 || second > 59)
			{
				Send( _T("501 Not a valid date") );
				break;
			}

			SYSTEMTIME st = {0};
			st.wYear = year;
			st.wMonth = month;
			st.wDay = day;
			st.wHour = hour;
			st.wMinute = minute;
			st.wSecond = second;

			FILETIME ft;
			if (!SystemTimeToFileTime(&st, &ft))
			{
				Send( _T("501 Not a valid date") );
				break;
			}

			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			if (args == _T("")) {
				Send( _T("501 Syntax error") );
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_LIST, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
				Send(_T("550 Permission denied"));
			else if (error & PERMISSION_INVALIDNAME)
				Send(_T("550 Filename invalid."));
			else if (error & 2)
				Send(_T("550 File not found"));
			else
			{
				HANDLE hFile = CreateFile(physicalFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
				if (hFile == INVALID_HANDLE_VALUE)
					Send(_T("550 Cannot access file"));
				else
				{
					if (!SetFileTime(hFile, 0, 0, &ft))
						Send(_T("550 Failed to set file modification time"));
					else
						Send(_T("213 modify=") + timeval.Left(14) + _T("; ") + logicalFile);

					CloseHandle(hFile);
				}
			}
		}
		break;
	case commands::HASH:
		{
			if (!m_owner.m_pOptions->GetOptionVal(OPTION_ENABLE_HASH))
			{
				Send(_T("500 Syntax error, command unrecognized."));
				break;
			}

			//Unquote args
			if (!UnquoteArgs(args)) {
				Send( _T("501 Syntax error") );
				break;
			}

			CStdString physicalFile, logicalFile;
			int error = m_owner.m_pPermissions->CheckFilePermissions(m_status.user, args, m_CurrentServerDir, FOP_READ, physicalFile, logicalFile);
			if (error & PERMISSION_DENIED)
			{
				Send(_T("550 Permission denied"));
				ResetTransferstatus();
			}
			else if (error & PERMISSION_INVALIDNAME)
			{
				Send(_T("550 Filename invalid."));
				ResetTransferstatus();
			}
			else if (error)
			{
				Send(_T("550 File not found"));
				ResetTransferstatus();
			}
			else
			{
				int hash_res = m_owner.GetHashThread().Hash(physicalFile, m_hash_algorithm, m_hash_id, &m_owner);
				if (hash_res == CHashThread::BUSY)
					Send(_T("450 Another hash operation is already in progress."));
				else if (hash_res != CHashThread::PENDING)
					Send(_T("550 Failed to hash file"));
			}
		}
		break;
	default:
		Send(_T("502 Command not implemented."));
	}

	if (!m_RecvLineBuffer.empty())
		m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_COMMAND, m_userid);

	return;
}

void CControlSocket::ProcessTransferMsg()
{
	if (!m_transferstatus.socket)
		return;
	transfer_status_t const status = m_transferstatus.socket->GetStatus();

	GetSystemTime(&m_LastCmdTime);
	if (m_transferstatus.socket)
		if (m_transferstatus.socket->GetMode() == TRANSFERMODE_SEND || m_transferstatus.socket->GetMode() == TRANSFERMODE_RECEIVE)
			GetSystemTime(&m_LastTransferTime);

	if (status == transfer_status_t::noconn && m_transferstatus.pasv && m_transferstatus.usedResolvedIP)
		m_owner.ExternalIPFailed();

	int mode = m_transferstatus.socket->GetMode();
	_int64 zlibBytesIn = 0;
	_int64 zlibBytesOut = 0;
	if (m_transferMode == mode_zlib)
		m_transferstatus.socket->GetZlibStats(zlibBytesIn, zlibBytesOut);

	CStdString resource = m_transferstatus.resource;
	ResetTransferstatus();

	if (status == transfer_status_t::success) {
		CStdString msg = _T("226 Successfully transferred \"") + resource + _T("\"");

		if ((mode == TRANSFERMODE_LIST || mode == TRANSFERMODE_SEND) && zlibBytesIn && zlibBytesOut) {
			CStdString str;
			if (zlibBytesIn >= zlibBytesOut) {
				_int64 percent = 10000 - (zlibBytesOut * 10000 / zlibBytesIn);
				str.Format(_T(", compression saved %I64d of %I64d bytes (%I64d.%02I64d%%)"), zlibBytesIn - zlibBytesOut, zlibBytesIn, percent / 100, percent % 100);
			}
			else {
				_int64 percent = (zlibBytesOut * 10000 / zlibBytesIn) - 10000;
				str.Format(_T(", unfortunately compression did increase the transfer size by %I64d bytes to %I64d bytes (%I64d.%02I64d%%)"), zlibBytesOut - zlibBytesIn, zlibBytesOut, percent / 100, percent % 100);
			}
			msg += str;
		}
		else if (mode == TRANSFERMODE_RECEIVE && zlibBytesIn && zlibBytesOut) {
			CStdString str;
			if (zlibBytesOut >= zlibBytesIn) {
				_int64 percent = 10000 - (zlibBytesIn * 10000 / zlibBytesOut);
				str.Format(_T(", compression saved %I64d of %I64d bytes (%I64d.%02I64d%%)"), zlibBytesOut - zlibBytesIn, zlibBytesOut, percent / 100, percent % 100);
			}
			else {
				_int64 percent = (zlibBytesIn * 10000 / zlibBytesOut) - 10000;
				str.Format(_T(", unfortunately compression did increase the transfer size by %I64d bytes to %I64d bytes (%I64d.%02I64d%%)"), zlibBytesIn - zlibBytesOut, zlibBytesIn, percent / 100, percent % 100);
			}
			msg += str;
		}
		Send(msg);
	}
	else if (status == transfer_status_t::closed_aborted)
		Send(_T("426 Connection closed; aborted transfer of \"") + resource + _T("\""));
	else if (status == transfer_status_t::noconn)
		Send(_T("425 Can't open data connection for transfer of \"") + resource + _T("\""));
	else if (status == transfer_status_t::noaccess)
		Send(_T("550 can't access file."));
	else if (status == transfer_status_t::timeout) {
		Send(_T("426 Connection timed out, aborting transfer of \"") + resource + _T("\""));
		ForceClose(1);
		return;
	}
	else if (status == transfer_status_t::ip_mismatch)
		Send(_T("425 Rejected data connection for transfer of \"") + resource + _T("\", IP addresses of control and data connection do not match"));
	else if (status == transfer_status_t::zlib)
		Send(_T("450 zlib error"));
	else if (status == transfer_status_t::tls_no_resume)
		Send(_T("450 TLS session of data connection has not resumed or the session does not match the control connection"));
	else if (status == transfer_status_t::tls_unknown)
		Send(_T("450 Unknown TLS error on data connection"));
	if (m_bWaitGoOffline) {
		ForceClose(0);
	}
	else if (m_bQuitCommand) {
		Send(_T("221 Goodbye"));
		if (CanQuit())
			ForceClose(5);
	}
}

CTransferSocket* CControlSocket::GetTransferSocket()
{
	return m_transferstatus.socket;
}

void CControlSocket::ForceClose(int nReason)
{
	// Don't call SendTransferInfoNotification, since connection
	// does get removed real soon.
	ResetTransferstatus(false);

	if (m_shutdown && nReason == 1) {
		Close();
		m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);
		return;
	}

	if (!nReason)
		Send(_T("421 Server is going offline"));
	else if (nReason == 1)
		Send(_T("421 Connection timed out."));
	else if (nReason == 2)
		Send(_T("421 No-transfer-time exceeded. Closing control connection."));
	else if (nReason == 3)
		Send(_T("421 Login time exceeded. Closing control connection."));
	else if (nReason == 4)
		Send(_T("421 Kicked by Administrator"));
	else if (nReason == 5)
	{
		// 221 Goodbye
	}
	SendStatus(_T("disconnected."), 0);
	m_shutdown = true;
	int res = ShutDown();
	if (m_pSslLayer) {
		if (!res && GetLastError() == WSAEWOULDBLOCK)
			return;
	}
	Close();
	m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);
}

void CControlSocket::IncUserCount(const CStdString &user)
{
	int curcount=GetUserCount(user)+1;
	simple_lock lock(m_mutex);
	m_UserCount[user] = curcount;
}

void CControlSocket::DecUserCount(const CStdString &user)
{
	int curcount=GetUserCount(user)-1;
	if (curcount<0)
		return;
	simple_lock lock(m_mutex);
	m_UserCount[user] = curcount;
}

int CControlSocket::GetUserCount(const CStdString &user)
{
	simple_lock lock(m_mutex);
	int count = 0;
	std::map<CStdString, int>::iterator iter = m_UserCount.find(user);
	if (iter != m_UserCount.end())
		count = iter->second;
	return count;
}

void CControlSocket::CheckForTimeout()
{
	if (m_antiHammeringWaitTime) {
		m_antiHammeringWaitTime -= 1000;
		if (m_antiHammeringWaitTime <= 0) {
			m_antiHammeringWaitTime = 0;
			TriggerEvent(FD_FORCEREAD);
		}
	}
	if (m_status.hammerValue > 0) {
		--m_status.hammerValue;
	}

	if (m_transferstatus.socket) {
		if (m_transferstatus.socket->CheckForTimeout()) {
			return;
		}
	}
	_int64 timeout;
	if (m_shutdown) {
		timeout = 3;
	}
	else {
		timeout = m_owner.m_pOptions->GetOptionVal(OPTION_TIMEOUT);
	}
	if (!timeout) {
		return;
	}
	SYSTEMTIME sCurrentTime;
	GetSystemTime(&sCurrentTime);
	FILETIME fCurrentTime;
	if (!SystemTimeToFileTime(&sCurrentTime, &fCurrentTime)) {
		return;
	}
	FILETIME fLastTime;
	if (!SystemTimeToFileTime(&m_LastCmdTime, &fLastTime)) {
		return;
	}
	_int64 elapsed = ((_int64)(fCurrentTime.dwHighDateTime - fLastTime.dwHighDateTime) << 32) + fCurrentTime.dwLowDateTime - fLastTime.dwLowDateTime;
	if (elapsed > (timeout*10000000)) {
		ForceClose(1);
		return;
	}
	if (m_status.loggedon) { //Transfer timeout
		_int64 nNoTransferTimeout = m_owner.m_pOptions->GetOptionVal(OPTION_NOTRANSFERTIMEOUT);
		if (!nNoTransferTimeout) {
			return;
		}
		SystemTimeToFileTime(&m_LastTransferTime, &fLastTime);
		elapsed = ((_int64)(fCurrentTime.dwHighDateTime - fLastTime.dwHighDateTime) << 32) + fCurrentTime.dwLowDateTime - fLastTime.dwLowDateTime;
		if (elapsed>(nNoTransferTimeout*10000000)) {
			ForceClose(2);
			return;
		}
	}
	else { //Login timeout
		_int64 nLoginTimeout = m_owner.m_pOptions->GetOptionVal(OPTION_LOGINTIMEOUT);
		if (!nLoginTimeout) {
			return;
		}
		else if (!SystemTimeToFileTime(&m_LoginTime, &fLastTime)) {
			return;
		}
		elapsed = ((_int64)(fCurrentTime.dwHighDateTime - fLastTime.dwHighDateTime) << 32) + fCurrentTime.dwLowDateTime - fLastTime.dwLowDateTime;
		if (elapsed>(nLoginTimeout*10000000)) {
			ForceClose(3);
			return;
		}
	}
}

void CControlSocket::WaitGoOffline()
{
	if (m_transferstatus.socket) {
		if (!m_transferstatus.socket->Started()) {
			ForceClose(0);
		}
		else {
			m_bWaitGoOffline = true;
		}
	}
	else {
		ForceClose(0);
	}
}

void CControlSocket::ResetTransferSocket( bool send_info )
{
	if (m_transferstatus.socket && send_info ) {
		SendTransferinfoNotification();
	}
	delete m_transferstatus.socket;
	m_transferstatus.socket = 0;
}

void CControlSocket::ResetTransferstatus( bool send_info )
{
	ResetTransferSocket( send_info );
	m_transferstatus.ip = _T("");
	m_transferstatus.port = -1;
	m_transferstatus.pasv = -1;
	m_transferstatus.rest = 0;
	m_transferstatus.resource = _T("");
}

BOOL CControlSocket::UnquoteArgs(CStdString &args)
{
	int pos1 = args.Find('"');
	int pos2 = args.ReverseFind('"');
	if (pos1 == -1 && pos2 == -1)
		return TRUE;
	if (pos1 || pos2 != (args.GetLength()-1) || pos1 >= (pos2-1))
		return FALSE;
	args = args.Mid(1, args.GetLength() - 2);
	return TRUE;
}

void CControlSocket::OnSend(int nErrorCode)
{
	if (m_nSendBufferLen && m_pSendBuffer) {
		long long nLimit = GetSpeedLimit(download);
		if (!nLimit)
			return;
		int numsend;
		if (nLimit <= -1 || nLimit > m_nSendBufferLen)
			numsend = m_nSendBufferLen;
		else
			numsend = static_cast<int>(nLimit);

		int numsent = CAsyncSocketEx::Send(m_pSendBuffer, numsend);

		if (numsent==SOCKET_ERROR && GetLastError() == WSAEWOULDBLOCK)
			return;
		if (!numsent || numsent == SOCKET_ERROR) {
			Close();
			SendStatus(_T("could not send reply, disconnected."), 0);
			m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);

			delete [] m_pSendBuffer;
			m_pSendBuffer = NULL;
			m_nSendBufferLen = 0;

			return;
		}

		if (nLimit > -1)
			m_SlQuotas[download].nTransferred += numsent;

		if (numsent == m_nSendBufferLen) {
			delete [] m_pSendBuffer;
			m_pSendBuffer = NULL;
			m_nSendBufferLen = 0;
		}
		else {
			char *tmp = m_pSendBuffer;
			m_pSendBuffer = new char[m_nSendBufferLen-numsent];
			memcpy(m_pSendBuffer, tmp+numsent, m_nSendBufferLen-numsent);
			delete [] tmp;
			m_nSendBufferLen -= numsent;
			TriggerEvent(FD_WRITE);
		}
	}
}

BOOL CControlSocket::DoUserLogin(LPCTSTR password)
{
	if (!m_owner.m_pPermissions->CheckUserLogin(m_status.user, password, false)) {
		AntiHammerIncrease(2);
		m_owner.AntiHammerIncrease(m_RemoteIP);

		if (m_owner.m_pAutoBanManager->RegisterAttempt(m_RemoteIP)) {
			Send(_T("421 Temporarily banned for too many failed login attempts"));
			ForceClose(-1);
			return FALSE;
		}

		Send(_T("530 Login or password incorrect!"));
		return FALSE;
	}

	if (!m_status.user.IsEnabled()) {
		Send(_T("530 Not logged in, user account has been disabled"));
		ForceClose(-1);
		return FALSE;
	}
	if (!m_status.user.BypassUserLimit()) {
		int nMaxUsers = (int)m_owner.m_pOptions->GetOptionVal(OPTION_MAXUSERS);
		if (m_owner.GetGlobalNumConnections() > nMaxUsers&&nMaxUsers) {
			SendStatus(_T("Refusing connection. Reason: Max. connection count reached."), 1);
			Send(_T("421 Too many users are connected, please try again later."));
			ForceClose(-1);
			return FALSE;
		}
	}
	if (m_status.user.GetUserLimit() && GetUserCount(m_status.username) >= m_status.user.GetUserLimit()) {
		CStdString str;
		str.Format(_T("Refusing connection. Reason: Max. connection count reached for the user \"%s\"."), m_status.username);
		SendStatus(str,1);
		Send(_T("421 Too many users logged in for this account. Try again later."));
		ForceClose(-1);
		return FALSE;
	}

	CStdString peerIP;
	UINT port = 0;

	BOOL bResult = GetPeerName(peerIP, port);
	if (bResult) {
		if (!m_status.user.AccessAllowed(peerIP)) {
			Send(_T("521 This user is not allowed to connect from this IP"));
			ForceClose(-1);
			return FALSE;
		}
	}
	else {
		SendStatus(_T("Could not get peer name"), 1);
		Send(_T("421 Refusing connection. Could not get peer name."));
		ForceClose(-1);
		return FALSE;
	}

	int count = m_owner.GetIpCount(peerIP);
	if (m_status.user.GetIpLimit() && count >= m_status.user.GetIpLimit()) {
		CStdString str;
		if (count==1)
			str.Format(_T("Refusing connection. Reason: No more connections allowed from this IP. (%s already connected once)"), peerIP.c_str());
		else
			str.Format(_T("Refusing connection. Reason: No more connections allowed from this IP. (%s already connected %d times)"), peerIP.c_str(), count);
		SendStatus(str, 1);
		Send(_T("421 Refusing connection. No more connections allowed from your IP."));
		ForceClose(-1);
		return FALSE;
	}

	CStdString args = _T("/");
	int res = m_owner.m_pPermissions->ChangeCurrentDir(m_status.user, m_CurrentServerDir, args);
	if (res) {
		if (res & PERMISSION_NOTFOUND) {
			Send(_T("550 Home directory does not exist"));
		}
		else if (res & PERMISSION_DENIED) {
			Send(_T("550 User does not have permission to access own home directory"));
		}
		else {
			Send(_T("550 Home directory could not be accessed"));
		}
		return FALSE;
	}

	m_status.ip = peerIP;

	count = GetUserCount(m_status.user.user);
	if (m_status.user.GetUserLimit() && count >= m_status.user.GetUserLimit()) {
		CStdString str;
		str.Format(_T("Refusing connection. Reason: Maximum connection count (%d) reached for this user"), m_status.user.GetUserLimit());
		SendStatus(str, 1);
		str.Format(_T("421 Refusing connection. Maximum connection count reached for the user '%s'"), m_status.user.user);
		Send(str);
		ForceClose(-1);
		return FALSE;
	}

	m_owner.IncIpCount(peerIP);
	IncUserCount(m_status.username);
	m_status.loggedon = TRUE;

	GetSystemTime(&m_LastTransferTime);

	m_owner.m_pPermissions->AutoCreateDirs(m_status.user);

	t_connectiondata_changeuser *conndata = new t_connectiondata_changeuser;
	t_connop *op = new t_connop;
	op->data = conndata;
	op->op = USERCONTROL_CONNOP_CHANGEUSER;
	op->userid = m_userid;
	conndata->user = m_status.username;

	m_owner.SendNotification(FSM_CONNECTIONDATA, (LPARAM)op);

	m_owner.AntiHammerDecrease(m_RemoteIP);

	return TRUE;
}

void CControlSocket::Continue()
{
	if (m_SlQuotas[download].bContinue) {
		if( m_nSendBufferLen ) {
			TriggerEvent(FD_WRITE);
		}
		if (m_transferstatus.socket && m_transferstatus.socket->Started())
			m_transferstatus.socket->TriggerEvent(FD_WRITE);
		m_SlQuotas[download].bContinue = false;
	}

	if (m_SlQuotas[upload].bContinue) {
		TriggerEvent(FD_READ);
		if (m_transferstatus.socket && m_transferstatus.socket->Started())
			m_transferstatus.socket->TriggerEvent(FD_READ);
		m_SlQuotas[upload].bContinue = false;
	}
}

long long CControlSocket::GetSpeedLimit(sltype mode)
{
	long long nLimit = -1;
	if (m_status.loggedon ) {
		nLimit = m_status.user.GetCurrentSpeedLimit(mode);
	}
	if (nLimit > 0) {
		nLimit *= 100;
		if (m_SlQuotas[mode].nTransferred >= nLimit) {
			m_SlQuotas[mode].bContinue = true;
			return 0;
		}
		else {
			nLimit -= m_SlQuotas[mode].nTransferred;
		}
	}
	else
		nLimit = -1;
	if (m_status.user.BypassServerSpeedLimit(mode))
		m_SlQuotas[mode].bBypassed = true;
	else if (m_SlQuotas[mode].nBytesAllowedToTransfer > -1) {
		if (nLimit <= -1 || nLimit > (m_SlQuotas[mode].nBytesAllowedToTransfer - m_SlQuotas[mode].nTransferred))
			nLimit = std::max(0ll, m_SlQuotas[mode].nBytesAllowedToTransfer - m_SlQuotas[mode].nTransferred);
	}

	if (!nLimit)
		m_SlQuotas[mode].bContinue = true;

	return nLimit;
}

BOOL CControlSocket::CreateTransferSocket(CTransferSocket *pTransferSocket)
{
	/* Create socket
	 * First try control connection port - 1, if that fails try
	 * control connection port + 1. If that fails as well, let
	 * the OS decide.
	 */
	bool bFallback = false;
	BOOL bCreated = FALSE;

	// Fix: Formerly, the data connection would always be opened using the server's default (primary) IP.
	// This would cause Windows Firewall to freak out if control connection was opened on a secondary IP.
	// When using Active FTP behind Windows Firewall, no connection could be made. This fix ensures the data
	// socket is on the same IP as the control socket.
	CStdString controlIP;
	UINT controlPort = 0;
	BOOL bResult = this->GetSockName(controlIP, controlPort);

	if (bResult)
	{
		// Try create control conn. port - 1
		if (controlPort > 1)
			if (pTransferSocket->Create(controlPort - 1, SOCK_STREAM, FD_CONNECT, controlIP, m_transferstatus.family, true))
				bCreated = TRUE;
	}
	if (!bCreated)
	{
creation_fallback:
		bFallback = true;
		// Let the OS find a valid port
		if (!pTransferSocket->Create(0, SOCK_STREAM, FD_CONNECT, controlIP, m_transferstatus.family, true))
		{
			// Give up
			Send(_T("421 Could not create socket."));
			ResetTransferstatus();
			return FALSE;
		}
	}

	if (pTransferSocket->Connect(m_transferstatus.ip, m_transferstatus.port) == 0)
	{
		if (!bFallback && GetLastError() == WSAEADDRINUSE)
		{
			pTransferSocket->Close();
			goto creation_fallback;
		}

		if (GetLastError() != WSAEWOULDBLOCK)
		{
			Send(_T("425 Can't open data connection"));
			ResetTransferstatus();
			return FALSE;
		}
	}

	if (m_pSslLayer && m_bProtP)
		pTransferSocket->UseSSL(true);

	SendTransferPreliminary();

	return TRUE;
}

bool CControlSocket::CheckIpForZlib()
{
	CStdString peerIP;
	UINT port = 0;
	BOOL bResult = GetPeerName(peerIP, port);
	if (!bResult)
		return false;

	if (!m_owner.m_pOptions->GetOptionVal(OPTION_MODEZ_ALLOWLOCAL) && !IsRoutableAddress(peerIP))
		return false;

	CStdString ips = m_owner.m_pOptions->GetOption(OPTION_MODEZ_DISALLOWED_IPS);
	ips += " ";

	int pos = ips.Find(' ');
	while (pos != -1)
	{
		CStdString blockedIP = ips.Left(pos);
		ips = ips.Mid(pos + 1);
		pos = ips.Find(' ');

		if (MatchesFilter(blockedIP, peerIP))
			return false;
	}

	return true;
}

void CControlSocket::AntiHammerIncrease(int amount /*=1*/)
{
	if (m_status.hammerValue < 8000)
		m_status.hammerValue += amount * 200;

	if (m_status.hammerValue > 2000)
		m_antiHammeringWaitTime += 1000 * (int)pow(1.3, (m_status.hammerValue / 400) - 5);
}

void CControlSocket::SendTransferinfoNotification(const char transfermode, const CStdString& physicalFile, const CStdString& logicalFile, __int64 startOffset, __int64 totalSize)
{
	t_connop *op = new t_connop;
	op->op = USERCONTROL_CONNOP_TRANSFERINIT;
	op->userid = m_userid;

	t_connectiondata_transferinfo *conndata = new t_connectiondata_transferinfo;
	conndata->transferMode = transfermode;
	conndata->physicalFile = physicalFile;
	conndata->logicalFile = logicalFile;
	conndata->startOffset = startOffset;
	conndata->totalSize = totalSize;
	op->data = conndata;

	m_owner.SendNotification(FSM_CONNECTIONDATA, (LPARAM)op);
}

int CControlSocket::OnLayerCallback(std::list<t_callbackMsg> const& callbacks)
{
	for (auto const& cb : callbacks) {
		if (m_pSslLayer && cb.pLayer == m_pSslLayer) {
			if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_INFO && cb.nParam2 == SSL_INFO_ESTABLISHED)
				SendStatus(_T("TLS connection established"), 0);
			else if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_INFO && cb.nParam2 == SSL_INFO_SHUTDOWNCOMPLETE) {
				if (m_shutdown) {
					Close();
					m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);
					return 0;
				}
				if (!m_bQuitCommand) {
					continue;
				}

				ForceClose(5);

				return 0;
			}
		}
		else if (cb.nType == LAYERCALLBACK_LAYERSPECIFIC && cb.nParam1 == SSL_VERBOSE_WARNING) {
			if (cb.str) {
				CStdString str = "TLS warning: ";
				str += cb.str;

				SendStatus(str, 1);
			}
		}
	}
	return 0;
}

bool CControlSocket::InitImplicitSsl()
{
	m_pSslLayer = new CAsyncSslSocketLayer(static_cast<int>(m_owner.m_pOptions->GetOptionVal(OPTION_TLS_MINVERSION)));
	int res = AddLayer(m_pSslLayer) ? 1 : 0;
	if (!res) {
		delete m_pSslLayer;
		m_pSslLayer = 0;
		return false;
	}

	CString error;
	res = m_pSslLayer->SetCertKeyFile(m_owner.m_pOptions->GetOption(OPTION_TLSCERTFILE), m_owner.m_pOptions->GetOption(OPTION_TLSKEYFILE), m_owner.m_pOptions->GetOption(OPTION_TLSKEYPASS), &error);
	if (res == SSL_FAILURE_LOADDLLS)
		SendStatus(_T("Failed to load TLS libraries"), 1);
	else if (res == SSL_FAILURE_INITSSL)
		SendStatus(_T("Failed to initialize TLS libraries"), 1);
	else if (res == SSL_FAILURE_VERIFYCERT) {
		if (error != _T(""))
			SendStatus(error, 1);
		else
			SendStatus(_T("Failed to set certificate and private key"), 1);
	}
	if (res) {
		RemoveAllLayers();
		delete m_pSslLayer;
		m_pSslLayer = NULL;
		Send(_T("431 Could not initialize TLS connection"), 1);
		return false;
	}

	int code = m_pSslLayer->InitSSLConnection(false);
	if (code == SSL_FAILURE_LOADDLLS)
		SendStatus(_T("Failed to load TLS libraries"), 1);
	else if (code == SSL_FAILURE_INITSSL)
		SendStatus(_T("Failed to initialize TLS library"), 1);

	if (!code)
		return true;

	RemoveAllLayers();
	delete m_pSslLayer;
	m_pSslLayer = NULL;
	Send(_T("431 Could not initialize TLS connection"));

	//Close socket
	Close();
	SendStatus(_T("disconnected."), 0);
	m_owner.PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_DELSOCKET, m_userid);

	return false;
}

bool CControlSocket::CanQuit()
{
	if (m_pSslLayer)
		return false;
	return true;
}

CStdString CControlSocket::GetPassiveIP()
{
	//Get the ip of the control socket
	CStdString localIP;
	UINT localPort;
	BOOL bValidSockAddr = GetSockName(localIP, localPort);

	//Get peer ip
	CStdString peerIP;
	UINT peerPort = 0;
	BOOL bResult = GetPeerName(peerIP, peerPort);
	if (bResult)
	{
		if (m_owner.m_pOptions->GetOptionVal(OPTION_NOEXTERNALIPONLOCAL) && !IsRoutableAddress(peerIP))
		{
			// Remote IP address from an unroutable subnet

			// Inside a NAT-in-NAT environment, two different unroutable address ranges are used.
			// If remote address comes from a different unroutable subnet, don't use local
			// address.
			// Note that in a NAT-in-NAT environment, the external IP address specified will either
			// be the worldwide one or the NAT one. Either external or single-NATed users won't be able
			// to use passive mode.
			m_transferstatus.usedResolvedIP = false;

			if (!bValidSockAddr)
				return _T("");
			return localIP;
		}
	}

	if (m_owner.m_pOptions->GetOptionVal(OPTION_CUSTOMPASVIPTYPE))
	{
		CStdString pasvIP = m_owner.GetExternalIP(localIP);
		if (pasvIP != _T("") && pasvIP != localIP)
		{
			m_transferstatus.usedResolvedIP = true;
			return pasvIP;
		}
	}

	m_transferstatus.usedResolvedIP = false;

	if (!bValidSockAddr)
		return _T("");

	return localIP;
}

void CControlSocket::ParseMlstOpts(CStdString args)
{
	if (args == _T(""))
	{
		for (int i = 0; i < 4; i++)
			m_facts[i] = false;
		Send(_T("200 MLST OPTS"));
		return;
	}
	if (args[0] != ' ')
	{
		Send(_T("501 Invalid MLST options"));
		return;
	}
	args = args.Mid(1);
	if (args.Find(' ') != -1)
	{
		Send(_T("501 Invalid MLST options"));
		return;
	}

	bool facts[4] = {0};
	while (args != _T(""))
	{
		int pos = args.Find(';');
		if (pos < 1)
		{
			Send(_T("501 Invalid MLST options"));
			return;
		}

		CStdString fact = args.Left(pos);
		args = args.Mid(pos + 1);

		if (fact == _T("TYPE"))
			facts[fact_type] = true;
		else if (fact == _T("SIZE"))
			facts[fact_size] = true;
		else if (fact == _T("MODIFY"))
			facts[fact_modify] = true;
		//else if (fact == _T("PERM"))
		//	facts[fact_perm] = true;
	}

	for (int i = 0; i < 4; i++)
		m_facts[i] = facts[i];

	CStdString factstr;
	if (facts[fact_type])
		factstr += _T("type;");
	if (facts[fact_size])
		factstr += _T("size;");
	if (facts[fact_modify])
		factstr += _T("modify;");
	if (facts[fact_perm])
		factstr += _T("perm;");

	CStdString result = _T("200 MLST OPTS");
	if (factstr != _T(""))
		result += _T(" ") + factstr;

	Send(result);
}

void CControlSocket::ParseHashOpts(CStdString args)
{
	if (args == _T(""))
	{
		switch (m_hash_algorithm)
		{
		case CHashThread::MD5:
			Send(_T("200 MD5"));
			break;
		case CHashThread::SHA512:
			Send(_T("200 SHA-512"));
			break;
		default:
			Send(_T("200 SHA-1"));
			break;
		}
		return;
	}
	if (args[0] != ' ')
	{
		Send(_T("501 Invalid HASH options"));
		return;
	}
	args = args.Mid(1);
	if (args.Find(' ') != -1)
	{
		Send(_T("501 Invalid HASH options"));
		return;
	}

	if (args == _T("SHA-1"))
	{
		m_hash_algorithm = CHashThread::SHA1;
		Send(_T("200 Hash algorithm set to SHA-1"));
	}
	else if (args == _T("SHA-512"))
	{
		m_hash_algorithm = CHashThread::SHA512;
		Send(_T("200 Hash algorithm set to SHA-512"));
	}
	else if (args == _T("MD5"))
	{
		m_hash_algorithm = CHashThread::MD5;
		Send(_T("200 Hash algorithm set to MD5"));
	}
	else
		Send(_T("501 Unknown algorithm"));
}

void CControlSocket::ProcessHashResult(int hash_id, int res, CHashThread::_algorithm alg, const CStdString& hash, const CStdString& file)
{
	if (hash_id != m_hash_id)
		return;

	m_hash_id = 0;

	if (res == CHashThread::BUSY)
		Send(_T("450 Another hash operation is already in progress."));
	else if (res == CHashThread::FAILURE_OPEN)
		Send(_T("550 Failed to open file"));
	else if (res == CHashThread::FAILURE_READ)
		Send(_T("550 Could not read from file"));
	else
	{
		CStdString algname;
		switch (alg)
		{
		case CHashThread::SHA1:
			algname = "SHA-1";
			break;
		case CHashThread::SHA512:
			algname = "SHA-512";
			break;
		case CHashThread::MD5:
			algname = "MD5";
			break;
		}
		Send(_T("213 ") + algname + _T(" ") + hash + _T(" ") + file);
	}
}

void CControlSocket::SendTransferPreliminary()
{
	bool connected = m_transferstatus.socket && m_transferstatus.socket->WasOnConnectCalled();

	CStdString msg;
	if (connected)
		msg = _T("150 Data channel opened for ");
	else
		msg = _T("150 Opening data channel for ");

	int mode = m_transferstatus.socket ? m_transferstatus.socket->GetMode() : TRANSFERMODE_NOTSET;

	_int64 rest = m_transferstatus.rest;
	if (mode == TRANSFERMODE_RECEIVE)
		msg += _T("file upload to server");
	else if (mode == TRANSFERMODE_SEND)
		msg += _T("file download from server");
	else
	{
		msg += _T("directory listing");
		rest = 0;
	}

	if (!m_transferstatus.resource.IsEmpty())
	{
		msg += _T(" of \"");
		msg += m_transferstatus.resource;
		msg += _T("\"");
	}

	if (rest > 0)
	{
		CStdString s;
		s.Format(_T(", restarting at offset %I64d"), m_transferstatus.rest);
		msg += s;
	}

	Send(msg);
}

PasvPortManager pasvPortManager;

bool CControlSocket::CreatePassiveTransferSocket()
{
	m_transferstatus.socket = new CTransferSocket(this);

	CStdString peerIP;
	UINT tmp = 0;
	(void)GetPeerName(peerIP, tmp);

	unsigned int retries = 15;
	PasvPortRandomizer randomizer(pasvPortManager, peerIP, *m_owner.m_pOptions);
	while (retries > 0) {
		PortLease port = randomizer.GetPort();
		if (!port.GetPort()) {
			retries = 0;
			break;
		}
		if (m_transferstatus.socket->CreateListenSocket(std::move(port), GetFamily())) {
			break;
		}
		--retries;
	}
	if (retries <= 0) {
		ResetTransferstatus(false);
		Send(_T("421 Could not create socket."));
		return false;
	}

	if (m_pSslLayer && m_bProtP)
		m_transferstatus.socket->UseSSL(true);

	if (!m_transferstatus.socket->Listen()) {
		ResetTransferstatus(false);
		Send(_T("421 Could not create socket, listening failed."));
		return false;
	}

	m_transferstatus.pasv = 1;

	return true;
}

void CControlSocket::UpdateUser()
{
	m_status.user = m_owner.m_pPermissions->GetUser(m_status.username);
}

CStdString CControlSocket::PrepareSend(CStdString const& str, bool sendStatus)
{
	if (sendStatus) {
		SendStatus(str, 3);
	}
	return str + _T("\r\n");
}

bool CControlSocket::VerifyIPFromPortCommand(CStdString & ip)
{
	CStdString controlIP;
	UINT tmp{};
	if (!GetPeerName(controlIP, tmp)) {
		Send(_T("421 Rejected command. Could not get peer name."));
		return false;
	}

	if (m_owner.m_pOptions->GetOptionVal(OPTION_ACTIVE_IGNORELOCAL)) {
		if (!IsRoutableAddress(ip) && IsRoutableAddress(controlIP)) {
			ip = controlIP;
		}
	}

	// Verify peer IP against control connection
	if (!CTransferSocket::IsAllowedDataConnectionIP(controlIP, ip, GetFamily(), *m_owner.m_pOptions)) {
		Send(_T("421 Rejected command, requested IP address does not match control connection IP."));
		return false;
	}

	return true;
}