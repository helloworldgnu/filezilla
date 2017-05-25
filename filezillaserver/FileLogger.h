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

#if !defined(AFX_FILELOGGER_H__FDF4A6C8_5A47_40FE_8D82_804E0DCCE3FE__INCLUDED_)
#define AFX_FILELOGGER_H__FDF4A6C8_5A47_40FE_8D82_804E0DCCE3FE__INCLUDED_

class COptions;
class CFileLogger final
{
public:
	BOOL CheckLogFile();
	BOOL Log(LPCTSTR msg);
	CFileLogger(COptions *pOptions);
	~CFileLogger();
protected:
	LPTSTR m_pFileName;
	COptions *m_pOptions;
	HANDLE m_hLogFile;
};

#endif // !defined(AFX_FILELOGGER_H__FDF4A6C8_5A47_40FE_8D82_804E0DCCE3FE__INCLUDED_)
