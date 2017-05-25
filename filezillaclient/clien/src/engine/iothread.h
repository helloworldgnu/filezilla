#ifndef __IOTHREAD_H__
#define __IOTHREAD_H__

#include <wx/file.h>
#include "event_loop.h"

#define BUFFERCOUNT 5
#define BUFFERSIZE 128*1024

// Does not actually read from or write to file
// Useful for benchmarks to avoid IO bottleneck
// skewing results
//#define SIMULATE_IO

struct io_thread_event_type{};
typedef CEvent<io_thread_event_type> CIOThreadEvent;

enum IORet
{
	IO_Success = 0,
	IO_Error = -2,
	IO_Again = -1
};

class CFile;
class CIOThread final : protected wxThread
{
public:
	CIOThread();
	virtual ~CIOThread();

	bool Create(std::unique_ptr<CFile> && pFile, bool read, bool binary);
	virtual void Destroy(); // Only call that might be blocking

	// Call before first call to one of the GetNext*Buffer functions
	// This handler will receive the CIOThreadEvent events. The events
	// get triggerd iff a buffer is available after a call to the
	// GetNext*Buffer functions returned IO_Again
	void SetEventHandler(CEventHandler* handler);

	// Gets next buffer
	// Return value:  IO_Success on EOF
	//                IO_Again if it would block
	//                IO_Error on error
	//                buffersize else
	int GetNextReadBuffer(char** pBuffer);

	// Gets next write buffer
	// Return value: IO_Again if it would block
	//               IO_Error on error
	//               IO_Success else
	int GetNextWriteBuffer(char** pBuffer);

	bool Finalize(int len);

	wxString GetError();

protected:
	void Close();

	virtual ExitCode Entry();

	int ReadFromFile(char* pBuffer, int maxLen);
	bool WriteToFile(char* pBuffer, int len);
	bool DoWrite(const char* pBuffer, int len);

	CEventHandler* m_evtHandler{};

	bool m_read{};
	bool m_binary{};
	std::unique_ptr<CFile> m_pFile;

	char* m_buffers[BUFFERCOUNT];
	unsigned int m_bufferLens[BUFFERCOUNT];

	mutex m_mutex;
	condition m_condition;

	int m_curAppBuf{};
	int m_curThreadBuf{};

	bool m_error{};
	bool m_running{};
	bool m_threadWaiting{};
	bool m_appWaiting{};

	bool m_destroyed{};

	bool m_wasCarriageReturn{};

	wxString m_error_description;

#ifdef SIMULATE_IO
	wxFileOffset size_{};
#endif
};

#endif //__IOTHREAD_H__
