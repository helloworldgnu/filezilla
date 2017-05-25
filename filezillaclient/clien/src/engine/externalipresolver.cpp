#include <filezilla.h>
#include "externalipresolver.h"
#include "socket.h"
#include "misc.h"

#include <wx/regex.h>

namespace {
mutex s_sync;
wxString ip;
bool checked = false;
}

CExternalIPResolver::CExternalIPResolver(CEventHandler & handler)
	: CEventHandler(handler.event_loop_)
	, m_handler(&handler)
{
	ResetHttpData(true);
}

CExternalIPResolver::~CExternalIPResolver()
{
	delete [] m_pSendBuffer;
	m_pSendBuffer = 0;
	delete [] m_pRecvBuffer;
	m_pRecvBuffer = 0;

	delete m_pSocket;
	m_pSocket = 0;
}

void CExternalIPResolver::GetExternalIP(const wxString& address, CSocket::address_family protocol, bool force /*=false*/)
{
	{
		scoped_lock l(s_sync);
		if (checked) {
			if (force)
				checked = false;
			else {
				m_done = true;
				return;
			}
		}
	}

	m_address = address;
	m_protocol = protocol;

	wxString host;
	int pos;
	if ((pos = address.Find(_T("://"))) != -1)
		host = address.Mid(pos + 3);
	else
		host = address;

	if ((pos = host.Find(_T("/"))) != -1)
		host = host.Left(pos);

	wxString hostWithPort = host;

	if ((pos = host.Find(':', true)) != -1) {
		wxString port = host.Mid(pos + 1);
		if (!port.ToULong(&m_port) || m_port < 1 || m_port > 65535)
			m_port = 80;
		host = host.Left(pos);
	}
	else
		m_port = 80;

	if (host.empty()) {
		m_done = true;
		return;
	}

	m_pSocket = new CSocket(this);

	int res = m_pSocket->Connect(host, m_port, protocol);
	if (res && res != EINPROGRESS) {
		Close(false);
		return;
	}

	wxString buffer = wxString::Format(_T("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n"), address, hostWithPort, wxString(PACKAGE_STRING, wxConvLocal));
	m_pSendBuffer = new char[strlen(buffer.mb_str()) + 1];
	strcpy(m_pSendBuffer, buffer.mb_str());
}

void CExternalIPResolver::operator()(CEventBase const& ev)
{
	Dispatch<CSocketEvent>(ev, this, &CExternalIPResolver::OnSocketEvent);
}

void CExternalIPResolver::OnSocketEvent(CSocketEventSource*, SocketEventType t, int error)
{
	if (!m_pSocket)
		return;

	switch (t)
	{
	case SocketEventType::read:
		OnReceive();
		break;
	case SocketEventType::connection:
		OnConnect(error);
		break;
	case SocketEventType::close:
		OnClose();
		break;
	case SocketEventType::write:
		OnSend();
		break;
	default:
		break;
	}

}

void CExternalIPResolver::OnConnect(int error)
{
	if (error)
		Close(false);
}

void CExternalIPResolver::OnClose()
{
	if (m_data.empty())
		Close(false);
	else
		OnData(0, 0);
}

void CExternalIPResolver::OnReceive()
{
	if (!m_pRecvBuffer) {
		m_pRecvBuffer = new char[m_recvBufferLen];
		m_recvBufferPos = 0;
	}

	if (m_pSendBuffer)
		return;

	while (m_pSocket) {
		unsigned int len = m_recvBufferLen - m_recvBufferPos;
		int error;
		int read = m_pSocket->Read(m_pRecvBuffer + m_recvBufferPos, len, error);
		if (read == -1) {
			if (error != EAGAIN)
				Close(false);
			return;
		}

		if (!read) {
			Close(false);
			return;
		}

		if (m_finished) {
			// Just ignore all further data
			m_recvBufferPos = 0;
			return;
		}

		m_recvBufferPos += read;

		if (!m_gotHeader)
			OnHeader();
		else {
			if (m_transferEncoding == chunked)
				OnChunkedData();
			else
				OnData(m_pRecvBuffer, m_recvBufferPos);
		}
	}
}

void CExternalIPResolver::OnSend()
{
	while (m_pSendBuffer) {
		unsigned int len = strlen(m_pSendBuffer + m_sendBufferPos);
		int error;
		int written = m_pSocket->Write(m_pSendBuffer + m_sendBufferPos, len, error);
		if (written == -1) {
			if (error != EAGAIN)
				Close(false);
			return;
		}

		if (!written) {
			Close(false);
			return;
		}

		if (written == (int)len) {
			delete [] m_pSendBuffer;
			m_pSendBuffer = 0;

			OnReceive();
		}
		else
			m_sendBufferPos += written;
	}
}

void CExternalIPResolver::Close(bool successful)
{
	delete [] m_pSendBuffer;
	m_pSendBuffer = 0;

	delete [] m_pRecvBuffer;
	m_pRecvBuffer = 0;

	delete m_pSocket;
	m_pSocket = 0;

	if (m_done)
		return;

	m_done = true;

	{
		scoped_lock l(s_sync);
		if (!successful) {
			ip.clear();
		}
		checked = true;
	}

	if (m_handler) {
		m_handler->SendEvent<CExternalIPResolveEvent>();
		m_handler = 0;
	}
}

void CExternalIPResolver::OnHeader()
{
	// Parse the HTTP header.
	// We do just the neccessary parsing and silently ignore most header fields
	// Redirects are supported though if the server sends the Location field.

	for (;;) {
		// Find line ending
		unsigned int i = 0;
		for (i = 0; (i + 1) < m_recvBufferPos; ++i) {
			if (m_pRecvBuffer[i] == '\r') {
				if (m_pRecvBuffer[i + 1] != '\n') {
					Close(false);
					return;
				}
				break;
			}
		}
		if ((i + 1) >= m_recvBufferPos) {
			if (m_recvBufferPos == m_recvBufferLen) {
				// We don't support header lines larger than 4096
				Close(false);
				return;
			}
			return;
		}

		m_pRecvBuffer[i] = 0;

		if (!m_responseCode) {
			m_responseString = wxString(m_pRecvBuffer, wxConvLocal);
			if (m_recvBufferPos < 16 || memcmp(m_pRecvBuffer, "HTTP/1.", 7)) {
				// Invalid HTTP Status-Line
				Close(false);
				return;
			}

			if (m_pRecvBuffer[9] < '1' || m_pRecvBuffer[9] > '5' ||
				m_pRecvBuffer[10] < '0' || m_pRecvBuffer[10] > '9' ||
				m_pRecvBuffer[11] < '0' || m_pRecvBuffer[11] > '9')
			{
				// Invalid response code
				Close(false);
				return;
			}

			m_responseCode = (m_pRecvBuffer[9] - '0') * 100 + (m_pRecvBuffer[10] - '0') * 10 + m_pRecvBuffer[11] - '0';

			if (m_responseCode >= 400) {
				// Failed request
				Close(false);
				return;
			}

			if (m_responseCode == 305) {
				// Unsupported redirect
				Close(false);
				return;
			}
		}
		else {
			if (!i) {
				// End of header, data from now on

				// Redirect if neccessary
				if (m_responseCode >= 300) {
					delete m_pSocket;
					m_pSocket = 0;

					delete [] m_pRecvBuffer;
					m_pRecvBuffer = 0;

					wxString location = m_location;

					ResetHttpData(false);

					GetExternalIP(location, m_protocol);
					return;
				}

				m_gotHeader = true;

				memmove(m_pRecvBuffer, m_pRecvBuffer + 2, m_recvBufferPos - 2);
				m_recvBufferPos -= 2;
				if (m_recvBufferPos) {
					if (m_transferEncoding == chunked)
						OnChunkedData();
					else
						OnData(m_pRecvBuffer, m_recvBufferPos);
				}
				return;
			}
			if (m_recvBufferPos > 12 && !memcmp(m_pRecvBuffer, "Location: ", 10)) {
				m_location = wxString(m_pRecvBuffer + 10, wxConvLocal);
			}
			else if (m_recvBufferPos > 21 && !memcmp(m_pRecvBuffer, "Transfer-Encoding: ", 19)) {
				if (!strcmp(m_pRecvBuffer + 19, "chunked"))
					m_transferEncoding = chunked;
				else if (!strcmp(m_pRecvBuffer + 19, "identity"))
					m_transferEncoding = identity;
				else
					m_transferEncoding = unknown;
			}
		}

		memmove(m_pRecvBuffer, m_pRecvBuffer + i + 2, m_recvBufferPos - i - 2);
		m_recvBufferPos -= i + 2;

		if (!m_recvBufferPos)
			break;
	}
}

void CExternalIPResolver::OnData(char* buffer, unsigned int len)
{
	if (buffer) {
		unsigned int i;
		for (i = 0; i < len; ++i) {
			if (buffer[i] == '\r' || buffer[i] == '\n')
				break;
			if (buffer[i] & 0x80) {
				Close(false);
				return;
			}
		}

		if (i)
			m_data += wxString(buffer, wxConvLocal, i);

		if (i == len)
			return;
	}

	if (m_protocol == CSocket::ipv6) {
		if (!m_data.empty() && m_data[0] == '[') {
			if (m_data.Last() != ']') {
				Close(false);
				return;
			}
			m_data.RemoveLast();
			m_data = m_data.Mid(1);
		}

		if (GetIPV6LongForm(m_data).empty()) {
			Close(false);
			return;
		}

		scoped_lock l(s_sync);
		ip = m_data;
	}
	else {

		// Validate ip address
		wxString digit = _T("0*[0-9]{1,3}");
		const wxChar* dot = _T("\\.");
		wxString exp = _T("(^|[^\\.[:digit:]])(") + digit + dot + digit + dot + digit + dot + digit + _T(")([^\\.[:digit:]]|$)");
		wxRegEx regex;
		regex.Compile(exp);

		if (!regex.Matches(m_data)) {
			Close(false);
			return;
		}

		scoped_lock l(s_sync);
		ip = regex.GetMatch(m_data, 2);
	}

	Close(true);
}

void CExternalIPResolver::ResetHttpData(bool resetRedirectCount)
{
	m_gotHeader = false;
	m_location.clear();
	m_responseCode = 0;
	m_responseString.clear();
	if (resetRedirectCount)
		m_redirectCount = 0;

	m_transferEncoding = unknown;

	m_chunkData.getTrailer = false;
	m_chunkData.size = 0;
	m_chunkData.terminateChunk = false;

	m_finished = false;
}

void CExternalIPResolver::OnChunkedData()
{
	char* p = m_pRecvBuffer;
	unsigned int len = m_recvBufferPos;

	for (;;) {
		if (m_chunkData.size != 0) {
			unsigned int dataLen = len;
			if (m_chunkData.size < len)
				dataLen = m_chunkData.size.GetLo();
			OnData(p, dataLen);
			if (!m_pRecvBuffer)
				return;

			m_chunkData.size -= dataLen;
			p += dataLen;
			len -= dataLen;

			if (m_chunkData.size == 0)
				m_chunkData.terminateChunk = true;

			if (!len)
				break;
		}

		// Find line ending
		unsigned int i = 0;
		for (i = 0; (i + 1) < len; ++i) {
			if (p[i] == '\r') {
				if (p[i + 1] != '\n') {
					Close(false);
					return;
				}
				break;
			}
		}
		if ((i + 1) >= len) {
			if (len == m_recvBufferLen) {
				// We don't support lines larger than 4096
				Close(false);
				return;
			}
			break;
		}

		p[i] = 0;

		if (m_chunkData.terminateChunk) {
			if (i) {
				// Chunk has to end with CRLF
				Close(false);
				return;
			}
			m_chunkData.terminateChunk = false;
		}
		else if (m_chunkData.getTrailer) {
			if (!i) {
				m_finished = true;
				m_recvBufferPos = 0;
				return;
			}

			// Ignore the trailer
		}
		else {
			// Read chunk size
			char* q = p;
			while (*q) {
				if (*q >= '0' && *q <= '9') {
					m_chunkData.size *= 16;
					m_chunkData.size += *q - '0';
				}
				else if (*q >= 'A' && *q <= 'F') {
					m_chunkData.size *= 10;
					m_chunkData.size += *q - 'A' + 10;
				}
				else if (*q >= 'a' && *q <= 'f') {
					m_chunkData.size *= 10;
					m_chunkData.size += *q - 'a' + 10;
				}
				else if (*q == ';' || *q == ' ')
					break;
				else {
					// Invalid size
					Close(false);
					return;
				}
				q++;
			}
			if (m_chunkData.size == 0)
				m_chunkData.getTrailer = true;
		}

		p += i + 2;
		len -= i + 2;

		if (!len)
			break;
	}

	if (p != m_pRecvBuffer) {
		memmove(m_pRecvBuffer, p, len);
		m_recvBufferPos = len;
	}
}

bool CExternalIPResolver::Successful() const
{
	scoped_lock l(s_sync);
	return !ip.empty();
}

wxString CExternalIPResolver::GetIP() const
{
	scoped_lock l(s_sync);
	return ip;
}
