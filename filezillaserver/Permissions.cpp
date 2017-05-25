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

// Permissions.cpp: Implementierung der Klasse CPermissions.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "misc\md5.h"
#include "Permissions.h"
#include "tinyxml/tinyxml.h"
#include "xml_utils.h"
#include "options.h"
#include "iputils.h"

#include "AsyncSslSocketLayer.h"

class CPermissionsHelperWindow final
{
public:
	CPermissionsHelperWindow(CPermissions *pPermissions)
	{
		ASSERT(pPermissions);
		m_pPermissions = pPermissions;

		//Create window
		WNDCLASSEX wndclass;
		wndclass.cbSize = sizeof wndclass;
		wndclass.style = 0;
		wndclass.lpfnWndProc = WindowProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = GetModuleHandle(0);
		wndclass.hIcon = 0;
		wndclass.hCursor = 0;
		wndclass.hbrBackground = 0;
		wndclass.lpszMenuName = 0;
		wndclass.lpszClassName = _T("CPermissions Helper Window");
		wndclass.hIconSm = 0;

		RegisterClassEx(&wndclass);

		m_hWnd=CreateWindow(_T("CPermissions Helper Window"), _T("CPermissions Helper Window"), 0, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0));
		ASSERT(m_hWnd);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)this);
	};

	~CPermissionsHelperWindow()
	{
		//Destroy window
		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = 0;
		}
	}

	HWND GetHwnd()
	{
		return m_hWnd;
	}

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message==WM_USER)
		{
			/* If receiving WM_USER, update the permission data of the instance with the permission
			 * data from the global data
			 */

			// Get own instance
			CPermissionsHelperWindow *pWnd=(CPermissionsHelperWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (!pWnd)
				return 0;
			if (!pWnd->m_pPermissions)
				return 0;

			pWnd->m_pPermissions->UpdatePermissions(true);
		}
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}

protected:
	CPermissions *m_pPermissions;

private:
	HWND m_hWnd;
};

/////////////////////////////////////////////////////////////////////////////
// CPermissions

std::recursive_mutex CPermissions::m_mutex;
CPermissions::t_UsersList CPermissions::m_sUsersList;
CPermissions::t_GroupsList CPermissions::m_sGroupsList;
std::list<CPermissions *> CPermissions::m_sInstanceList;

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

CPermissions::CPermissions(std::function<void()> const& updateCallback)
	: updateCallback_(updateCallback)
{
	Init();
}

CPermissions::~CPermissions()
{
	simple_lock lock(m_mutex);
	std::list<CPermissions *>::iterator instanceIter;
	for (instanceIter=m_sInstanceList.begin(); instanceIter!=m_sInstanceList.end(); ++instanceIter)
		if (*instanceIter==this)
			break;
	ASSERT(instanceIter != m_sInstanceList.end());
	if (instanceIter != m_sInstanceList.end())
		m_sInstanceList.erase(instanceIter);
	delete m_pPermissionsHelperWindow;
}

void CPermissions::AddLongListingEntry(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char*, bool *)
{
	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (!pTime && GetStatus64(directory.dir, status)) {
		size = GetLength64(status);
		pTime = &status.ftLastWriteTime;
	}

	unsigned int nameLen = strlen(name);

	// This wastes some memory but keeps the whole thing fast
	if (result.empty() || (8192 - result.back().len) < (60 + nameLen)) {
		result.push_back(t_dirlisting());
	}

	if (isDir) {
		memcpy(result.back().buffer + result.back().len, "drwxr-xr-x", 10);
		result.back().len += 10;
	}
	else {
		result.back().buffer[result.back().len++] = '-';
		result.back().buffer[result.back().len++] = directory.bFileRead ? 'r' : '-';
		result.back().buffer[result.back().len++] = directory.bFileWrite ? 'w' : '-';

		BOOL isexe = FALSE;
		if (nameLen > 4) {
			CStdStringA ext = name + nameLen - 4;
			if (ext.ReverseFind('.') != -1) {
				ext.MakeLower();
				if (ext == ".exe")
					isexe = TRUE;
				else if (ext == ".bat")
					isexe = TRUE;
				else if (ext == ".com")
					isexe = TRUE;
			}
		}
		result.back().buffer[result.back().len++] = isexe ? 'x' : '-';
		result.back().buffer[result.back().len++] = directory.bFileRead ? 'r' : '-';
		result.back().buffer[result.back().len++] = '-';
		result.back().buffer[result.back().len++] = isexe ? 'x' : '-';
		result.back().buffer[result.back().len++] = directory.bFileRead ? 'r' : '-';
		result.back().buffer[result.back().len++] = '-';
		result.back().buffer[result.back().len++] = isexe ? 'x' : '-';
	}

	memcpy(result.back().buffer + result.back().len, " 1 ftp ftp ", 11);
	result.back().len += 11;

	result.back().len += sprintf(result.back().buffer + result.back().len, "% 14I64d", size);

	// Adjust time zone info and output file date/time
	SYSTEMTIME sLocalTime;
	GetLocalTime(&sLocalTime);
	FILETIME fTime;
	VERIFY(SystemTimeToFileTime(&sLocalTime, &fTime));

	FILETIME mtime;
	if (pTime)
		mtime = *pTime;
	else
		mtime = fTime;

	TIME_ZONE_INFORMATION tzInfo;
	int tzRes = GetTimeZoneInformation(&tzInfo);
	_int64 offset = tzInfo.Bias+((tzRes==TIME_ZONE_ID_DAYLIGHT)?tzInfo.DaylightBias:tzInfo.StandardBias);
	offset *= 60 * 10000000;

	long long t1 = (static_cast<long long>(mtime.dwHighDateTime) << 32) + mtime.dwLowDateTime;
	t1 -= offset;
	mtime.dwHighDateTime = static_cast<DWORD>(t1 >> 32);
	mtime.dwLowDateTime = static_cast<DWORD>(t1 & 0xFFFFFFFF);

	SYSTEMTIME sFileTime;
	FileTimeToSystemTime(&mtime, &sFileTime);

	long long t2 = (static_cast<long long>(fTime.dwHighDateTime) << 32) + fTime.dwLowDateTime;
	const char months[][4]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	result.back().len += sprintf(result.back().buffer + result.back().len, " %s %02d ", months[sFileTime.wMonth-1], sFileTime.wDay);
	if (t1 > t2 || (t2-t1) > (1000000ll*60*60*24*350))
		result.back().len += sprintf(result.back().buffer + result.back().len, " %d ", sFileTime.wYear);
	else
		result.back().len += sprintf(result.back().buffer + result.back().len, "%02d:%02d ", sFileTime.wHour, sFileTime.wMinute);

	memcpy(result.back().buffer + result.back().len, name, nameLen);
	result.back().len += nameLen;
	result.back().buffer[result.back().len++] = '\r';
	result.back().buffer[result.back().len++] = '\n';
}

void CPermissions::AddFactsListingEntry(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char*, bool *enabledFacts)
{
	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (!pTime && GetStatus64(directory.dir, status)) {
		size = GetLength64(status);
		pTime = &status.ftLastWriteTime;
	}

	unsigned int nameLen = strlen(name);

	// This wastes some memory but keeps the whole thing fast
	if (result.empty() || (8192 - result.back().len) < (76 + nameLen)) {
		result.push_back(t_dirlisting());
	}

	if (!enabledFacts || enabledFacts[0]) {
		if (isDir) {
			memcpy(result.back().buffer + result.back().len, "type=dir;", 9);
			result.back().len += 9;
		}
		else {
			memcpy(result.back().buffer + result.back().len, "type=file;", 10);
			result.back().len += 10;
		}
	}

	// Adjust time zone info and output file date/time
	SYSTEMTIME sLocalTime;
	GetLocalTime(&sLocalTime);
	FILETIME fTime;
	VERIFY(SystemTimeToFileTime(&sLocalTime, &fTime));

	FILETIME mtime;
	if (pTime)
		mtime = *pTime;
	else
		mtime = fTime;

	if (!enabledFacts || enabledFacts[2]) {
		if (mtime.dwHighDateTime || mtime.dwLowDateTime) {
			SYSTEMTIME time;
			FileTimeToSystemTime(&mtime, &time);
			CStdStringA str;
			str.Format("modify=%04d%02d%02d%02d%02d%02d;",
				time.wYear,
				time.wMonth,
				time.wDay,
				time.wHour,
				time.wMinute,
				time.wSecond);

			memcpy(result.back().buffer + result.back().len, str.c_str(), str.GetLength());
			result.back().len += str.GetLength();
		}
	}

	if (!enabledFacts || enabledFacts[1]) {
		if (!isDir)
			result.back().len += sprintf(result.back().buffer + result.back().len, "size=%I64d;", size);
	}

	if (enabledFacts && enabledFacts[fact_perm]) {
		// TODO: a, d,f,p,r,w
		memcpy(result.back().buffer + result.back().len, "perm=", 5);
		result.back().len += 5;
		if (isDir) {
			if (directory.bFileWrite)
				result.back().buffer[result.back().len++] = 'c';
			result.back().buffer[result.back().len++] = 'e';
			if (directory.bDirList)
				result.back().buffer[result.back().len++] = 'l';
			if (directory.bFileDelete || directory.bDirDelete)
				result.back().buffer[result.back().len++] = 'p';
		}
	}

	result.back().len += sprintf(result.back().buffer + result.back().len, " %s\r\n", name);
}

void CPermissions::AddShortListingEntry(std::list<t_dirlisting> &result, bool, const char* name, const t_directory&, __int64, FILETIME*, const char* dirToDisplay, bool *)
{
	unsigned int nameLen = strlen(name);
	unsigned int dirToDisplayLen = strlen(dirToDisplay);

	// This wastes some memory but keeps the whole thing fast
	if (result.empty() || (8192 - result.back().len) < (10 + nameLen + dirToDisplayLen)) {
		result.push_back(t_dirlisting());
	}

	memcpy(result.back().buffer + result.back().len, dirToDisplay, dirToDisplayLen);
	result.back().len += dirToDisplayLen;
	memcpy(result.back().buffer + result.back().len, name, nameLen);
	result.back().len += nameLen;
	result.back().buffer[result.back().len++] = '\r';
	result.back().buffer[result.back().len++] = '\n';
}

bool is8dot3(const CStdString& file)
{
	int i;
	for (i = 0; i < 8; ++i) {
		if (!file[i])
			return true;

		if (file[i] == '.')
			break;
	}
	if (!file[i])
		return true;
	if (file[i++] != '.')
		return false;

	for (int j = 0; j < 3; j++)
	{
		const TCHAR &c = file[i++];
		if (!c)
			return true;
		if (c == '.')
			return false;
	}
	if (file[i])
		return false;

	return true;
}

int CPermissions::GetDirectoryListing(CUser const& user, CStdString currentDir, CStdString dirToDisplay,
									  std::list<t_dirlisting> &result, CStdString& physicalDir,
									  CStdString& logicalDir, void (*addFunc)(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *enabledFacts),
									  bool *enabledFacts /*=0*/)
{
	CStdString dir = CanonifyServerDir(currentDir, dirToDisplay);
	if (dir == _T(""))
		return PERMISSION_INVALIDNAME;
	logicalDir = dir;

	// Get directory from directory name
	t_directory directory;
	BOOL bTruematch;
	int res = GetRealDirectory(dir, user, directory, bTruematch);
	CStdString sFileSpec = _T("*"); // Which files to list in the directory
	if (res == PERMISSION_FILENOTDIR || res == PERMISSION_NOTFOUND) // Try listing using a direct wildcard filespec instead?
	{
		// Check dirToDisplay if we are allowed to go back a directory
		dirToDisplay.Replace('\\', '/');
		while (dirToDisplay.Replace(_T("//"), _T("/")));
		if (dirToDisplay.Right(1) == _T("/"))
			return res;
		int pos = dirToDisplay.ReverseFind('/');
		if (res != PERMISSION_FILENOTDIR && dirToDisplay.Mid(pos + 1).Find('*') == -1)
			return res;
		dirToDisplay = dirToDisplay.Left(pos + 1);

		if (dir == _T("/"))
			return res;

		pos = dir.ReverseFind('/');
		sFileSpec = dir.Mid(pos + 1);
		if (pos)
			dir = dir.Left(pos);
		else
			dir = _T("/");

		if (sFileSpec.Find(_T("*")) == -1 && res != PERMISSION_FILENOTDIR)
			return res;

		res = GetRealDirectory(dir, user, directory, bTruematch);
	}
	if (res)
		return res;

	// Check permissions
	if (!directory.bDirList)
		return PERMISSION_DENIED;

	TIME_ZONE_INFORMATION tzInfo;
	int tzRes = GetTimeZoneInformation(&tzInfo);
	_int64 offset = tzInfo.Bias+((tzRes==TIME_ZONE_ID_DAYLIGHT)?tzInfo.DaylightBias:tzInfo.StandardBias);
	offset *= 60 * 10000000;

	if (dirToDisplay != _T("") && dirToDisplay.Right(1) != _T("/"))
		dirToDisplay += _T("/");

	auto dirToDisplayUTF8 = ConvToNetwork(dirToDisplay);
	if (dirToDisplayUTF8.empty() && !dirToDisplay.empty())
		return PERMISSION_DENIED;

	for (auto const& virtualAliasName : user.virtualAliasNames) {
		if (virtualAliasName.first.CompareNoCase(dir))
			continue;

		t_directory directory;
		BOOL truematch = false;
		if (GetRealDirectory(dir + _T("/") + virtualAliasName.second, user, directory, truematch))
			continue;
		if (!directory.bDirList)
			continue;
		if (!truematch && !directory.bDirSubdirs)
			continue;

		if (sFileSpec != _T("*.*") && sFileSpec != _T("*")) {
			if (!WildcardMatch(virtualAliasName.second, sFileSpec))
				continue;
		}

		auto name = ConvToNetwork(virtualAliasName.second);
		if (!name.empty())
			addFunc(result, true, name.c_str(), directory, 0, 0, dirToDisplayUTF8.c_str(), enabledFacts);
	}

	physicalDir = directory.dir;
	if (sFileSpec != _T("*") && sFileSpec != _T("*.*"))
		physicalDir += sFileSpec;

	WIN32_FIND_DATA FindFileData;
	WIN32_FIND_DATA NextFindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(directory.dir + _T("\\") + sFileSpec, &NextFindFileData);
	while (hFind != INVALID_HANDLE_VALUE)
	{
		FindFileData = NextFindFileData;
		if (!FindNextFile(hFind, &NextFindFileData))
		{
			FindClose(hFind);
			hFind = INVALID_HANDLE_VALUE;
		}

		if (!_tcscmp(FindFileData.cFileName, _T(".")) || !_tcscmp(FindFileData.cFileName, _T("..")))
			continue;

		CStdString const fn = FindFileData.cFileName;

		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// Check permissions of subdir. If we don't have LIST permission,
			// don't display the subdir.
			BOOL truematch;
			t_directory subDir;
			if (GetRealDirectory(dir + _T("/") + fn, user, subDir, truematch))
				continue;

			if (subDir.bDirList) {
				auto utf8 = ConvToNetwork(fn);
				if (utf8.empty() && !fn.empty())
					continue;
				addFunc(result, true, utf8.c_str(), subDir, 0, &FindFileData.ftLastWriteTime, dirToDisplayUTF8.c_str(), enabledFacts);
			}
		}
		else {
			auto utf8 = ConvToNetwork(fn);
			if (utf8.empty() && !fn.empty())
				continue;
			addFunc(result, false, utf8.c_str(), directory, FindFileData.nFileSizeLow + ((_int64)FindFileData.nFileSizeHigh<<32), &FindFileData.ftLastWriteTime, dirToDisplayUTF8.c_str(), enabledFacts);
		}
	}

	return 0;
}

int CPermissions::CheckDirectoryPermissions(CUser const& user, CStdString dirname, CStdString currentdir, int op, CStdString& physicalDir, CStdString& logicalDir)
{
	CStdString dir = CanonifyServerDir(currentdir, dirname);
	if (dir.empty())
		return PERMISSION_INVALIDNAME;
	if (dir == _T("/"))
		return PERMISSION_NOTFOUND;

	int pos = dir.ReverseFind('/');
	if (pos == -1 || !dir[pos + 1])
		return PERMISSION_NOTFOUND;
	logicalDir = dir;
	dirname = dir.Mid(pos + 1);
	if (!pos)
		dir = _T("/");
	else
		dir = dir.Left(pos);

	// dir now is the absolute path (logical server path of course)
	// awhile dirname is the pure dirname without the full path

	CStdString realDir;
	CStdString realDirname;

	//Get the physical path, only of dir to get the right permissions
	t_directory directory;
	BOOL truematch;
	int res;

	CStdString dir2 = dir;
	CStdString dirname2 = dirname;
	do
	{
		res = GetRealDirectory(dir2, user, directory, truematch);
		if (res & PERMISSION_NOTFOUND && op == DOP_CREATE)
		{ //that path could not be found. Maybe more than one directory level has to be created, check that
			if (dir2 == _T("/"))
				return res;

			int pos = dir2.ReverseFind('/');
			if (pos == -1)
				return res;

			dirname2 = dir2.Mid(pos+1) + _T("/") + dirname2;
			if (pos)
				dir2 = dir2.Left(pos);
			else
				dir2 = _T("/");

			continue;
		}
		else if (res)
			return res;

		realDir = directory.dir;
		realDirname = dirname2;
		if (!directory.bDirDelete && op & DOP_DELETE)
			res |= PERMISSION_DENIED;
		if (!directory.bDirCreate && op & DOP_CREATE)
			res |= PERMISSION_DENIED;
		break;
	} while (TRUE);

	realDirname.Replace(_T("/"), _T("\\"));
	physicalDir = realDir + _T("\\") + realDirname;

	//Check if dir + dirname is a valid path
	int res2 = GetRealDirectory(dir + _T("/") + dirname, user, directory, truematch);

	if (!res2)
		physicalDir = directory.dir;

	if (!res2 && op&DOP_CREATE)
		res |= PERMISSION_DOESALREADYEXIST;
	else if (!(res2 & PERMISSION_NOTFOUND)) {
		if (op&DOP_DELETE && user.GetAliasTarget(logicalDir + _T("/")) != _T(""))
			res |= PERMISSION_DENIED;
		return res | res2;
	}

	// check dir attributes
	DWORD nAttributes = GetFileAttributes(physicalDir);
	if (nAttributes==0xFFFFFFFF && !(op&DOP_CREATE))
		res |= PERMISSION_NOTFOUND;
	else if (!(nAttributes&FILE_ATTRIBUTE_DIRECTORY))
		res |= PERMISSION_FILENOTDIR;

	//Finally, a valid path+dirname!
	return res;
}

int CPermissions::CheckFilePermissions(CUser const& user, CStdString filename, CStdString currentdir, int op, CStdString& physicalFile, CStdString& logicalFile)
{
	CStdString dir = CanonifyServerDir(currentdir, filename);
	if (dir.empty())
		return PERMISSION_INVALIDNAME;
	if (dir == _T("/"))
		return PERMISSION_NOTFOUND;

	int pos = dir.ReverseFind('/');
	if (pos == -1)
		return PERMISSION_NOTFOUND;

	logicalFile = dir;

	filename = dir.Mid(pos + 1);
	if (pos)
		dir = dir.Left(pos);
	else
		dir = "/";

	// dir now is the absolute path (logical server path of course)
	// while filename is the filename

	//Get the physical path
	t_directory directory;
	BOOL truematch;
	int res = GetRealDirectory(dir, user, directory, truematch);

	if (res)
		return res;
	if (!directory.bFileRead && op&FOP_READ)
		res |= PERMISSION_DENIED;
	if (!directory.bFileDelete && op&FOP_DELETE)
		res |= PERMISSION_DENIED;
	if (!directory.bFileWrite && op&(FOP_CREATENEW|FOP_WRITE|FOP_APPEND))
		res |= PERMISSION_DENIED;
	if ((!directory.bDirList || (!directory.bDirSubdirs && !truematch)) && op&FOP_LIST)
		res |= PERMISSION_DENIED;
	if (op&FOP_DELETE && user.GetAliasTarget(logicalFile + _T("/")) != _T(""))
		res |= PERMISSION_DENIED;

	physicalFile = directory.dir + "\\" + filename;
	DWORD nAttributes = GetFileAttributes(physicalFile);
	if (nAttributes == 0xFFFFFFFF)
	{
		if (!(op&(FOP_WRITE|FOP_APPEND|FOP_CREATENEW)))
			res |= PERMISSION_NOTFOUND;
	}
	else
	{
		if (nAttributes & FILE_ATTRIBUTE_DIRECTORY)
			res |= PERMISSION_DIRNOTFILE;
		if (!directory.bFileAppend && op & FOP_APPEND)
			res |= PERMISSION_DENIED;
		if (!directory.bFileDelete && op & FOP_WRITE)
			res |= PERMISSION_DENIED;
		if (op & FOP_CREATENEW)
			res |= PERMISSION_DOESALREADYEXIST;
	}

	//If res is 0 we finally have a valid path+filename!
	return res;
}

CStdString CPermissions::GetHomeDir(CUser const& user) const
{
	if (user.homedir.empty()) {
		return CStdString();
	}

	CStdString path = user.homedir;
	user.DoReplacements(path);

	return path;
}

int CPermissions::GetRealDirectory(CStdString directory, const CUser &user, t_directory &ret, BOOL &truematch)
{
	/*
	 * This function translates pathnames from absolute server paths
	 * into absolute local paths.
	 * The given server directory is already an absolute canonified path, so
	 * parsing it is very quick.
	 * To find the absolute local path, we go though each segment of the server
	 * path. For the local path, we start form the homedir and append segments
	 * sequentially or resolve aliases if required.
	 */

	directory.TrimLeft(_T("/"));

	// Split server path
	// --------------------

	//Split dir into pieces
	std::list<CStdString> PathPieces;
	int pos;

	while((pos = directory.Find('/')) != -1) {
		PathPieces.push_back(directory.Left(pos));
		directory = directory.Mid(pos + 1);
	}
	if (directory != _T("")) {
		PathPieces.push_back(directory);
	}

	// Get absolute local path
	// -----------------------

	//First get the home dir
	CStdString homepath = GetHomeDir(user);
	if (homepath.empty()) {
		//No homedir found
		return PERMISSION_DENIED;
	}

	// Reassamble path to get local path
	CStdString path = homepath; // Start with homedir as root

	CStdString virtualPath = _T("/");
	if (PathPieces.empty()) {
		DWORD nAttributes = GetFileAttributes(path);
		if (nAttributes == 0xFFFFFFFF) {
			return PERMISSION_NOTFOUND;
		}
		if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			return PERMISSION_FILENOTDIR;
		}
	}
	else {
		// Go through all pieces
		for (auto const& piece : PathPieces) {
			// Check if piece exists
			virtualPath += piece + _T("/");
			DWORD nAttributes = GetFileAttributes(path + _T("\\") + piece);
			if (nAttributes != 0xFFFFFFFF) {
				if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					return PERMISSION_FILENOTDIR;
				}
				path += _T("\\") + piece;
				continue;
			}
			else {
				// Physical path did not exist, check aliases
				const CStdString& target = user.GetAliasTarget(virtualPath);

				if (!target.empty()) {
					if (target.Right(1) != _T(":")) {
						nAttributes = GetFileAttributes(target);
						if (nAttributes == 0xFFFFFFFF) {
							return PERMISSION_NOTFOUND;
						}
						else if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
							return PERMISSION_FILENOTDIR;
						}
					}
					path = target;
					continue;
				}

			}
			return PERMISSION_NOTFOUND;
		}
	}
	const CStdString realpath = path;

	// Check permissions
	// -----------------

	/* We got a valid local path, now find the closest matching path within the
	 * permissions.
	 * We do this by sequentially comparing the path with all permissions and
	 * sequentially removing the last path segment until we found a match or
	 * all path segments have been removed
	 * Distinguish the case
	 */
	truematch = TRUE;

	while (path != _T("")) {
		BOOL bFoundMatch = FALSE;
		unsigned int i;

		// Check user permissions
		for (i = 0; i < user.permissions.size(); ++i) {
			CStdString permissionPath = user.permissions[i].dir;
			user.DoReplacements(permissionPath);
			if (!permissionPath.CompareNoCase(path)) {
				bFoundMatch = TRUE;
				ret = user.permissions[i];
				break;
			}
		}

		// Check owner (group) permissions
		if (!bFoundMatch && user.pOwner) {
			for (i = 0; i < user.pOwner->permissions.size(); ++i) {
				CStdString permissionPath = user.pOwner->permissions[i].dir;
				user.DoReplacements(permissionPath);
				if (!permissionPath.CompareNoCase(path)) {
					bFoundMatch = TRUE;
					ret = user.pOwner->permissions[i];
					break;
				}
			}
		}

		if (!bFoundMatch) {
			// No match found, remove last segment and try again
			int pos = path.ReverseFind('\\');
			if (pos != -1) {
				path = path.Left(pos);
			}
			else {
				return PERMISSION_DENIED;
			}
			truematch = FALSE;
			continue;
		}
		ret.dir = realpath;

		// We can check the bDirSubdirs permission right here
		if (!truematch && !ret.bDirSubdirs) {
			return PERMISSION_DENIED;
		}

		return 0;
	}
	return PERMISSION_NOTFOUND;
}

int CPermissions::ChangeCurrentDir(CUser const& user, CStdString &currentdir, CStdString &dir)
{
	CStdString canonifiedDir = CanonifyServerDir(currentdir, dir);
	if (canonifiedDir.empty())
		return PERMISSION_INVALIDNAME;
	dir = canonifiedDir;

	//Get the physical path
	t_directory directory;
	BOOL truematch;
	int res = GetRealDirectory(dir, user, directory, truematch);
	if (res)
		return res;
	if (!directory.bDirList)
	{
		if (!directory.bFileRead && !directory.bFileWrite)
			return PERMISSION_DENIED;
	}

	//Finally, a valid path!
	currentdir = dir; //Server paths are relative, so we can use the absolute server path

	return 0;
}

CUser CPermissions::GetUser(CStdString const& username) const
{
	// Get user from username
	auto const& it = m_UsersList.find(username);
	if( it != m_UsersList.end() ) {
		return it->second;
	}
	return CUser();
}

bool CPermissions::CheckUserLogin(CUser const& user, LPCTSTR pass, BOOL noPasswordCheck /*=FALSE*/)
{
	if (user.user.empty())
		return false;

	if (user.password.empty() || noPasswordCheck) {
		return true;
	}

	if (!pass)
		return false;
	auto tmp = ConvToNetwork(pass);
	if (tmp.empty() && *pass) {
		// Network broke. Meh...
		return false;
	}

	if (user.salt.empty()) {
		// It should be an old MD5 hashed password
		if (user.password.size() != MD5_HEX_FORM_LENGTH) {
			// But it isn't...
			return false;
		}

		MD5 md5;
		md5.update((unsigned char const*)tmp.c_str(), tmp.size());
		md5.finalize();
		char *res = md5.hex_digest();
		CStdString hash = res;
		delete[] res;

		return hash == user.password;
	}
	
	// It's a salted SHA-512 hash

	auto saltedPassword = ConvToNetwork(pass + user.salt);
	if (saltedPassword.empty()) {
		return false;
	}

	CAsyncSslSocketLayer ssl(0);
	CStdString hash = ssl.SHA512(reinterpret_cast<unsigned char const*>(saltedPassword.c_str()), saltedPassword.size());

	return !hash.empty() && user.password == hash;
}

void CPermissions::UpdateInstances()
{
	simple_lock lock(m_mutex);
	for (std::list<CPermissions *>::iterator iter=m_sInstanceList.begin(); iter!=m_sInstanceList.end(); ++iter) {
		if (*iter != this) {
			ASSERT((*iter)->m_pPermissionsHelperWindow);
			::PostMessage((*iter)->m_pPermissionsHelperWindow->GetHwnd(), WM_USER, 0, 0);
		}
	}
}

void CPermissions::SetKey(TiXmlElement *pXML, LPCTSTR name, LPCTSTR value)
{
	ASSERT(pXML);
	TiXmlElement* pOption = new TiXmlElement("Option");
	pOption->SetAttribute("Name", ConvToNetwork(name).c_str());
	XML::SetText(pOption, value);
	pXML->LinkEndChild(pOption);
}

void CPermissions::SetKey(TiXmlElement *pXML, LPCTSTR name, int value)
{
	ASSERT(pXML);
	CStdString str;
	str.Format(_T("%d"), value);
	SetKey(pXML, name, str);
}

void CPermissions::SavePermissions(TiXmlElement *pXML, const t_group &user)
{
	TiXmlElement* pPermissions = new TiXmlElement("Permissions");
	pXML->LinkEndChild(pPermissions);

	for (unsigned int i=0; i < user.permissions.size(); i++)
	{
		TiXmlElement* pPermission = new TiXmlElement("Permission");
		pPermissions->LinkEndChild(pPermission);

		pPermission->SetAttribute("Dir", ConvToNetwork(user.permissions[i].dir).c_str());
		if (!user.permissions[i].aliases.empty()) {
			TiXmlElement* pAliases = new TiXmlElement("Aliases");
			pPermission->LinkEndChild(pAliases);
			for (auto const& alias : user.permissions[i].aliases) {
				TiXmlElement *pAlias = new TiXmlElement("Alias");
				XML::SetText(pAlias, alias);
				pAliases->LinkEndChild(pAlias);
			}
		}
		SetKey(pPermission, _T("FileRead"), user.permissions[i].bFileRead ? _T("1"):_T("0"));
		SetKey(pPermission, _T("FileWrite"), user.permissions[i].bFileWrite ? _T("1"):_T("0"));
		SetKey(pPermission, _T("FileDelete"), user.permissions[i].bFileDelete ?_T("1"):_T("0"));
		SetKey(pPermission, _T("FileAppend"), user.permissions[i].bFileAppend ? _T("1"):_T("0"));
		SetKey(pPermission, _T("DirCreate"), user.permissions[i].bDirCreate ? _T("1"):_T("0"));
		SetKey(pPermission, _T("DirDelete"), user.permissions[i].bDirDelete ? _T("1"):_T("0"));
		SetKey(pPermission, _T("DirList"), user.permissions[i].bDirList ? _T("1"):_T("0"));
		SetKey(pPermission, _T("DirSubdirs"), user.permissions[i].bDirSubdirs ? _T("1"):_T("0"));
		SetKey(pPermission, _T("IsHome"), user.permissions[i].bIsHome ? _T("1"):_T("0"));
		SetKey(pPermission, _T("AutoCreate"), user.permissions[i].bAutoCreate ? _T("1"):_T("0"));
	}
}

bool CPermissions::GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength)
{
	// This function returns all account data as a command string which will be
	// sent to the user interface.
	if (!pBuffer || !nBufferLength) {
		return false;
	}

	simple_lock lock(m_mutex);

	// First calculate the required buffer length
	DWORD len = 3 * 2;
	if (m_sGroupsList.size() > 0xffffff || m_sUsersList.size() > 0xffffff) {
		return false;
	}
	for (auto const& group : m_sGroupsList) {
		len += group.GetRequiredBufferLen();
	}
	for (auto const& iter : m_sUsersList) {
		len += iter.second.GetRequiredBufferLen();
	}

	// Allocate memory
	*pBuffer = new unsigned char[len];
	unsigned char* p  = *pBuffer;

	// Write groups to buffer
	*p++ = ((m_sGroupsList.size() / 256) / 256) & 255;
	*p++ = (m_sGroupsList.size() / 256) % 256;
	*p++ = m_sGroupsList.size() % 256;
	for (auto const& group : m_sGroupsList) {
		p = group.FillBuffer(p);
		if (!p) {
			delete [] *pBuffer;
			*pBuffer = NULL;
			return FALSE;
		}
	}

	// Write users to buffer
	*p++ = ((m_sUsersList.size() / 256) / 256) & 255;
	*p++ = (m_sUsersList.size() / 256) % 256;
	*p++ = m_sUsersList.size() % 256;
	for (auto const& iter : m_sUsersList ) {
		p = iter.second.FillBuffer(p);
		if (!p) {
			delete [] *pBuffer;
			*pBuffer = NULL;
			return FALSE;
		}
	}

	*nBufferLength = len;

	return TRUE;
}

BOOL CPermissions::ParseUsersCommand(unsigned char *pData, DWORD dwDataLength)
{
	t_GroupsList groupsList;
	t_UsersList usersList;

	unsigned char *p = pData;
	unsigned char* endMarker = pData + dwDataLength;

	if (dwDataLength < 3)
		return FALSE;
	int num = *p * 256 * 256 + p[1] * 256 + p[2];
	p += 3;

	int i;
	for (i = 0; i < num; ++i) {
		t_group group;
		p = group.ParseBuffer(p, endMarker - p);
		if (!p)
			return FALSE;

		if (group.group != _T("")) {
			//Set a home dir if no home dir could be read
			BOOL bGotHome = FALSE;
			for (unsigned int dir = 0; dir < group.permissions.size(); dir++)
				if (group.permissions[dir].bIsHome)
				{
					bGotHome = TRUE;
					break;
				}

			if (!bGotHome && !group.permissions.empty())
				group.permissions.begin()->bIsHome = TRUE;

			groupsList.push_back(group);
		}
	}

	if ((endMarker - p) < 3)
		return FALSE;
	num = *p * 256 * 256 + p[1] * 256 + p[2];
	p += 3;

	for (i = 0; i < num; ++i) {
		CUser user;

		p = user.ParseBuffer(p, endMarker - p);
		if (!p)
			return FALSE;

		if (user.user != _T("")) {
			user.pOwner = NULL;
			if (user.group != _T("")) {
				for (auto const& group : groupsList) {
					if (group.group == user.group) {
						user.pOwner = &group;
						break;
					}
				}
				if (!user.pOwner)
					user.group = _T("");
			}

			if (!user.pOwner) {
				//Set a home dir if no home dir could be read
				BOOL bGotHome = FALSE;
				for (unsigned int dir = 0; dir < user.permissions.size(); dir++)
					if (user.permissions[dir].bIsHome)
					{
						bGotHome = TRUE;
						break;
					}

				if (!bGotHome && !user.permissions.empty())
					user.permissions.begin()->bIsHome = TRUE;
			}

			std::vector<t_directory>::iterator iter;
			for (iter = user.permissions.begin(); iter != user.permissions.end(); iter++)
			{
				if (iter->bIsHome)
				{
					user.homedir = iter->dir;
					break;
				}
			}
			if (user.homedir == _T("") && user.pOwner) {
				for (auto const& perm : user.pOwner->permissions ) {
					if (perm.bIsHome) {
						user.homedir = perm.dir;
						break;
					}
				}
			}

			user.PrepareAliasMap();

			CStdString name = user.user;
			name.ToLower();
			usersList[name] = user;
		}
	}

	// Update the account list
	{
		simple_lock lock(m_mutex);
		m_sGroupsList = groupsList;
		m_sUsersList = usersList;
	}
	UpdatePermissions(true);
	UpdateInstances();

	return SaveSettings();
}

bool CPermissions::SaveSettings()
{
	// Write the new account data into xml file

	TiXmlElement *pXML = COptions::GetXML();
	if (!pXML)
		return false;

	TiXmlElement* pGroups;
	while ((pGroups = pXML->FirstChildElement("Groups")))
		pXML->RemoveChild(pGroups);
	pGroups = new TiXmlElement("Groups");
	pXML->LinkEndChild(pGroups);

	//Save the changed user details
	for (t_GroupsList::const_iterator groupiter = m_GroupsList.begin(); groupiter != m_GroupsList.end(); groupiter++) {
		TiXmlElement* pGroup = new TiXmlElement("Group");
		pGroups->LinkEndChild(pGroup);

		pGroup->SetAttribute("Name", ConvToNetwork(groupiter->group).c_str());

		SetKey(pGroup, _T("Bypass server userlimit"), groupiter->nBypassUserLimit);
		SetKey(pGroup, _T("User Limit"), groupiter->nUserLimit);
		SetKey(pGroup, _T("IP Limit"), groupiter->nIpLimit);
		SetKey(pGroup, _T("Enabled"), groupiter->nEnabled);
		SetKey(pGroup, _T("Comments"), groupiter->comment);
		SetKey(pGroup, _T("ForceSsl"), groupiter->forceSsl);

		SaveIpFilter(pGroup, *groupiter);
		SavePermissions(pGroup, *groupiter);
		SaveSpeedLimits(pGroup, *groupiter);
	}

	TiXmlElement* pUsers;
	while ((pUsers = pXML->FirstChildElement("Users")))
		pXML->RemoveChild(pUsers);
	pUsers = new TiXmlElement("Users");
	pXML->LinkEndChild(pUsers);

	//Save the changed user details
	for (auto const& iter : m_UsersList) {
		CUser const& user = iter.second;
		TiXmlElement* pUser = new TiXmlElement("User");
		pUsers->LinkEndChild(pUser);

		pUser->SetAttribute("Name", ConvToNetwork(user.user).c_str());

		SetKey(pUser, _T("Pass"), user.password);
		SetKey(pUser, _T("Salt"), user.salt);
		SetKey(pUser, _T("Group"), user.group);
		SetKey(pUser, _T("Bypass server userlimit"), user.nBypassUserLimit);
		SetKey(pUser, _T("User Limit"), user.nUserLimit);
		SetKey(pUser, _T("IP Limit"), user.nIpLimit);
		SetKey(pUser, _T("Enabled"), user.nEnabled);
		SetKey(pUser, _T("Comments"), user.comment);
		SetKey(pUser, _T("ForceSsl"), user.forceSsl);

		SaveIpFilter(pUser, user);
		SavePermissions(pUser, user);
		SaveSpeedLimits(pUser, user);
	}
	if (!COptions::FreeXML(pXML, true))
		return false;

	return true;
}

bool CPermissions::Init()
{
	simple_lock lock(m_mutex);
	m_pPermissionsHelperWindow = new CPermissionsHelperWindow(this);
	if (m_sInstanceList.empty() && m_sUsersList.empty() && m_sGroupsList.empty()) {
		// It's the first time Init gets called after application start, read
		// permissions from xml file.
		ReadSettings();
	}
	UpdatePermissions(false);

	std::list<CPermissions *>::iterator instanceIter;
	for (instanceIter = m_sInstanceList.begin(); instanceIter != m_sInstanceList.end(); instanceIter++)
		if (*instanceIter == this)
			break;
	if (instanceIter == m_sInstanceList.end())
		m_sInstanceList.push_back(this);

	return TRUE;
}

void CPermissions::ReadPermissions(TiXmlElement *pXML, t_group &user, BOOL &bGotHome)
{
	bGotHome = FALSE;
	for (TiXmlElement* pPermissions = pXML->FirstChildElement("Permissions"); pPermissions; pPermissions = pPermissions->NextSiblingElement("Permissions"))
	{
		for (TiXmlElement* pPermission = pPermissions->FirstChildElement("Permission"); pPermission; pPermission = pPermission->NextSiblingElement("Permission"))
		{
			t_directory dir;
			dir.dir = ConvFromNetwork(pPermission->Attribute("Dir"));
			dir.dir.Replace('/', '\\');
			dir.dir.TrimRight('\\');
			if (dir.dir == _T(""))
				continue;

			for (TiXmlElement* pAliases = pPermission->FirstChildElement("Aliases"); pAliases; pAliases = pAliases->NextSiblingElement("Aliases")) {
				for (TiXmlElement* pAlias = pAliases->FirstChildElement("Alias"); pAlias; pAlias = pAlias->NextSiblingElement("Alias"))	{
					CStdString alias = XML::ReadText(pAlias);
					if (alias == _T("") || alias[0] != '/')
						continue;

					alias.Replace(_T("\\"), _T("/"));
					while (alias.Replace(_T("//"), _T("/")));
					alias.TrimRight('/');
					if (alias != _T("") && alias != _T("/"))
						dir.aliases.push_back(alias);
				}
			}
			for (TiXmlElement* pOption = pPermission->FirstChildElement("Option"); pOption; pOption = pOption->NextSiblingElement("Option")) {
				CStdString name = ConvFromNetwork(pOption->Attribute("Name"));
				CStdString value = XML::ReadText(pOption);

				if (name == _T("FileRead"))
					dir.bFileRead = value == _T("1");
				else if (name == _T("FileWrite"))
					dir.bFileWrite = value == _T("1");
				else if (name == _T("FileDelete"))
					dir.bFileDelete = value == _T("1");
				else if (name == _T("FileAppend"))
					dir.bFileAppend = value == _T("1");
				else if (name == _T("DirCreate"))
					dir.bDirCreate = value == _T("1");
				else if (name == _T("DirDelete"))
					dir.bDirDelete = value == _T("1");
				else if (name == _T("DirList"))
					dir.bDirList = value == _T("1");
				else if (name == _T("DirSubdirs"))
					dir.bDirSubdirs = value == _T("1");
				else if (name == _T("IsHome"))
					dir.bIsHome = value == _T("1");
				else if (name == _T("AutoCreate"))
					dir.bAutoCreate = value == _T("1");
			}

			//Avoid multiple home dirs
			if (dir.bIsHome)
				if (!bGotHome)
					bGotHome = TRUE;
				else
					dir.bIsHome = FALSE;

			if (user.permissions.size() < 20000)
				user.permissions.push_back(dir);
		}
	}
}

void CPermissions::AutoCreateDirs(CUser const& user)
{
	// Create missing directores after a user has logged on
	for (auto const& permission : user.permissions) {
		if (permission.bAutoCreate) {
			CStdString dir = permission.dir;
			user.DoReplacements(dir);

			dir += _T("\\");
			CStdString str;
			while (dir != _T("")) {
				int pos = dir.Find(_T("\\"));
				CStdString piece = dir.Left(pos + 1);
				dir = dir.Mid(pos + 1);

				str += piece;
				CreateDirectory(str, 0);
			}
		}
	}
	if (user.pOwner) {
		for (auto const& permission : user.pOwner->permissions) {
			if (permission.bAutoCreate)
			{
				CStdString dir = permission.dir;
				user.DoReplacements(dir);

				dir += _T("\\");
				CStdString str;
				while (dir != _T("")) {
					int pos = dir.Find(_T("\\"));
					CStdString piece = dir.Left(pos + 1);
					dir = dir.Mid(pos + 1);

					str += piece;
					CreateDirectory(str, 0);
				}
			}
		}
	}
}

void CPermissions::ReadSpeedLimits(TiXmlElement *pXML, t_group &group)
{
	const CStdString prefixes[] = { _T("Dl"), _T("Ul") };
	const char* names[] = { "Download", "Upload" };

	for (TiXmlElement* pSpeedLimits = pXML->FirstChildElement("SpeedLimits"); pSpeedLimits; pSpeedLimits = pSpeedLimits->NextSiblingElement("SpeedLimits"))
	{
		CStdString str;
		int n;

		for (int i = 0; i < 2; i++)
		{
			str = pSpeedLimits->Attribute(ConvToNetwork(prefixes[i] + _T("Type")).c_str());
			n = _ttoi(str);
			if (n >= 0 && n < 4)
				group.nSpeedLimitType[i] = n;
			str = pSpeedLimits->Attribute(ConvToNetwork(prefixes[i] + _T("Limit")).c_str());
			n = _ttoi(str);
			if (n < 0)
				group.nSpeedLimit[i] = 0;
			else if (n > 1048576)
				group.nSpeedLimit[i] = 1048576;
			else
				group.nSpeedLimit[i] = n;

			str = pSpeedLimits->Attribute(ConvToNetwork(_T("Server") + prefixes[i] + _T("LimitBypass")).c_str());
			n = _ttoi(str);
			if (n >= 0 && n < 4)
				group.nBypassServerSpeedLimit[i] = n;

			for (TiXmlElement* pLimit = pSpeedLimits->FirstChildElement(names[i]); pLimit; pLimit = pLimit->NextSiblingElement(names[i]))
			{
				for (TiXmlElement* pRule = pLimit->FirstChildElement("Rule"); pRule; pRule = pRule->NextSiblingElement("Rule"))
				{
					CSpeedLimit limit;
					if (!limit.Load(pRule))
						continue;

					if (group.SpeedLimits[i].size() < 20000)
						group.SpeedLimits[i].push_back(limit);
				}
			}
		}
	}
}

void CPermissions::SaveSpeedLimits(TiXmlElement *pXML, const t_group &group)
{
	TiXmlElement* pSpeedLimits = pXML->LinkEndChild(new TiXmlElement("SpeedLimits"))->ToElement();

	CStdString str;

	const CStdString prefixes[] = { _T("Dl"), _T("Ul") };
	const char* names[] = { "Download", "Upload" };

	for (int i = 0; i < 2; ++i) {
		pSpeedLimits->SetAttribute(ConvToNetwork(prefixes[i] + _T("Type")).c_str(), group.nSpeedLimitType[i]);
		pSpeedLimits->SetAttribute(ConvToNetwork(prefixes[i] + _T("Limit")).c_str(), group.nSpeedLimit[i]);
		pSpeedLimits->SetAttribute(ConvToNetwork(_T("Server") + prefixes[i] + _T("LimitBypass")).c_str(), group.nBypassServerSpeedLimit[i]);

		TiXmlElement* pSpeedLimit = new TiXmlElement(names[i]);
		pSpeedLimits->LinkEndChild(pSpeedLimit);

		for (auto const& limit : group.SpeedLimits[i]) {
			TiXmlElement* pRule = pSpeedLimit->LinkEndChild(new TiXmlElement("Rule"))->ToElement();
			limit.Save(pRule);
		}
	}
}

void CPermissions::ReloadConfig()
{
	ReadSettings();
	UpdatePermissions(true);
	UpdateInstances();
}

void CPermissions::ReadIpFilter(TiXmlElement *pXML, t_group &group)
{
	for (TiXmlElement* pFilter = pXML->FirstChildElement("IpFilter"); pFilter; pFilter = pFilter->NextSiblingElement("IpFilter")) {
		for (TiXmlElement* pDisallowed = pFilter->FirstChildElement("Disallowed"); pDisallowed; pDisallowed = pDisallowed->NextSiblingElement("Disallowed")) {
			for (TiXmlElement* pIP = pDisallowed->FirstChildElement("IP"); pIP; pIP = pIP->NextSiblingElement("IP")) {
				CStdString ip = XML::ReadText(pIP);
				if (ip == _T("")) {
					continue;
				}

				if (group.disallowedIPs.size() >= 30000) {
					break;
				}

				if (ip == _T("*")) {
					group.disallowedIPs.push_back(ip);
				}
				else {
					if (IsValidAddressFilter(ip)) {
						group.disallowedIPs.push_back(ip);
					}
				}
			}
		}
		for (TiXmlElement* pAllowed = pFilter->FirstChildElement("Allowed"); pAllowed; pAllowed = pAllowed->NextSiblingElement("Allowed")) {
			for (TiXmlElement* pIP = pAllowed->FirstChildElement("IP"); pIP; pIP = pIP->NextSiblingElement("IP")) {
				CStdString ip = XML::ReadText(pIP);
				if (ip == _T("")) {
					continue;
				}

				if (group.allowedIPs.size() >= 30000) {
					break;
				}

				if (ip == _T("*")) {
					group.allowedIPs.push_back(ip);
				}
				else {
					if (IsValidAddressFilter(ip)) {
						group.allowedIPs.push_back(ip);
					}
				}
			}
		}
	}
}

void CPermissions::SaveIpFilter(TiXmlElement *pXML, const t_group &group)
{
	TiXmlElement* pFilter = pXML->LinkEndChild(new TiXmlElement("IpFilter"))->ToElement();

	TiXmlElement* pDisallowed = pFilter->LinkEndChild(new TiXmlElement("Disallowed"))->ToElement();

	for (auto const& disallowedIP : group.disallowedIPs) {
		TiXmlElement* pIP = pDisallowed->LinkEndChild(new TiXmlElement("IP"))->ToElement();
		XML::SetText(pIP, disallowedIP);
	}

	TiXmlElement* pAllowed = pFilter->LinkEndChild(new TiXmlElement("Allowed"))->ToElement();

	for (auto const& allowedIP : group.allowedIPs) {
		TiXmlElement* pIP = pAllowed->LinkEndChild(new TiXmlElement("IP"))->ToElement();
		XML::SetText(pIP, allowedIP);
	}
}

CStdString CPermissions::CanonifyServerDir(CStdString currentDir, CStdString newDir) const
{
	/*
	 * CanonifyPath takes the current and the new server dir as parameter,
	 * concats the paths if neccessary and canonifies the dir:
	 * - remove dot-segments
	 * - convert backslashes into slashes
	 * - remove double slashes
	 */

	if (newDir == _T(""))
		return currentDir;

	// Make segment separators pretty
	newDir.Replace(_T("\\"), _T("/"));
	while (newDir.Replace(_T("//"), _T("/")));

	if (newDir == _T("/"))
		return newDir;

	// This list will hold the individual path segments
	std::list<CStdString> piecelist;

	/*
	 * Check the type of the path: Absolute or relative?
	 * On relative paths, use currentDir as base, else use
	 * only dir.
	 */
	if (newDir.Left(1) != _T("/"))
	{
		// New relative path, split currentDir and add it to the piece list.
		currentDir.TrimLeft(_T("/"));
		int pos;
		while((pos = currentDir.Find(_T("/"))) != -1)
		{
			piecelist.push_back(currentDir.Left(pos));
			currentDir = currentDir.Mid(pos + 1);
		}
		if (currentDir != _T(""))
			piecelist.push_back(currentDir);
	}

	/*
	 * Now split up the new dir into individual segments. Here we
	 * check for dot segments and remove the proper number of segments
	 * from the piece list on dots.
	 */

	int pos;
	newDir.TrimLeft(_T("/"));
	if (newDir.Right(1) != _T("/"))
		newDir += _T("/");
	while ((pos = newDir.Find(_T("/"))) != -1)
	{
		CStdString piece = newDir.Left(pos);
		newDir = newDir.Mid(pos + 1);

		if (piece == _T(""))
			continue;

		bool allDots = true;
		int dotCount = 0;
		for (int i = 0; i < piece.GetLength(); i++)
			if (piece[i] != '.')
			{
				allDots = false;
				break;
			}
			else
				dotCount++;

		if (allDots)
		{
			while (--dotCount)
			{
				if (!piecelist.empty())
					piecelist.pop_back();
			}
		}
		else
			piecelist.push_back(piece);
	}

	// Reassemble the directory
	CStdString result;

	if (piecelist.empty())
		return _T("/");

	// List of reserved filenames which may not be used on a Windows system
	static LPCTSTR reservedNames[] = {	_T("CON"),	_T("PRN"),	_T("AUX"),	_T("CLOCK$"), _T("NUL"),
										_T("COM1"), _T("COM2"), _T("COM3"), _T("COM4"), _T("COM5"),
										_T("COM6"), _T("COM7"), _T("COM8"), _T("COM9"),
										_T("LPT1"), _T("LPT2"), _T("LPT3"), _T("LPT4"), _T("LPT5"),
										_T("LPT6"), _T("LPT7"), _T("LPT8"), _T("LPT9"),
										_T("") };

	for (std::list<CStdString>::iterator iter = piecelist.begin(); iter != piecelist.end(); iter++)
	{
		// Check for reserved filenames
		CStdString piece = *iter;
		piece.MakeUpper();
		for (LPCTSTR *reserved = reservedNames; **reserved; reserved++)
		{
			if (piece == *reserved)
				return _T("");
		}
		int pos = piece.Find(_T("."));
		if (pos >= 3)
		{
			piece = piece.Left(pos);
			for (LPCTSTR *reserved = reservedNames; **reserved; reserved++)
			{
				if (piece == *reserved)
					return _T("");
			}
		}

		result += _T("/") + *iter;
	}

	// Now dir is the canonified absolute server path.
	return result;
}

int CPermissions::GetFact(CUser const& user, CStdString const& currentDir, CStdString file, CStdString& fact, CStdString& logicalName, bool enabledFacts[3])
{
	CStdString dir = CanonifyServerDir(currentDir, file);
	if (dir.empty())
		return PERMISSION_INVALIDNAME;
	logicalName = dir;

	t_directory directory;
	BOOL bTruematch;
	int res = GetRealDirectory(dir, user, directory, bTruematch);
	if (res == PERMISSION_FILENOTDIR) {
		if (dir == _T("/"))
			return res;

		int pos = dir.ReverseFind('/');
		if (pos == -1)
			return res;

		CStdString dir2;
		if (pos)
			dir2 = dir.Left(pos);
		else
			dir2 = _T("/");

		CStdString fn = dir.Mid(pos + 1);
		int res = GetRealDirectory(dir2, user, directory, bTruematch);
		if (res)
			return res | PERMISSION_FILENOTDIR;

		if (!directory.bFileRead)
			return PERMISSION_DENIED;

		file = directory.dir + _T("\\") + fn;

		if (enabledFacts[0])
			fact = _T("type=file;");
		else
			fact = _T("");
	}
	else if (res)
		return res;
	else {
		if (!directory.bDirList)
			return PERMISSION_DENIED;

		if (!bTruematch && !directory.bDirSubdirs)
			return PERMISSION_DENIED;

		file = directory.dir;

		if (enabledFacts[0])
			fact = _T("type=dir;");
		else
			fact = _T("");
	}

	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (GetStatus64(file, status)) {
		if (enabledFacts[1] && !(status.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			CStdString str;
			str.Format(_T("size=%I64d;"), GetLength64(status));
			fact += str;
		}

		if (enabledFacts[2]) {
			// Get last modification time
			FILETIME ftime = status.ftLastWriteTime;
			if (ftime.dwHighDateTime || ftime.dwLowDateTime) {
				SYSTEMTIME time;
				FileTimeToSystemTime(&ftime, &time);
				CStdString str;
				str.Format(_T("modify=%04d%02d%02d%02d%02d%02d;"),
					time.wYear,
					time.wMonth,
					time.wDay,
					time.wHour,
					time.wMinute,
					time.wSecond);

				fact += str;
			}
		}
	}

	fact += _T(" ") + logicalName;

	return 0;
}

void CUser::PrepareAliasMap()
{
	/*
	 * Prepare the alias map.
	 * For fast access, aliases are stored as key/value pairs.
	 * The key is the folder part of the alias.
	 * The value is a structure containing the name of the alias
	 * and the target folder.
	 * Example:
	 * Shared folder c:\myfolder, alias /myotherfolder/myalias
	 * Key: /myotherfolder, Value = myalias, c:\myfolder
	 */

	virtualAliases.clear();
	for (auto const& permission : permissions) {
		for (auto alias : permission.aliases) {
			DoReplacements(alias);

			if (alias[0] != '/') {
				continue;
			}

			int pos = alias.ReverseFind('/');
			CStdString dir = alias.Left(pos);
			if (dir == _T(""))
				dir = _T("/");
			virtualAliasNames.insert(std::pair<CStdString, CStdString>(dir, alias.Mid(pos + 1)));
			virtualAliases[alias + _T("/")] = permission.dir;
			DoReplacements(virtualAliases[alias + _T("/")]);
		}
	}

	if (!pOwner)
		return;

	for (auto const& permission : pOwner->permissions) {
		for (auto alias : permission.aliases) {
			DoReplacements(alias);

			if (alias[0] != '/') {
				continue;
			}

			int pos = alias.ReverseFind('/');
			CStdString dir = alias.Left(pos);
			if (dir == _T(""))
				dir = _T("/");
			virtualAliasNames.insert(std::pair<CStdString, CStdString>(dir, alias.Mid(pos + 1)));
			virtualAliases[alias + _T("/")] = permission.dir;
			DoReplacements(virtualAliases[alias + _T("/")]);
		}
	}
}

CStdString CUser::GetAliasTarget(CStdString const& virtualPath) const
{
	// Find the target for the alias with the specified path and name
	for (auto const& alias : virtualAliases) {
		if (!alias.first.CompareNoCase(virtualPath))
			return alias.second;
	}

	return CStdString();
}

void CPermissions::ReadSettings()
{
	TiXmlElement *pXML = COptions::GetXML();
	bool fileIsDirty = false; //By default, nothing gets changed when reading the file
	if (!pXML)
		return;

	simple_lock lock(m_mutex);

	m_sGroupsList.clear();
	m_sUsersList.clear();

	TiXmlElement* pGroups = pXML->FirstChildElement("Groups");
	if (!pGroups)
		pGroups = pXML->LinkEndChild(new TiXmlElement("Groups"))->ToElement();

	for (TiXmlElement* pGroup = pGroups->FirstChildElement("Group"); pGroup; pGroup = pGroup->NextSiblingElement("Group")) {
		t_group group;
		group.nBypassUserLimit = 2;
		group.group = ConvFromNetwork(pGroup->Attribute("Name"));
		if (group.group == _T(""))
			continue;

		for (TiXmlElement* pOption = pGroup->FirstChildElement("Option"); pOption; pOption = pOption->NextSiblingElement("Option")) {
			CStdString name = ConvFromNetwork(pOption->Attribute("Name"));
			CStdString value = XML::ReadText(pOption);

			if (name == _T("Bypass server userlimit"))
				group.nBypassUserLimit = _ttoi(value);
			else if (name == _T("User Limit"))
				group.nUserLimit = _ttoi(value);
			else if (name == _T("IP Limit"))
				group.nIpLimit = _ttoi(value);
			else if (name == _T("Enabled"))
				group.nEnabled = _ttoi(value);
			else if (name == _T("Comments"))
				group.comment = value;
			else if (name == _T("ForceSsl"))
				group.forceSsl = _ttoi(value);
		}
		if (group.nUserLimit < 0 || group.nUserLimit > 999999999)
			group.nUserLimit = 0;
		if (group.nIpLimit < 0 || group.nIpLimit > 999999999)
			group.nIpLimit = 0;

		ReadIpFilter(pGroup, group);

		BOOL bGotHome = FALSE;
		ReadPermissions(pGroup, group, bGotHome);
		//Set a home dir if no home dir could be read
		if (!bGotHome && !group.permissions.empty())
			group.permissions.begin()->bIsHome = TRUE;

		ReadSpeedLimits(pGroup, group);

		if (m_sGroupsList.size() < 200000)
			m_sGroupsList.push_back(group);
	}

	TiXmlElement* pUsers = pXML->FirstChildElement("Users");
	if (!pUsers)
		pUsers = pXML->LinkEndChild(new TiXmlElement("Users"))->ToElement();

	for (TiXmlElement* pUser = pUsers->FirstChildElement("User"); pUser; pUser = pUser->NextSiblingElement("User")) {
		CUser user;
		user.nBypassUserLimit = 2;
		user.user = ConvFromNetwork(pUser->Attribute("Name"));
		if (user.user == _T("")) {
			continue;
		}

		for (TiXmlElement* pOption = pUser->FirstChildElement("Option"); pOption; pOption = pOption->NextSiblingElement("Option")) {
			CStdString name = ConvFromNetwork(pOption->Attribute("Name"));
			CStdString value = XML::ReadText(pOption);

			if (name == _T("Pass")) {
				user.password = value;
			}
			else if (name == _T("Salt")) {
				user.salt = value;
			}
			else if (name == _T("Bypass server userlimit"))
				user.nBypassUserLimit = _ttoi(value);
			else if (name == _T("User Limit"))
				user.nUserLimit = _ttoi(value);
			else if (name == _T("IP Limit"))
				user.nIpLimit = _ttoi(value);
			else if (name == _T("Group"))
				user.group = value;
			else if (name == _T("Enabled"))
				user.nEnabled = _ttoi(value);
			else if (name == _T("Comments"))
				user.comment = value;
			else if (name == _T("ForceSsl"))
				user.forceSsl = _ttoi(value);
		}

		// If provided password is not a salted SHA512 hash and neither an old MD5 hash, convert it into a salted SHA512 hash
		if (!user.password.empty() && user.salt.empty() && user.password.size() != MD5_HEX_FORM_LENGTH) {
			user.generateSalt();

			auto saltedPassword = ConvToNetwork(user.password + user.salt);
			if (saltedPassword.empty()) {
				// We skip this user
				continue;
			}

			CAsyncSslSocketLayer ssl(0);
			CStdString hash = ssl.SHA512(reinterpret_cast<unsigned char const*>(saltedPassword.c_str()), saltedPassword.size());

			if (hash.empty()) {
				// We skip this user
				continue;
			}

			user.password = hash;

			fileIsDirty = true;
		}

		if (user.nUserLimit < 0 || user.nUserLimit > 999999999)
			user.nUserLimit = 0;
		if (user.nIpLimit < 0 || user.nIpLimit > 999999999)
			user.nIpLimit = 0;

		if (user.group != _T("")) {
			for (auto const& group : m_sGroupsList) {
				if (group.group == user.group) {
					user.pOwner = &group;
					break;
				}
			}

			if (!user.pOwner)
				user.group = _T("");
		}

		ReadIpFilter(pUser, user);

		BOOL bGotHome = FALSE;
		ReadPermissions(pUser, user, bGotHome);
		user.PrepareAliasMap();

		//Set a home dir if no home dir could be read
		if (!bGotHome && !user.pOwner) {
			if (!user.permissions.empty())
				user.permissions.begin()->bIsHome = TRUE;
		}

		std::vector<t_directory>::iterator iter;
		for (iter = user.permissions.begin(); iter != user.permissions.end(); iter++) {
			if (iter->bIsHome) {
				user.homedir = iter->dir;
				break;
			}
		}
		if (user.homedir == _T("") && user.pOwner) {
			for (auto const& perm : user.pOwner->permissions ) {
				if (perm.bIsHome) {
					user.homedir = perm.dir;
					break;
				}
			}
		}

		ReadSpeedLimits(pUser, user);

		if (m_sUsersList.size() < 200000) {
			CStdString name = user.user;
			name.ToLower();
			m_sUsersList[name] = user;
		}
	}
	COptions::FreeXML(pXML, false);

	UpdatePermissions(false);

	if (fileIsDirty) {
		SaveSettings();
	}
}

// Replace :u and :g (if a group it exists)
void CUser::DoReplacements(CStdString& path) const
{
	path.Replace(_T(":u"), user);
	path.Replace(_T(":U"), user);
	if (group != _T(""))
	{
		path.Replace(_T(":g"), group);
		path.Replace(_T(":G"), group);
	}
}

bool CPermissions::WildcardMatch(CStdString string, CStdString pattern) const
{
	if (pattern == _T("*") || pattern == _T("*.*"))
		return true;

	// Do a really primitive wildcard check, does even ignore ?
	string.MakeLower();
	pattern.MakeLower();

	bool starFirst = false;
	while (pattern != _T(""))
	{
		int pos = pattern.Find('*');
		if (pos == -1)
		{
			if (starFirst)
			{
				if (string.GetLength() > pattern.GetLength())
					string = string.Right(pattern.GetLength());
			}
			if (pattern != string)
				return false;
			else
				return true;
		}
		else if (!pos)
		{
			starFirst = true;
			pattern = pattern.Mid(1);
		}
		else
		{
			int npos = string.Find(pattern.Left(pos));
			if (npos == -1)
				return false;
			if (npos && !starFirst)
				return false;
			pattern = pattern.Mid(pos + 1);
			string = string.Mid(npos + pos);

			starFirst = true;
		}
	}
	return true;
}

void CPermissions::UpdatePermissions(bool notifyOwner)
{
	{
		simple_lock lock(m_mutex);

		// Clear old group data and copy over the new data
		m_GroupsList.clear();
		for (auto const& group : m_sGroupsList ) {
			m_GroupsList.push_back(group);
		}

		// Clear old user data and copy over the new data
		m_UsersList.clear();
		for (auto const& it : m_sUsersList ) {
			CUser user = it.second;
			user.pOwner = NULL;
			if (user.group != _T("")) {	// Set owner
				for (auto const& group : m_GroupsList ) {
					if (group.group == user.group) {
						user.pOwner = &group;
						break;
					}
				}
			}
			m_UsersList[it.first] = user;
		}
	}

	if (notifyOwner && updateCallback_) {
		updateCallback_();
	}
}
