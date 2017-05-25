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

// Based upon example code from Nishant S
// Original code is available under http://www.codeproject.com/system/serviceskeleton.asp

#include "stdafx.h"
#include "server.h"
#include "Options.h"

void ServiceMain(DWORD argc, LPTSTR *argv);
void ServiceCtrlHandler(DWORD nControlCode);
BOOL UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
					 DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint,
					 DWORD dwWaitHint);
BOOL StartServiceThread();
DWORD ServiceExecutionThread(LPDWORD param);
HANDLE hServiceThread=0;
void KillService();

SERVICE_STATUS_HANDLE nServiceStatusHandle = 0;
HANDLE killServiceEvent = 0;
BOOL nServiceRunning = 0;
DWORD nServiceCurrentStatus = 0;

static TCHAR ServiceDisplayName[257] = _T("FileZilla Server FTP server");
static TCHAR ServiceName[257] = _T("FileZilla Server");

int SetAdminPort(int port);
int ReloadConfig();
int CompatMain(LPCSTR lpCmdLine);

class Scm final
{
public:
	Scm(int desiredAccess)
		: hScm_(OpenSCManager(0, 0, desiredAccess))
	{}

	~Scm()
	{
		if (hScm_) {
			CloseServiceHandle(hScm_);
		}
	}

	Scm(Scm const&) = delete;
	Scm& operator=(Scm const&) = delete;

	operator SC_HANDLE() {
		return hScm_;
	}

	explicit operator bool() const {
		return hScm_ != 0;
	}

private:
	SC_HANDLE hScm_{};
};

void LoadServiceName()
{
	COptions *pOptions = new COptions();

	CStdString const name = pOptions->GetOption(OPTION_SERVICE_NAME);
	if (name.size() > 0 && name.size() < 256)
		_tcscpy(ServiceName, name);

	CStdString const display_name = pOptions->GetOption(OPTION_SERVICE_DISPLAY_NAME);
	if (display_name.size() > 0 && display_name.size() < 256)
		_tcscpy(ServiceDisplayName, display_name);

	delete pOptions;

}

int SetServiceName(LPCTSTR serviceName)
{
	size_t len = _tcslen(serviceName);
	if (!len || len > 256)
		return 1;

	COptions *pOptions = new COptions();
	pOptions->SetOption(OPTION_SERVICE_NAME, serviceName);
	delete pOptions;

	pOptions = new COptions();
	if (pOptions->GetOption(OPTION_SERVICE_NAME) != serviceName) {
		delete pOptions;
		return 1;
	}
	delete pOptions;

	return 0;
}

int SetServiceDisplayName(LPCTSTR serviceDisplayName)
{
	size_t len = _tcslen(serviceDisplayName);
	if (!len || len > 256)
		return 1;

	COptions *pOptions = new COptions();
	pOptions->SetOption(OPTION_SERVICE_DISPLAY_NAME, serviceDisplayName);
	delete pOptions;

	pOptions = new COptions();
	if (pOptions->GetOption(OPTION_SERVICE_DISPLAY_NAME) != serviceDisplayName) {
		delete pOptions;
		return 1;
	}
	delete pOptions;

	return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR	 lpCmdLine,
					 int	   nCmdShow )
{
	BOOL bNT = FALSE;
	BOOL bInstalled = FALSE;
	BOOL dwCurrentState = 0;

	int nAction = 0;
	if (lpCmdLine[0] == '/' || lpCmdLine[0] == '-') {
		lpCmdLine++;
		if (strlen(lpCmdLine) >= 6 && !strncmp(lpCmdLine, "compat", 6)) {
			lpCmdLine += 6;
			while (lpCmdLine[0] == ' ')
				lpCmdLine++;
			return CompatMain(lpCmdLine);
		}
		else if (!strcmp(lpCmdLine, "install"))
			nAction = 1;
		else if (!strcmp(lpCmdLine, "uninstall"))
			nAction = 2;
		else if (!strcmp(lpCmdLine, "start"))
			nAction = 3;
		else if (!strcmp(lpCmdLine, "stop"))
			nAction = 4;
		else if (!strcmp(lpCmdLine, "install auto"))
			nAction = 5;
		else if (strlen(lpCmdLine) >= 10 && !strncmp(lpCmdLine, "adminport ", 10))
			nAction = 6;
		else if (!strcmp(lpCmdLine, "reload-config"))
			nAction = 7;
		else if (!strncmp(lpCmdLine, "servicename ", 12))
			return SetServiceName(CStdString(lpCmdLine + 12));
		else if (!strncmp(lpCmdLine, "servicedisplayname ", 19))
			return SetServiceDisplayName(CStdString(lpCmdLine + 19));
	}

	LoadServiceName();

	if (nAction == 6)
		return SetAdminPort(atoi(lpCmdLine + 10));
	else if (nAction == 7)
		return ReloadConfig();

	SC_HANDLE hService;
	
	{
		Scm scm(SC_MANAGER_CONNECT);
		if (scm) {
			bNT = TRUE;
			hService = OpenService(scm, ServiceName, GENERIC_READ);
			if (hService) {
				bInstalled = TRUE;

				SERVICE_STATUS ServiceStatus;
				if (QueryServiceStatus(hService, &ServiceStatus)) {
					dwCurrentState = ServiceStatus.dwCurrentState;
					if (dwCurrentState == SERVICE_START_PENDING) {
						CloseServiceHandle(hService);

						const SERVICE_TABLE_ENTRY servicetable[] =
						{
							{ServiceName,(LPSERVICE_MAIN_FUNCTION)ServiceMain},
							{NULL,NULL}
						};
						BOOL success;
						success = StartServiceCtrlDispatcher(servicetable);
						if (!success) {
							int nError = GetLastError();
							TCHAR buffer[1000];
							_stprintf(buffer, _T("StartServiceCtrlDispatcher failed with error %d"), nError);
							MessageBox(0, buffer, _T("Error while starting service"), MB_OK);
							//error occured
						}
						return 0;
					}
				}

				CloseServiceHandle(hService);
			}
		}
		else {
			if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
				--lpCmdLine;
				return CompatMain(lpCmdLine);
			}
		}
	}

	if (!bInstalled) {
		if (nAction == 1 || nAction == 5 || (nAction == 0 && MessageBox(0, _T("Install Service?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES)) {
			Scm scm(SC_MANAGER_CREATE_SERVICE);
			if (!scm) {
				return 1;
			}
			int nStartMode = (nAction==5)?SERVICE_AUTO_START:SERVICE_DEMAND_START;
			if (!nAction)
				if (MessageBox(0, _T("Autostart service?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES)
					nStartMode = SERVICE_AUTO_START;
			TCHAR buffer[MAX_PATH + 3];
			buffer[0] = '"';
			DWORD written = GetModuleFileName(0, buffer + 1, MAX_PATH);
			if (!written) {
				MessageBox(0, _T("Failed to get own executable path"), _T("Could not install server"), MB_ICONSTOP);
				return 1;
			}
			buffer[written + 1] = '"';
			buffer[written + 2] = 0;

			hService = CreateService(scm, ServiceName,
				ServiceDisplayName,
				SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS, nStartMode,
				SERVICE_ERROR_NORMAL,
				buffer,
				0, 0, 0, 0, 0);
			if (!hService) {
				return 1;
			}
			CloseServiceHandle(hService);
			dwCurrentState = SERVICE_STOPPED;
		}
		else
			return 0;
	}

	if (dwCurrentState == SERVICE_STOPPED && (nAction==3 || (nAction == 0 && MessageBox(0, _T("Start server?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES))) {
		SC_HANDLE hService;
		Scm scm(SC_MANAGER_ALL_ACCESS);
		if (!scm) {
			return 1;
		}
		hService = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
		if (!hService) {
			return 1;
		}
		StartService(hService, 0, NULL);
		CloseServiceHandle(hService);
		return 0;
	}

	if (dwCurrentState == SERVICE_STOPPED && (nAction==2 || (nAction == 0 && MessageBox(0, _T("Uninstall Service?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES))) {
		SC_HANDLE hService;
		Scm scm(SC_MANAGER_CREATE_SERVICE);
		if (!scm) {
			return 1;
		}
		hService = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
		if (!hService) {
			return 1;
		}
		DeleteService(hService);
		CloseServiceHandle(hService);
		return 0;
	}

	if (dwCurrentState != SERVICE_STOPPED && (nAction==4 || (nAction == 0 && MessageBox(0, _T("Stop Server?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES))) {
		SC_HANDLE hService;
		Scm scm(SC_MANAGER_ALL_ACCESS);
		if (!scm) {
			return 1;
		}
		hService = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
		if (!hService) {
			return 1;
		}
		SERVICE_STATUS status;
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
		CloseServiceHandle(hService);
		return 0;
	}

	if (dwCurrentState == SERVICE_STOPPED)
		return 0;

	return 0;
}

void ServiceMain(DWORD argc, LPTSTR *argv)
{
	LoadServiceName();

	BOOL success;
	nServiceStatusHandle = RegisterServiceCtrlHandler(ServiceName,
		(LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if (!nServiceStatusHandle)
		return;

	success = UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR, 0, 1, 3000);
	if (!success)
		return;

	killServiceEvent = CreateEvent(0, TRUE, FALSE, 0);
	if (killServiceEvent == NULL)
		return;

	success = UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR, 0, 2, 1000);
	if (!success)
		return;

	success = StartServiceThread();
	if (!success)
		return;

	nServiceCurrentStatus = SERVICE_RUNNING;
	success = UpdateServiceStatus(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
	if (!success)
		return;

	WaitForSingleObject(killServiceEvent, INFINITE);
	CloseHandle(killServiceEvent);
	WaitForSingleObject(hServiceThread, INFINITE);
	CloseHandle(hServiceThread);
	UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
}

BOOL UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
					 DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint,
					 DWORD dwWaitHint)
{
	BOOL success;
	SERVICE_STATUS nServiceStatus;
	nServiceStatus.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
	nServiceStatus.dwCurrentState=dwCurrentState;
	if (dwCurrentState == SERVICE_START_PENDING) {
		nServiceStatus.dwControlsAccepted = 0;
	}
	else {
		nServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	}
	if (dwServiceSpecificExitCode == 0) {
		nServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
	}
	else {
		nServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
	}
	nServiceStatus.dwServiceSpecificExitCode = dwServiceSpecificExitCode;
	nServiceStatus.dwCheckPoint = dwCheckPoint;
	nServiceStatus.dwWaitHint = dwWaitHint;

	success=SetServiceStatus(nServiceStatusHandle,&nServiceStatus);
	if (!success) {
		KillService();
		return success;
	}
	else
		return success;
}

BOOL StartServiceThread()
{
	DWORD id;
	hServiceThread=CreateThread(0, 0,
		(LPTHREAD_START_ROUTINE)ServiceExecutionThread,
		0, 0, &id);
	if (hServiceThread == 0) {
		return false;
	}
	else {
		nServiceRunning = true;
		return true;
	}
}

void KillService()
{
	if (hMainWnd)
		PostMessage(hMainWnd, WM_CLOSE, 0, 0);
	nServiceRunning = false;
}


static BOOL CALLBACK SendReloadConfigProc(HWND hwnd, LPARAM lParam)
{
	int* res = (int*)lParam;

	TCHAR buffer[100];

	if (!GetClassName(hwnd, buffer, 100))
		return TRUE;

	if (_tcscmp(buffer, _T("FileZilla Server Helper Window")))
		return TRUE;

	if (!GetWindowText(hwnd, buffer, 100))
		return TRUE;

	if (_tcscmp(buffer, _T("FileZilla Server Helper Window")))
		return TRUE;

	PostMessage(hwnd, WM_FILEZILLA_RELOADCONFIG, 0, 0);

	*res = 0;

	return TRUE;
}


int SendReloadConfig()
{
	int res = 1;
	EnumWindows(SendReloadConfigProc, (LPARAM)&res);

	return res;
}


void ServiceCtrlHandler(DWORD nControlCode)
{
	BOOL success;
	switch(nControlCode) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		nServiceCurrentStatus = SERVICE_STOP_PENDING;
		success=UpdateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 3000);
		KillService();
		return;
	case 128:
		SendReloadConfig();
		break;
	default:
		break;
	}
	UpdateServiceStatus(nServiceCurrentStatus, NO_ERROR, 0, 0, 0);
}

DWORD ServiceExecutionThread(LPDWORD param)
{
	// initialize Winsock library
	BOOL res=TRUE;
	WSADATA wsaData;

	WORD wVersionRequested = MAKEWORD(2, 2);
	int nResult = WSAStartup(wVersionRequested, &wsaData);
	if (nResult != 0)
		res = FALSE;
	else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		res = FALSE;
	}

	if (!res) {
		SetEvent(killServiceEvent);
		UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
		return 0;
	}

	CServer *pServer = new CServer;
	VERIFY(pServer->Create());

	if (!nServiceRunning)
		PostQuitMessage(0);

	MSG msg;
	while (GetMessage(&msg, 0, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	delete pServer;
	WSACleanup();

	SetEvent(killServiceEvent);
	return 0;
}

int SetAdminPort(int nAdminPort)
{
	if (nAdminPort < 1 || nAdminPort > 65535)
		return 1;

	COptions *pOptions = new COptions();
	pOptions->SetOption(OPTION_ADMINPORT, nAdminPort);
	delete pOptions;

	pOptions = new COptions();
	if (pOptions->GetOptionVal(OPTION_ADMINPORT) != nAdminPort) {
		delete pOptions;
		return 1;
	}
	delete pOptions;

	return 0;
}

int ReloadConfig()
{
	int res = SendReloadConfig();

	SC_HANDLE hService;
	Scm scm(SC_MANAGER_CONNECT);

	if (scm) {
		hService = OpenService(scm, ServiceName, SERVICE_USER_DEFINED_CONTROL);
		if (hService) {
			SERVICE_STATUS status;
			res |= ControlService(hService, 128, &status) == 0;
			CloseServiceHandle(hService);
		}
	}

	return !res;
}

int CompatMain(LPCSTR lpCmdLine)
{
	int nAction = 0;

	if (lpCmdLine[0] == '/' || lpCmdLine[0] == '-') {
		lpCmdLine++;
		if (!strcmp(lpCmdLine, "start"))
			nAction = 1;
		else if (!strcmp(lpCmdLine, "stop"))
			nAction = 2;
		else if (strlen(lpCmdLine) >= 10 && !strncmp(lpCmdLine, "adminport ", 10))
			nAction = 3;
		else if (!strcmp(lpCmdLine, "install"))
			return 0;
		else if (!strcmp(lpCmdLine, "uninstall"))
			return 0;
		else if (!strcmp(lpCmdLine, "install auto"))
			return 0;
		else if (!strcmp(lpCmdLine, "reload-config"))
			nAction = 4;
	}

	if (nAction == 3)
		return SetAdminPort(atoi(lpCmdLine + 10));
	else if (nAction == 4)
		return ReloadConfig();

	HWND hWnd = FindWindow(_T("FileZilla Server Helper Window"), _T("FileZilla Server Helper Window"));
	if (nAction == 1 && hWnd)
		return 0;
	else if (nAction==2 && !hWnd)
		return 0;

	if (!hWnd && (nAction == 1 || (nAction == 0 && MessageBox(0, _T("Start Server?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES))) {
		// initialize Winsock library
		BOOL res=TRUE;
		WSADATA wsaData;

		WORD wVersionRequested = MAKEWORD(2, 2);
		int nResult = WSAStartup(wVersionRequested, &wsaData);
		if (nResult != 0)
			res = FALSE;
		else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
			WSACleanup();
			res = FALSE;
		}

		if (!res) {
			return 1;
		}

		CServer *pServer = new CServer;
		VERIFY(pServer->Create());

		MSG msg;
		while (GetMessage(&msg, 0, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		delete pServer;
		WSACleanup();
		return 0;
	}
	else if (hWnd && (nAction == 2 || (nAction == 0 && MessageBox(0, _T("Stop Server?"), _T("Question"), MB_YESNO|MB_ICONQUESTION)==IDYES))) {
		SendMessage(hWnd, WM_CLOSE, 0, 0);
		if (GetLastError())
			return 1;
		else
			return 0;
	}

	return 1;
}