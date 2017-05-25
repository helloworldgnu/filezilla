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
#include "autobanmanager.h"
#include "Options.h"

int CAutoBanManager::m_refCount = 0;
std::map<CStdString, time_t> CAutoBanManager::m_banMap;
std::map<CStdString, CAutoBanManager::t_attemptInfo> CAutoBanManager::m_attemptMap;

std::recursive_mutex CAutoBanManager::m_mutex;

CAutoBanManager::CAutoBanManager(COptions* pOptions)
	: m_pOptions(pOptions)
{
	simple_lock lock(m_mutex);
	m_refCount++;
}

CAutoBanManager::~CAutoBanManager()
{
	simple_lock lock(m_mutex);
	m_refCount--;
	if (!m_refCount) {
		m_banMap.clear();
		m_attemptMap.clear();
	}
}

bool CAutoBanManager::IsBanned(const CStdString& ip)
{
	bool enabled = m_pOptions->GetOptionVal(OPTION_AUTOBAN_ENABLE) != 0;
	if (!enabled)
		return false;

	simple_lock lock(m_mutex);
	return m_banMap.find(ip) != m_banMap.end();
}

bool CAutoBanManager::RegisterAttempt(const CStdString& ip)
{
	bool enabled = m_pOptions->GetOptionVal(OPTION_AUTOBAN_ENABLE) != 0;
	if (!enabled)
		return false;

	const int maxAttempts = (int)m_pOptions->GetOptionVal(OPTION_AUTOBAN_ATTEMPTS);
	const int banType = (int)m_pOptions->GetOptionVal(OPTION_AUTOBAN_TYPE);

	simple_lock lock(m_mutex);
	if (m_banMap.find(ip) != m_banMap.end()) {
		return true;
	}

	std::map<CStdString, t_attemptInfo>::iterator iter = m_attemptMap.find(ip);
	if (iter == m_attemptMap.end()) {
		t_attemptInfo info;
		info.attempts = 1;
		info.time = time(0);
		m_attemptMap[ip] = info;
	}
	else {
		if (++iter->second.attempts >= maxAttempts) {
			m_attemptMap.erase(iter);

			if (!banType)
				m_banMap[ip] = time(0);
			else {
				// TODO
			}
			return true;
		}
		else
			iter->second.time = time(0);
	}

	return false;
}

void CAutoBanManager::PurgeOutdated()
{
	const int banTime = (int)m_pOptions->GetOptionVal(OPTION_AUTOBAN_BANTIME) * 60 * 60;

	time_t now = time(0);

	simple_lock lock(m_mutex);
	std::map<CStdString, time_t>::iterator iter = m_banMap.begin();
	while (iter != m_banMap.end()) {
		const time_t diff = now - iter->second;
		if (diff > banTime) {
			std::map<CStdString, time_t>::iterator remove = iter++;
			m_banMap.erase(remove);
		}
		else
			iter++;
	}

	{
		std::map<CStdString, t_attemptInfo>::iterator iter = m_attemptMap.begin();
		while (iter != m_attemptMap.end()) {
			const time_t diff = now - iter->second.time;
			if (diff > banTime) {
				std::map<CStdString, t_attemptInfo>::iterator remove = iter++;
				m_attemptMap.erase(remove);
			}
			else
				iter++;
		}
	}
}
