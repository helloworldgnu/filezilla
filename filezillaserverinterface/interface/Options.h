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

#ifndef FILEZILLA_SERVER_INTERFACE_OPTIONS_HEADER
#define FILEZILLA_SERVER_INTERFACE_OPTIONS_HEADER

#define IOPTION_STARTMINIMIZED 1
#define IOPTION_LASTSERVERADDRESS 2
#define IOPTION_LASTSERVERPORT 3
#define IOPTION_LASTSERVERPASS 4
#define IOPTION_ALWAYS 5
#define IOPTION_USERSORTING 6
#define IOPTION_FILENAMEDISPLAY 7
#define IOPTION_RECONNECTCOUNT 8

#define IOPTIONS_NUM 8

class COptionsDlg;
class COptions final
{
	friend COptionsDlg;

public:
	CString GetOption(int nOptionID);
	__int64 GetOptionVal(int nOptionID);
	void SetOption(int nOptionID, CString const& value);
	void SetOption(int nOptionID, __int64 value);

protected:
	CString GetFileName(bool for_saving);

	void SaveOption(int nOptionID, CString const& value);

	static bool IsNumeric(LPCTSTR str);

	struct t_OptionsCache final
	{
		BOOL bCached{};
		CTime createtime;
		int nType{};
		CString str;
		_int64 value{};
	} m_OptionsCache[IOPTIONS_NUM];
	void Init();
	static BOOL m_bInitialized;
};

#endif
