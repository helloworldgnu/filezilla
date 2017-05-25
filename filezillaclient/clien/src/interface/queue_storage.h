#ifndef __QUEUE_STORAGE_H__
#define __QUEUE_STORAGE_H__

#include <vector>

class CFileItem;
class CServerItem;
class CServer;

class CQueueStorage
{
	class Impl;

public:
	CQueueStorage();
	virtual ~CQueueStorage();

	CQueueStorage(CQueueStorage const&) = delete;
	CQueueStorage& operator=(CQueueStorage const&) = delete;

	// Call before loading
	bool BeginTransaction();

	// Call after finishing loading
	bool EndTransaction();

	bool Clear(); // Also clears caches

	bool Vacuum();

	bool SaveQueue(std::vector<CServerItem*> const& queue);

	// > 0 = server id
	//   0 = No server
	// < 0 = failure.
	int64_t GetServer(CServer& server, bool fromBeginning);
	CServer GetNextServer();

	int64_t GetFile(CFileItem** pItem, int64_t server);

	static wxString GetDatabaseFilename();

private:
	Impl* d_;
};

#endif //__QUEUE_STORAGE_H__
