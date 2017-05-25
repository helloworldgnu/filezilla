#include "StdAfx.h"
#include "hash_thread.h"
#include "misc\md5.h"
#include "ServerThread.h"

#define SHA512_STANDALONE
typedef unsigned int uint32;
#include "hash_algorithms/int64.h"
#include "hash_algorithms/sshsh512.c"
#include "hash_algorithms/sshsha.c"

CHashThread::CHashThread()
{
	m_filename = 0;
	m_quit = false;
	m_id = 0;
	m_active_id = 0;
	m_result = OK;
	m_hash = 0;
	m_server_thread = 0;
	m_algorithm = SHA512;

	m_hThread = CreateThread(0, 0, &CHashThread::ThreadFunc, this, 0, 0);
}

CHashThread::~CHashThread()
{
	{
		simple_lock lock(m_mutex);
		m_quit = true;
		m_server_thread = 0;
		delete [] m_filename;
		delete [] m_hash;
	}

	WaitForSingleObject(m_hThread, INFINITE);

	CloseHandle(m_hThread);
}

DWORD CHashThread::ThreadFunc(LPVOID pThis)
{
	((CHashThread*)pThis)->Loop();

	return 0;
}

namespace {
char* toHex(unsigned char* buffer, unsigned int len)
{
	char* hex = new char[len * 2 + 1];
	hex[len * 2] = 0;

	for (unsigned int i = 0; i < len; i++)
	{
		unsigned char const l = buffer[i] >> 4;
		unsigned char const r = buffer[i] & 0x0F;

		if (l > 9)
			hex[i * 2] = 'a' + l - 10;
		else
			hex[i * 2] = '0' + l;

		if (r > 9)
			hex[i * 2 + 1] = 'a' + r - 10;
		else
			hex[i * 2 + 1] = '0' + r;
	}

	return hex;
}

void FreeState(void* data, CHashThread::_algorithm alg)
{
	switch (alg)
	{
	case CHashThread::MD5:
		delete static_cast<::MD5*>(data);
		break;
	case CHashThread::SHA512:
		delete static_cast<SHA512_State*>(data);
		break;
	case CHashThread::SHA1:
		delete static_cast<SHA_State*>(data);
		break;
	}
}
}

void CHashThread::DoHash()
{
	if (!m_filename)
		return;

	LPCTSTR file = m_filename;
	m_filename = 0;

	m_mutex.unlock();

	int shareMode = FILE_SHARE_READ;
	HANDLE hFile = CreateFile(file, GENERIC_READ, shareMode, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
	delete [] file;

	if (hFile == INVALID_HANDLE_VALUE) {
		m_mutex.lock();
		m_result = FAILURE_OPEN;
		if (m_server_thread)
			m_server_thread->PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_HASHRESULT, m_active_id);
		return;
	}

	const enum _algorithm alg = m_algorithm;

	void* data;
	switch (alg)
	{
	case MD5:
		data = new ::MD5;
		break;
	case SHA512:
		data = new SHA512_State;
		SHA512_Init((SHA512_State*)data);
		break;
	case SHA1:
		data = new SHA_State;
		SHA_Init((SHA_State*)data);
		break;
	}

	unsigned char buffer[262144];
	DWORD read = 0;
	BOOL res = 0;
	while ((res = ReadFile(hFile, buffer, 262144, &read, 0) != 0) && read > 0)
	{
		switch (alg)
		{
		case MD5:
			((::MD5*)data)->update(buffer, read);
			break;
		case SHA512:
			SHA512_Bytes((SHA512_State*)data, buffer, read);
			break;
		case SHA1:
			SHA_Bytes((SHA_State*)data, buffer, read);
			break;
		}

		m_mutex.lock();
		if (!m_server_thread) {
			CloseHandle(hFile);
			FreeState(data, alg);
			return;
		}
		m_mutex.unlock();
	}

	CloseHandle(hFile);

	m_mutex.lock();
	if (!res) {
		m_result = FAILURE_READ;
		if (m_server_thread)
			m_server_thread->PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_HASHRESULT, m_active_id);
		FreeState(data, alg);
		return;
	}

	switch (alg)
	{
	case MD5:
		((::MD5*)data)->finalize();
		m_hash = ((::MD5*)data)->hex_digest();
		break;
	case SHA512:
		{
			unsigned char digest[64];
			SHA512_Final((SHA512_State*)data, digest);
			m_hash = toHex(digest, 64);
		}
		break;
	case SHA1:
		{
			unsigned char digest[20];
			SHA_Final((SHA_State*)data, digest);
			m_hash = toHex(digest, 20);
		}
		break;
	}
	m_result = m_hash ? OK : FAILURE_READ;
	if (m_server_thread)
		m_server_thread->PostThreadMessage(WM_FILEZILLA_THREADMSG, FTM_HASHRESULT, m_active_id);

	FreeState(data, alg);
}

void CHashThread::Loop()
{
	m_mutex.lock();
	while (!m_quit) {
		DoHash();
		m_mutex.unlock();
		Sleep(100);
		m_mutex.lock();
	}
	m_mutex.unlock();
}

enum CHashThread::_result CHashThread::Hash(LPCTSTR file, enum _algorithm algorithm, int& id, CServerThread* server_thread)
{
	simple_lock lock(m_mutex);
	if (m_active_id) {
		return BUSY;
	}

	++m_id;
	if (m_id > 1000000)
		m_id = 1;
	id = m_id;
	m_active_id = id;

	delete [] m_hash;
	m_hash = 0;

	delete [] m_filename;
	m_filename = new TCHAR[_tcslen(file) + 1];
	_tcscpy(m_filename, file);

	m_server_thread = server_thread;
	m_algorithm = algorithm;

	m_result = PENDING;

	return PENDING;
}

enum CHashThread::_result CHashThread::GetResult(int id, CHashThread::_algorithm& alg, CStdString& hash, CStdString& file)
{
	if (id <= 0)
		return FAILURE_MASK;

	simple_lock lock(m_mutex);

	if (id != m_active_id) {
		return BUSY;
	}

	if (m_result == PENDING) {
		return PENDING;
	}

	alg = m_algorithm;
	file = m_filename;

	delete [] m_filename;
	m_filename = 0;

	m_active_id = 0;

	if (m_result == OK) {
		hash = m_hash;
		delete [] m_hash;
		m_hash = 0;
		return OK;
	}

	return m_result;
}

void CHashThread::Stop(CServerThread* server_thread)
{
	simple_lock lock(m_mutex);
	if (m_server_thread == server_thread)
		m_server_thread = 0;
}
