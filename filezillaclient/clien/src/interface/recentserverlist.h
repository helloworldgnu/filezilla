#ifndef __RECENTSERVERLIST_H__
#define __RECENTSERVERLIST_H__

#include "xmlfunctions.h"

class CRecentServerList
{
public:
	static void SetMostRecentServer(const CServer& server);
	static const std::list<CServer> GetMostRecentServers(bool lockMutex = true);
	static void Clear();
};

#endif //__RECENTSERVERLIST_H__
