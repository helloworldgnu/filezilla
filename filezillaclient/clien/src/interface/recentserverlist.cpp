#include <filezilla.h>
#include "recentserverlist.h"
#include "ipcmutex.h"
#include "filezillaapp.h"
#include "Options.h"
#include "xmlfunctions.h"

const std::list<CServer> CRecentServerList::GetMostRecentServers(bool lockMutex /*=true*/)
{
	std::list<CServer> mostRecentServers;

	CInterProcessMutex mutex(MUTEX_MOSTRECENTSERVERS, false);
	if (lockMutex)
		mutex.Lock();

	CXmlFile xmlFile(wxGetApp().GetSettingsFile(_T("recentservers")));
	TiXmlElement* pElement = xmlFile.Load();
	if (!pElement || !(pElement = pElement->FirstChildElement("RecentServers")))
		return mostRecentServers;

	bool modified = false;
	TiXmlElement* pServer = pElement->FirstChildElement("Server");
	while (pServer) {
		CServer server;
		if (!GetServer(pServer, server) || mostRecentServers.size() >= 10) {
			TiXmlElement* pRemove = pServer;
			pServer = pServer->NextSiblingElement("Server");
			pElement->RemoveChild(pRemove);
			modified = true;
		}
		else {
			std::list<CServer>::const_iterator iter;
			for (iter = mostRecentServers.begin(); iter != mostRecentServers.end(); ++iter) {
				if (*iter == server)
					break;
			}
			if (iter == mostRecentServers.end())
				mostRecentServers.push_back(server);
			pServer = pServer->NextSiblingElement("Server");
		}
	}

	if (modified) {
		xmlFile.Save(false);
	}

	return mostRecentServers;
}

void CRecentServerList::SetMostRecentServer(const CServer& server)
{
	CInterProcessMutex mutex(MUTEX_MOSTRECENTSERVERS);

	// Make sure list is initialized
	auto mostRecentServers = GetMostRecentServers(false);

	bool relocated = false;
	for (auto iter = mostRecentServers.begin(); iter != mostRecentServers.end(); ++iter) {
		if (iter->EqualsNoPass(server)) {
			mostRecentServers.erase(iter);
			mostRecentServers.push_front(server);
			relocated = true;
			break;
		}
	}
	if (!relocated) {
		mostRecentServers.push_front(server);
		if (mostRecentServers.size() > 10)
			mostRecentServers.pop_back();
	}

	if (COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) == 2)
		return;

	CXmlFile xmlFile(wxGetApp().GetSettingsFile(_T("recentservers")));
	TiXmlElement* pDocument = xmlFile.CreateEmpty();
	if (!pDocument)
		return;

	TiXmlElement* pElement = pDocument->FirstChildElement("RecentServers");
	if (!pElement)
		pElement = pDocument->LinkEndChild(new TiXmlElement("RecentServers"))->ToElement();

	for (std::list<CServer>::const_iterator iter = mostRecentServers.begin(); iter != mostRecentServers.end(); ++iter) {
		TiXmlElement* pServer = pElement->LinkEndChild(new TiXmlElement("Server"))->ToElement();
		SetServer(pServer, *iter);
	}

	xmlFile.Save(true);
}

void CRecentServerList::Clear()
{
	CInterProcessMutex mutex(MUTEX_MOSTRECENTSERVERS);

	CXmlFile xmlFile(wxGetApp().GetSettingsFile(_T("recentservers")));
	xmlFile.CreateEmpty();
	xmlFile.Save(true);
}
