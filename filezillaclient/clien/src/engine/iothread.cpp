#include <filezilla.h>

#include "file.h"
#include "iothread.h"

#include <wx/log.h>

CIOThread::CIOThread()
	: wxThread(wxTHREAD_JOINABLE)
{
	m_buffers[0] = new char[BUFFERSIZE*BUFFERCOUNT];
	for (unsigned int i = 0; i < BUFFERCOUNT; ++i) {
		m_buffers[i] = m_buffers[0] + BUFFERSIZE * i;
		m_bufferLens[i] = 0;
	}
}

CIOThread::~CIOThread()
{
	Close();

	delete [] m_buffers[0];
}

void CIOThread::Close()
{
	if (m_pFile) {
		// The file might have been preallocated and the transfer stopped before being completed
		// so always truncate the file to the actually written size before closing it.
		if (!m_read)
			m_pFile->Truncate();

		m_pFile.reset();
	}
}

bool CIOThread::Create(std::unique_ptr<CFile> && pFile, bool read, bool binary)
{
	wxASSERT(pFile);

	Close();

	m_pFile = std::move(pFile);
	m_read = read;
	m_binary = binary;

	if (read) {
		m_curAppBuf = BUFFERCOUNT - 1;
		m_curThreadBuf = 0;
	}
	else {
		m_curAppBuf = -1;
		m_curThreadBuf = 0;
	}

#ifdef SIMULATE_IO
	size_ = m_pFile->Length();
#endif

	m_running = true;
	wxThread::Create();
	wxThread::Run();

	return true;
}

wxThread::ExitCode CIOThread::Entry()
{
	if (m_read) {
		while (m_running) {
			int len = ReadFromFile(m_buffers[m_curThreadBuf], BUFFERSIZE);

			scoped_lock l(m_mutex);

			if (m_appWaiting) {
				if (!m_evtHandler) {
					m_running = false;
					break;
				}
				m_appWaiting = false;
				m_evtHandler->SendEvent<CIOThreadEvent>();
			}

			if (len == wxInvalidOffset) {
				m_error = true;
				m_running = false;
				break;
			}

			m_bufferLens[m_curThreadBuf] = len;

			if (!len) {
				m_running = false;
				break;
			}

			++m_curThreadBuf %= BUFFERCOUNT;
			if (m_curThreadBuf == m_curAppBuf) {
				if (!m_running)
					break;

				m_threadWaiting = true;
				if (m_running)
					m_condition.wait(l);
			}
		}
	}
	else {
		scoped_lock l(m_mutex);
		while (m_curAppBuf == -1) {
			if (!m_running) {
				return 0;
			}
			else {
				m_threadWaiting = true;
				m_condition.wait(l);
			}
		}

		for (;;) {
			while (m_curThreadBuf == m_curAppBuf) {
				if (!m_running) {
					return 0;
				}
				m_threadWaiting = true;
				m_condition.wait(l);
			}

			l.unlock();
			bool writeSuccessful = WriteToFile(m_buffers[m_curThreadBuf], BUFFERSIZE);
			l.lock();

			if (!writeSuccessful) {
				m_error = true;
				m_running = false;
			}

			if (m_appWaiting) {
				if (!m_evtHandler) {
					m_running = false;
					break;
				}
				m_appWaiting = false;
				m_evtHandler->SendEvent<CIOThreadEvent>();
			}

			if (m_error)
				break;

			++m_curThreadBuf %= BUFFERCOUNT;
		}
	}

	return 0;
}

int CIOThread::GetNextWriteBuffer(char** pBuffer)
{
	wxASSERT(!m_destroyed);

	scoped_lock l(m_mutex);

	if (m_error)
		return IO_Error;

	if (m_curAppBuf == -1) {
		m_curAppBuf = 0;
		*pBuffer = m_buffers[0];
		return IO_Success;
	}

	int newBuf = (m_curAppBuf + 1) % BUFFERCOUNT;
	if (newBuf == m_curThreadBuf) {
		m_appWaiting = true;
		return IO_Again;
	}

	if (m_threadWaiting) {
		m_condition.signal(l);
		m_threadWaiting = false;
	}

	m_curAppBuf = newBuf;
	*pBuffer = m_buffers[newBuf];

	return IO_Success;
}

bool CIOThread::Finalize(int len)
{
	wxASSERT(m_pFile);

	if (m_destroyed)
		return true;

	Destroy();

	if (m_curAppBuf == -1)
		return true;

	if (m_error)
		return false;

	if (!len)
		return true;

	if (!WriteToFile(m_buffers[m_curAppBuf], len))
		return false;

#ifndef __WXMSW__
	if (!m_binary && m_wasCarriageReturn) {
		const char CR = '\r';
		if (m_pFile->Write(&CR, 1) != 1)
			return false;
	}
#endif
	return true;
}

int CIOThread::GetNextReadBuffer(char** pBuffer)
{
	wxASSERT(!m_destroyed);
	wxASSERT(m_read);

	int newBuf = (m_curAppBuf + 1) % BUFFERCOUNT;

	scoped_lock l(m_mutex);

	if (newBuf == m_curThreadBuf) {
		if (m_error)
			return IO_Error;
		else if (!m_running)
			return IO_Success;
		else {
			m_appWaiting = true;
			return IO_Again;
		}
	}

	if (m_threadWaiting) {
		m_condition.signal(l);
		m_threadWaiting = false;
	}

	*pBuffer = m_buffers[newBuf];
	m_curAppBuf = newBuf;

	return m_bufferLens[newBuf];
}

void CIOThread::Destroy()
{
	if (m_destroyed)
		return;
	m_destroyed = true;

	scoped_lock l(m_mutex);

	m_running = false;
	if (m_threadWaiting) {
		m_threadWaiting = false;
		m_condition.signal(l);
	}
	l.unlock();

	Wait(wxTHREAD_WAIT_BLOCK);
}

int CIOThread::ReadFromFile(char* pBuffer, int maxLen)
{
#ifdef SIMULATE_IO
	if (size_ < 0) {
		return 0;
	}
	size_ -= maxLen;
	return maxLen;
#endif

	// In binary mode, no conversion has to be done.
	// Also, under Windows the native newline format is already identical
	// to the newline format of the FTP protocol
#ifndef __WXMSW__
	if (m_binary)
#endif
		return m_pFile->Read(pBuffer, maxLen);

#ifndef __WXMSW__

	// In the worst case, length will doubled: If reading
	// only LFs from the file
	const int readLen = maxLen / 2;

	char* r = pBuffer + readLen;
	int len = m_pFile->Read(r, readLen);
	if (!len || len == wxInvalidOffset)
		return len;

	const char* const end = r + len;
	char* w = pBuffer;

	// Convert all stand-alone LFs into CRLF pairs.
	while (r != end) {
		char c = *r++;
		if (c == '\n') {
			if (!m_wasCarriageReturn)
				*w++ = '\r';
			m_wasCarriageReturn = false;
		}
		else if (c == '\r')
			m_wasCarriageReturn = true;
		else
			m_wasCarriageReturn = false;

		*w++ = c;
	}

	return w - pBuffer;
#endif
}

bool CIOThread::WriteToFile(char* pBuffer, int len)
{
#ifdef SIMULATE_IO
	return true;
#endif
	// In binary mode, no conversion has to be done.
	// Also, under Windows the native newline format is already identical
	// to the newline format of the FTP protocol
#ifndef __WXMSW__
	if (m_binary) {
#endif
		return DoWrite(pBuffer, len);
#ifndef __WXMSW__
	}
	else {
		// On all CRLF pairs, omit the CR. Don't harm stand-alone CRs
		// I assume disk access is buffered, otherwise the 1 byte writes are
		// going to hurt performance.
		const char CR = '\r';
		const char* const end = pBuffer + len;
		for (char* r = pBuffer; r != end; ++r) {
			char c = *r;
			if (c == '\r')
				m_wasCarriageReturn = true;
			else if (c == '\n') {
				m_wasCarriageReturn = false;
				if (!DoWrite(&c, 1))
					return false;
			}
			else {
				if (m_wasCarriageReturn) {
					m_wasCarriageReturn = false;
					if (!DoWrite(&CR, 1))
						return false;
				}

				if (!DoWrite(&c, 1))
					return false;
			}
		}
		return true;
	}
#endif
}

bool CIOThread::DoWrite(const char* pBuffer, int len)
{
	int written = m_pFile->Write(pBuffer, len);
	if (written == len) {
		return true;
	}

	int code = wxSysErrorCode();

	const wxString error = wxSysErrorMsg(code);

	scoped_lock locker(m_mutex);
	m_error_description = error;

	return false;
}

wxString CIOThread::GetError()
{
	scoped_lock locker(m_mutex);
	return m_error_description;
}

void CIOThread::SetEventHandler(CEventHandler* handler)
{
	scoped_lock locker(m_mutex);
	m_evtHandler = handler;
}
