#ifndef __HASHTHREAD_H__
#define __HASHTHREAD_H__

class CServerThread;
class CHashThread final
{
public:
	enum _result
	{
		OK = 0,
		PENDING = 1,
		BUSY = 2,
		FAILURE_OPEN = 4,
		FAILURE_READ = 8,
		FAILURE_MASK = 12
	};

	enum _algorithm
	{
		MD5,
		SHA1,
		SHA512
	};

	CHashThread();
	~CHashThread();

	enum _result Hash(LPCTSTR file, enum _algorithm algorithm, int& id, CServerThread* server_thread);

	enum _result GetResult(int id, CHashThread::_algorithm& alg, CStdString& hash, CStdString& file);

	void Stop(CServerThread* server_thread);

private:
	void DoHash();
	void Loop();

	static DWORD WINAPI ThreadFunc(LPVOID pThis);

	LPTSTR m_filename;
	CServerThread* m_server_thread;

	std::recursive_mutex m_mutex;

	bool m_quit;

	int m_id;
	int m_active_id;
	enum _result m_result;
	char* m_hash;
	enum _algorithm m_algorithm;

	HANDLE m_hThread;
};

#endif
