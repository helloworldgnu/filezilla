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

#ifndef __AUTOBANMANAGER_H__
#define __AUTOBANMANAGER_H__

class COptions;
class CAutoBanManager final
{
public:

	CAutoBanManager(COptions* pOptions);
	~CAutoBanManager();

	void PurgeOutdated();

	bool IsBanned(const CStdString& ip);

	// Returns true if address got banned
	bool RegisterAttempt(const CStdString& ip);

protected:

	static int m_refCount;

	struct t_attemptInfo
	{
		int attempts;
		time_t time;
	};

	static std::map<CStdString, time_t> m_banMap;
	static std::map<CStdString, t_attemptInfo> m_attemptMap;

	static std::recursive_mutex m_mutex;

	COptions* m_pOptions;
};

#endif //__AUTOBANMANAGER_H__
