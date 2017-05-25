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

#include "MFC64bitFix.h"

/*__int64 GetLength64(CFile &file)
{
	DWORD low;
	DWORD high;
	low=GetFileSize((void *)file.m_hFile, &high);
	_int64 size=((_int64)high<<32)+low;
	return size;
}*/

_int64 GetLength64(LPCTSTR filename)
{
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if (!GetStatus64(filename, data) ) {
		return -1;
	}

	return GetLength64(data);
}

_int64 GetLength64(WIN32_FILE_ATTRIBUTE_DATA & data)
{
	return (static_cast<_int64>(data.nFileSizeHigh) << 32) + data.nFileSizeLow;
}

bool GetStatus64(LPCTSTR lpszFileName, WIN32_FILE_ATTRIBUTE_DATA & rStatus)
{
	if (!GetFileAttributesEx(lpszFileName, GetFileExInfoStandard, &rStatus)) {
		return false;
	}

	if (!rStatus.ftCreationTime.dwHighDateTime && !rStatus.ftCreationTime.dwLowDateTime) {
		rStatus.ftCreationTime = rStatus.ftLastWriteTime;
	}
	else if (!rStatus.ftLastWriteTime.dwHighDateTime && !rStatus.ftLastWriteTime.dwLowDateTime) {
		rStatus.ftLastWriteTime = rStatus.ftCreationTime;
	}

	if (!rStatus.ftLastAccessTime.dwHighDateTime && !rStatus.ftLastAccessTime.dwLowDateTime) {
		rStatus.ftLastAccessTime = rStatus.ftLastWriteTime;
	}

	return true;
}

_int64 GetPosition64(HANDLE hFile)
{
	if (!hFile || hFile == INVALID_HANDLE_VALUE)
		return -1;
	LONG low = 0;
	LONG high = 0;
	low = SetFilePointer(hFile, low, &high, FILE_CURRENT);
	if (low == 0xFFFFFFFF && GetLastError() != NO_ERROR)
		return -1;
	return ((_int64)high<<32)+low;
}
