#include <filezilla.h>

#include "ControlSocket.h"
#include "engineprivate.h"
#include "httpcontrolsocket.h"
#include "local_filesys.h"
#include "tlssocket.h"

#include <wx/file.h>

#define FZ_REPLY_REDIRECTED FZ_REPLY_ALREADYCONNECTED

// Connect is special for HTTP: It is done on a per-command basis, so we need
// to establish a connection before each command.
class CHttpConnectOpData : public CConnectOpData
{
public:
	CHttpConnectOpData()
		: tls(false)
	{
	}

	virtual ~CHttpConnectOpData()
	{
	}

	bool tls;
};

class CHttpOpData
{
public:
	CHttpOpData(COpData* pOpData)
		: m_pOpData(pOpData)
	{
		m_gotHeader = false;
		m_responseCode = -1;
		m_redirectionCount = 0;
		m_transferEncoding = unknown;

		m_chunkData.getTrailer = false;
		m_chunkData.size = 0;
		m_chunkData.terminateChunk = false;

		m_totalSize = -1;
		m_receivedData = 0;
	}

	virtual ~CHttpOpData() {}

	bool m_gotHeader;
	int m_responseCode;
	wxString m_responseString;
	wxURI m_newLocation;
	int m_redirectionCount;

	wxLongLong m_totalSize;
	wxLongLong m_receivedData;

	COpData* m_pOpData;

	enum transferEncodings
	{
		identity,
		chunked,
		unknown
	};
	enum transferEncodings m_transferEncoding;

	struct t_chunkData
	{
		bool getTrailer;
		bool terminateChunk;
		wxLongLong size;
	} m_chunkData;
};

class CHttpFileTransferOpData : public CFileTransferOpData, public CHttpOpData
{
public:
	CHttpFileTransferOpData(bool is_download, const wxString& local_file, const wxString& remote_file, const CServerPath& remote_path)
		: CFileTransferOpData(is_download, local_file, remote_file, remote_path), CHttpOpData(this)
	{
		pFile = 0;
	}

	virtual ~CHttpFileTransferOpData()
	{
		delete pFile;
	}

	wxFile* pFile;
};

CHttpControlSocket::CHttpControlSocket(CFileZillaEnginePrivate & engine)
	: CRealControlSocket(engine)
{
	m_pRecvBuffer = 0;
	m_recvBufferPos = 0;
	m_pTlsSocket = 0;
	m_pHttpOpData = 0;
}

CHttpControlSocket::~CHttpControlSocket()
{
	RemoveHandler();
	DoClose();
	delete [] m_pRecvBuffer;
}

int CHttpControlSocket::SendNextCommand()
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::SendNextCommand()"));
	if (!m_pCurOpData)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Warning, _T("SendNextCommand called without active operation"));
		ResetOperation(FZ_REPLY_ERROR);
		return FZ_REPLY_ERROR;
	}

	if (m_pCurOpData->waitForAsyncRequest)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("Waiting for async request, ignoring SendNextCommand"));
		return FZ_REPLY_WOULDBLOCK;
	}

	switch (m_pCurOpData->opId)
	{
	case Command::transfer:
		return FileTransferSend();
	default:
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Warning, _T("Unknown opID (%d) in SendNextCommand"), m_pCurOpData->opId);
		ResetOperation(FZ_REPLY_INTERNALERROR);
		break;
	}

	return FZ_REPLY_ERROR;
}


int CHttpControlSocket::ContinueConnect()
{
	LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Verbose, _T("CHttpControlSocket::ContinueConnect() &engine_=%p"), &engine_);
	if (GetCurrentCommandId() != Command::connect ||
		!m_pCurrentServer)
	{
		LogMessage(MessageType::Debug_Warning, _T("Invalid context for call to ContinueConnect(), cmd=%d, m_pCurrentServer=%p"), GetCurrentCommandId(), m_pCurrentServer);
		return DoClose(FZ_REPLY_INTERNALERROR);
	}

	ResetOperation(FZ_REPLY_OK);
	return FZ_REPLY_OK;
}

bool CHttpControlSocket::SetAsyncRequestReply(CAsyncRequestNotification *pNotification)
{
	if (m_pCurOpData)
	{
		if (!m_pCurOpData->waitForAsyncRequest)
		{
			LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("Not waiting for request reply, ignoring request reply %d"), pNotification->GetRequestID());
			return false;
		}
		m_pCurOpData->waitForAsyncRequest = false;
	}

	switch (pNotification->GetRequestID())
	{
	case reqId_fileexists:
		{
			if (!m_pCurOpData || m_pCurOpData->opId != Command::transfer)
			{
				LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("No or invalid operation in progress, ignoring request reply %f"), pNotification->GetRequestID());
				return false;
			}

			CFileExistsNotification *pFileExistsNotification = static_cast<CFileExistsNotification *>(pNotification);
			return SetFileExistsAction(pFileExistsNotification);
		}
		break;
	case reqId_certificate:
		{
			if (!m_pTlsSocket || m_pTlsSocket->GetState() != CTlsSocket::TlsState::verifycert)
			{
				LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("No or invalid operation in progress, ignoring request reply %d"), pNotification->GetRequestID());
				return false;
			}

			CCertificateNotification* pCertificateNotification = static_cast<CCertificateNotification *>(pNotification);
			m_pTlsSocket->TrustCurrentCert(pCertificateNotification->m_trusted);
		}
		break;
	default:
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Warning, _T("Unknown request %d"), pNotification->GetRequestID());
		ResetOperation(FZ_REPLY_INTERNALERROR);
		return false;
	}

	return true;
}

void CHttpControlSocket::OnReceive()
{
	DoReceive();
}

int CHttpControlSocket::DoReceive()
{
	do
	{
		const enum CSocket::SocketState state = m_pSocket->GetState();
		if (state != CSocket::connected && state != CSocket::closing)
			return 0;

		if (!m_pRecvBuffer)
		{
			m_pRecvBuffer = new char[m_recvBufferLen];
			m_recvBufferPos = 0;
		}

		unsigned int len = m_recvBufferLen - m_recvBufferPos;
		int error;
		int read = m_pBackend->Read(m_pRecvBuffer + m_recvBufferPos, len, error);
		if (read == -1)
		{
			if (error != EAGAIN)
			{
				ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
			}
			return 0;
		}

		SetActive(CFileZillaEngine::recv);

		if (!m_pCurOpData || m_pCurOpData->opId == Command::connect) {
			// Just ignore all further data
			m_recvBufferPos = 0;
			return 0;
		}

		m_recvBufferPos += read;

		if (!m_pHttpOpData->m_gotHeader) {
			if (!read)
			{
				ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
				return 0;
			}

			int res = ParseHeader(m_pHttpOpData);
			if ((res & FZ_REPLY_REDIRECTED) == FZ_REPLY_REDIRECTED)
				return FZ_REPLY_REDIRECTED;
			if (res != FZ_REPLY_WOULDBLOCK)
				return 0;
		}
		else if (m_pHttpOpData->m_transferEncoding == CHttpOpData::chunked)
		{
			if (!read)
			{
				ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
				return 0;
			}
			OnChunkedData(m_pHttpOpData);
		}
		else
		{
			if (!read)
			{
				wxASSERT(!m_recvBufferPos);
				ProcessData(0, 0);
				return 0;
			}
			else
			{
				m_pHttpOpData->m_receivedData += m_recvBufferPos;
				ProcessData(m_pRecvBuffer, m_recvBufferPos);
				m_recvBufferPos = 0;
			}
		}
	}
	while (m_pSocket);

	return 0;
}

void CHttpControlSocket::OnConnect()
{
	wxASSERT(GetCurrentCommandId() == Command::connect);

	CHttpConnectOpData *pData = static_cast<CHttpConnectOpData *>(m_pCurOpData);

	if (pData->tls) {
		if (!m_pTlsSocket) {
			LogMessage(MessageType::Status, _("Connection established, initializing TLS..."));

			delete m_pBackend;
			m_pTlsSocket = new CTlsSocket(this, m_pSocket, this);
			m_pBackend = m_pTlsSocket;

			if (!m_pTlsSocket->Init()) {
				LogMessage(MessageType::Error, _("Failed to initialize TLS."));
				DoClose();
				return;
			}

			int res = m_pTlsSocket->Handshake();
			if (res == FZ_REPLY_ERROR)
				DoClose();
		}
		else {
			LogMessage(MessageType::Status, _("TLS connection established, sending HTTP request"));
			ResetOperation(FZ_REPLY_OK);
		}

		return;
	}
	else
	{
		LogMessage(MessageType::Status, _("Connection established, sending HTTP request"));
		ResetOperation(FZ_REPLY_OK);
	}
}

enum filetransferStates
{
	filetransfer_init = 0,
	filetransfer_waitfileexists,
	filetransfer_transfer
};

int CHttpControlSocket::FileTransfer(const wxString localFile, const CServerPath &remotePath,
							  const wxString &remoteFile, bool download,
							  const CFileTransferCommand::t_transferSettings&)
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::FileTransfer()"));

	LogMessage(MessageType::Status, _("Downloading %s"), remotePath.FormatFilename(remoteFile));

	if (!download)
	{
		ResetOperation(FZ_REPLY_CRITICALERROR | FZ_REPLY_NOTSUPPORTED);
		return FZ_REPLY_ERROR;
	}

	if (m_pCurOpData)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("deleting nonzero pData"));
		delete m_pCurOpData;
	}

	CHttpFileTransferOpData *pData = new CHttpFileTransferOpData(download, localFile, remoteFile, remotePath);
	m_pCurOpData = pData;
	m_pHttpOpData = pData;

	m_current_uri = wxURI(m_pCurrentServer->FormatServer() + pData->remotePath.FormatFilename(pData->remoteFile));

	if (!localFile.empty()) {
		pData->localFileSize = CLocalFileSystem::GetSize(pData->localFile);

		pData->opState = filetransfer_waitfileexists;
		int res = CheckOverwriteFile();
		if (res != FZ_REPLY_OK)
			return res;

		pData->opState = filetransfer_transfer;

		res = OpenFile(pData);
		if( res != FZ_REPLY_OK )
			return res;
	}
	else
		pData->opState = filetransfer_transfer;

	int res = InternalConnect(m_pCurrentServer->GetHost(), m_pCurrentServer->GetPort(), m_pCurrentServer->GetProtocol() == HTTPS);
	if (res != FZ_REPLY_OK)
		return res;

	return FileTransferSend();
}

int CHttpControlSocket::FileTransferSubcommandResult(int prevResult)
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::FileTransferSubcommandResult(%d)"), prevResult);

	if (!m_pCurOpData)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("Empty m_pCurOpData"));
		ResetOperation(FZ_REPLY_INTERNALERROR);
		return FZ_REPLY_ERROR;
	}

	if (prevResult != FZ_REPLY_OK)
	{
		ResetOperation(prevResult);
		return FZ_REPLY_ERROR;
	}

	return FileTransferSend();
}

int CHttpControlSocket::FileTransferSend()
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::FileTransferSend()"));

	if (!m_pCurOpData)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("Empty m_pCurOpData"));
		ResetOperation(FZ_REPLY_INTERNALERROR);
		return FZ_REPLY_ERROR;
	}

	if( !m_current_uri.HasScheme() || !m_current_uri.HasServer() || !m_current_uri.HasPath() ) {
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Warning, _T("Invalid URI: %s"), m_current_uri.BuildURI());
		ResetOperation(FZ_REPLY_INTERNALERROR);
		return FZ_REPLY_ERROR;
	}

	CHttpFileTransferOpData *pData = static_cast<CHttpFileTransferOpData *>(m_pCurOpData);

	if (pData->opState == filetransfer_waitfileexists)
	{
		pData->opState = filetransfer_transfer;

		int res = OpenFile(pData);
		if( res != FZ_REPLY_OK)
			return res;

		res = InternalConnect(m_pCurrentServer->GetHost(), m_pCurrentServer->GetPort(), m_pCurrentServer->GetProtocol() == HTTPS);
		if (res != FZ_REPLY_OK)
			return res;
	}

	wxString location = m_current_uri.GetPath();
	if( m_current_uri.HasQuery() ) {
		location += _T("?") + m_current_uri.GetQuery();
	}
	wxString action = wxString::Format(_T("GET %s HTTP/1.1"), location );
	LogMessageRaw(MessageType::Command, action);

	wxString hostWithPort = m_current_uri.GetServer();
	if( m_current_uri.HasPort() ) {
		hostWithPort += _T(":") + m_current_uri.GetPort();
	}
	wxString command = wxString::Format(_T("%s\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n"), action, hostWithPort, wxString(PACKAGE_STRING, wxConvLocal));
	if( pData->resume ) {
		command += wxString::Format(_T("Range: bytes=%") + wxString(wxFileOffsetFmtSpec) + _T("d-\r\n"), pData->localFileSize);
	}
	command += _T("\r\n");

	const wxWX2MBbuf str = command.mb_str();
	if (!Send(str, strlen(str)))
		return FZ_REPLY_ERROR;

	return FZ_REPLY_WOULDBLOCK;
}

int CHttpControlSocket::InternalConnect(wxString host, unsigned short port, bool tls)
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::InternalConnect()"));

	CHttpConnectOpData* pData = new CHttpConnectOpData;
	pData->pNextOpData = m_pCurOpData;
	m_pCurOpData = pData;
	pData->port = port;
	pData->tls = tls;

	if (!IsIpAddress(host))
		LogMessage(MessageType::Status, _("Resolving address of %s"), host);

	pData->host = host;
	return DoInternalConnect();
}

int CHttpControlSocket::DoInternalConnect()
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::DoInternalConnect()"));

	if (!m_pCurOpData)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("Empty m_pCurOpData"));
		ResetOperation(FZ_REPLY_INTERNALERROR);
		return FZ_REPLY_ERROR;
	}

	CHttpConnectOpData *pData = static_cast<CHttpConnectOpData *>(m_pCurOpData);

	delete m_pBackend;
	m_pBackend = new CSocketBackend(this, *m_pSocket, engine_.GetRateLimiter());

	int res = m_pSocket->Connect(pData->host, pData->port);
	if (!res)
		return FZ_REPLY_OK;

	if (res && res != EINPROGRESS)
		return ResetOperation(FZ_REPLY_ERROR);

	return FZ_REPLY_WOULDBLOCK;
}

int CHttpControlSocket::FileTransferParseResponse(char* p, unsigned int len)
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::FileTransferParseResponse(%p, %d)"), p, len);

	if (!m_pCurOpData) {
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Info, _T("Empty m_pCurOpData"));
		ResetOperation(FZ_REPLY_INTERNALERROR);
		return FZ_REPLY_ERROR;
	}

	CHttpFileTransferOpData *pData = static_cast<CHttpFileTransferOpData *>(m_pCurOpData);

	if (!p) {
		ResetOperation(FZ_REPLY_OK);
		return FZ_REPLY_OK;
	}

	if (engine_.transfer_status_.empty()) {
		engine_.transfer_status_.Init(pData->m_totalSize.GetValue(), 0, false);
		engine_.transfer_status_.SetStartTime();
	}

	if (pData->localFile.empty()) {
		char* q = new char[len];
		memcpy(q, p, len);
		engine_.AddNotification(new CDataNotification(q, len));
	}
	else {
		wxASSERT(pData->pFile);

		if (pData->pFile->Write(p, len) != len) {
			LogMessage(MessageType::Error, _("Failed to write to file %s"), pData->localFile);
			ResetOperation(FZ_REPLY_ERROR);
			return FZ_REPLY_ERROR;
		}
	}

	engine_.transfer_status_.Update(len);

	return FZ_REPLY_WOULDBLOCK;
}

int CHttpControlSocket::ParseHeader(CHttpOpData* pData)
{
	// Parse the HTTP header.
	// We do just the neccessary parsing and silently ignore most header fields
	// Redirects are supported though if the server sends the Location field.

	for (;;) {
		// Find line ending
		unsigned int i = 0;
		for (i = 0; (i + 1) < m_recvBufferPos; i++)
		{
			if (m_pRecvBuffer[i] == '\r')
			{
				if (m_pRecvBuffer[i + 1] != '\n')
				{
					LogMessage(MessageType::Error, _("Malformed reply, server not sending proper line endings"));
					ResetOperation(FZ_REPLY_ERROR);
					return FZ_REPLY_ERROR;
				}
				break;
			}
		}
		if ((i + 1) >= m_recvBufferPos)
		{
			if (m_recvBufferPos == m_recvBufferLen)
			{
				// We don't support header lines larger than 4096
				LogMessage(MessageType::Error, _("Too long header line"));
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}
			return FZ_REPLY_WOULDBLOCK;
		}

		m_pRecvBuffer[i] = 0;
		wxString const line = wxString(m_pRecvBuffer, wxConvLocal);
		if (!line.empty())
			LogMessageRaw(MessageType::Response, line);

		if (pData->m_responseCode == -1)
		{
			pData->m_responseString = line;
			if (m_recvBufferPos < 16 || memcmp(m_pRecvBuffer, "HTTP/1.", 7))
			{
				// Invalid HTTP Status-Line
				LogMessage(MessageType::Error, _("Invalid HTTP Response"));
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}

			if (m_pRecvBuffer[9] < '1' || m_pRecvBuffer[9] > '5' ||
				m_pRecvBuffer[10] < '0' || m_pRecvBuffer[10] > '9' ||
				m_pRecvBuffer[11] < '0' || m_pRecvBuffer[11] > '9')
			{
				// Invalid response code
				LogMessage(MessageType::Error, _("Invalid response code"));
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}

			pData->m_responseCode = (m_pRecvBuffer[9] - '0') * 100 + (m_pRecvBuffer[10] - '0') * 10 + m_pRecvBuffer[11] - '0';

			if( pData->m_responseCode == 416 ) {
				CHttpFileTransferOpData* pTransfer = static_cast<CHttpFileTransferOpData*>(pData->m_pOpData);
				if( pTransfer->resume ) {
					// Sad, the server does not like our attempt to resume.
					// Get full file instead.
					pTransfer->resume = false;
					int res = OpenFile(pTransfer);
					if( res != FZ_REPLY_OK ) {
						return res;
					}
					pData->m_newLocation = m_current_uri;
					pData->m_responseCode = 300;
				}
			}

			if (pData->m_responseCode >= 400)
			{
				// Failed request
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}

			if (pData->m_responseCode == 305)
			{
				// Unsupported redirect
				LogMessage(MessageType::Error, _("Unsupported redirect"));
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}
		}
		else
		{
			if (!i)
			{
				// End of header, data from now on

				// Redirect if neccessary
				if (pData->m_responseCode >= 300)
				{
					if (pData->m_redirectionCount++ == 6) {
						LogMessage(MessageType::Error, _("Too many redirects"));
						ResetOperation(FZ_REPLY_ERROR);
						return FZ_REPLY_ERROR;
					}

					ResetSocket();
					ResetHttpData(pData);

					if( !pData->m_newLocation.HasScheme() || !pData->m_newLocation.HasServer() || !pData->m_newLocation.HasPath() ) {
						LogMessage(MessageType::Error, _("Redirection to invalid or unsupported URI: %s"), m_current_uri.BuildURI());
						ResetOperation(FZ_REPLY_ERROR);
						return FZ_REPLY_ERROR;
					}

					enum ServerProtocol protocol = CServer::GetProtocolFromPrefix(pData->m_newLocation.GetScheme());
					if( protocol != HTTP && protocol != HTTPS ) {
						LogMessage(MessageType::Error, _("Redirection to invalid or unsupported address: %s"), pData->m_newLocation.BuildURI());
						ResetOperation(FZ_REPLY_ERROR);
						return FZ_REPLY_ERROR;
					}

					long port = CServer::GetDefaultPort(protocol);
					if( pData->m_newLocation.HasPort() && (!pData->m_newLocation.GetPort().ToLong(&port) || port < 1 || port > 65535) ) {
						LogMessage(MessageType::Error, _("Redirection to invalid or unsupported address: %s"), pData->m_newLocation.BuildURI());
						ResetOperation(FZ_REPLY_ERROR);
						return FZ_REPLY_ERROR;
					}

					m_current_uri = pData->m_newLocation;

					// International domain names
					wxString host = ConvertDomainName(m_current_uri.GetServer());

					int res = InternalConnect(host, static_cast<unsigned short>(port), protocol == HTTPS);
					if (res == FZ_REPLY_WOULDBLOCK)
						res |= FZ_REPLY_REDIRECTED;
					return res;
				}

				if( pData->m_pOpData && pData->m_pOpData->opId == Command::transfer) {
					CHttpFileTransferOpData* pTransfer = static_cast<CHttpFileTransferOpData*>(pData->m_pOpData);
					if( pTransfer->resume && pData->m_responseCode != 206 ) {
						pTransfer->resume = false;
						int res = OpenFile(pTransfer);
						if( res != FZ_REPLY_OK ) {
							return res;
						}
					}
				}

				pData->m_gotHeader = true;

				memmove(m_pRecvBuffer, m_pRecvBuffer + 2, m_recvBufferPos - 2);
				m_recvBufferPos -= 2;

				if (m_recvBufferPos)
				{
					int res;
					if (pData->m_transferEncoding == pData->chunked)
						res = OnChunkedData(pData);
					else
					{
						pData->m_receivedData += m_recvBufferPos;
						res = ProcessData(m_pRecvBuffer, m_recvBufferPos);
						m_recvBufferPos = 0;
					}
					return res;
				}

				return FZ_REPLY_WOULDBLOCK;
			}
			if (m_recvBufferPos > 12 && !memcmp(m_pRecvBuffer, "Location: ", 10))
			{
				pData->m_newLocation = wxURI(wxString(m_pRecvBuffer + 10, wxConvLocal));
				pData->m_newLocation.Resolve(m_current_uri);
			}
			else if (m_recvBufferPos > 21 && !memcmp(m_pRecvBuffer, "Transfer-Encoding: ", 19))
			{
				if (!strcmp(m_pRecvBuffer + 19, "chunked"))
					pData->m_transferEncoding = CHttpOpData::chunked;
				else if (!strcmp(m_pRecvBuffer + 19, "identity"))
					pData->m_transferEncoding = CHttpOpData::identity;
				else
					pData->m_transferEncoding = CHttpOpData::unknown;
			}
			else if (i > 16 && !memcmp(m_pRecvBuffer, "Content-Length: ", 16))
			{
				pData->m_totalSize = 0;
				char* p = m_pRecvBuffer + 16;
				while (*p)
				{
					if (*p < '0' || *p > '9')
					{
						LogMessage(MessageType::Error, _("Malformed header: %s"), _("Invalid Content-Length"));
						ResetOperation(FZ_REPLY_ERROR);
						return FZ_REPLY_ERROR;
					}
					pData->m_totalSize = pData->m_totalSize * 10 + *p++ - '0';
				}
			}
		}

		memmove(m_pRecvBuffer, m_pRecvBuffer + i + 2, m_recvBufferPos - i - 2);
		m_recvBufferPos -= i + 2;

		if (!m_recvBufferPos)
			break;
	}

	return FZ_REPLY_WOULDBLOCK;
}

int CHttpControlSocket::OnChunkedData(CHttpOpData* pData)
{
	char* p = m_pRecvBuffer;
	unsigned int len = m_recvBufferPos;

	for (;;)
	{
		if (pData->m_chunkData.size != 0)
		{
			unsigned int dataLen = len;
			if (pData->m_chunkData.size < len)
				dataLen = pData->m_chunkData.size.GetLo();
			pData->m_receivedData += dataLen;
			int res = ProcessData(p, dataLen);
			if (res != FZ_REPLY_WOULDBLOCK)
				return res;

			pData->m_chunkData.size -= dataLen;
			p += dataLen;
			len -= dataLen;

			if (pData->m_chunkData.size == 0)
				pData->m_chunkData.terminateChunk = true;

			if (!len)
				break;
		}

		// Find line ending
		unsigned int i = 0;
		for (i = 0; (i + 1) < len; i++)
		{
			if (p[i] == '\r')
			{
				if (p[i + 1] != '\n')
				{
					LogMessage(MessageType::Error, _("Malformed chunk data: %s"), _("Wrong line endings"));
					ResetOperation(FZ_REPLY_ERROR);
					return FZ_REPLY_ERROR;
				}
				break;
			}
		}
		if ((i + 1) >= len)
		{
			if (len == m_recvBufferLen)
			{
				// We don't support lines larger than 4096
				LogMessage(MessageType::Error, _("Malformed chunk data: %s"), _("Line length exceeded"));
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}
			break;
		}

		p[i] = 0;

		if (pData->m_chunkData.terminateChunk)
		{
			if (i)
			{
				// The chunk data has to end with CRLF. If i is nonzero,
				// it didn't end with just CRLF.
				LogMessage(MessageType::Error, _("Malformed chunk data: %s"), _("Chunk data improperly terminated"));
				ResetOperation(FZ_REPLY_ERROR);
				return FZ_REPLY_ERROR;
			}
			pData->m_chunkData.terminateChunk = false;
		}
		else if (pData->m_chunkData.getTrailer)
		{
			if (!i)
			{
				// We're done
				return ProcessData(0, 0);
			}

			// Ignore the trailer
		}
		else
		{
			// Read chunk size
			for( char* q = p; *q && *q != ';' && *q != ' '; ++q ) {
				pData->m_chunkData.size *= 16;
				if (*q >= '0' && *q <= '9') {
					pData->m_chunkData.size += *q - '0';
				}
				else if (*q >= 'A' && *q <= 'F') {
					pData->m_chunkData.size += *q - 'A' + 10;
				}
				else if (*q >= 'a' && *q <= 'f') {
					pData->m_chunkData.size += *q - 'a' + 10;
				}
				else {
					// Invalid size
					LogMessage(MessageType::Error, _("Malformed chunk data: %s"), _("Invalid chunk size"));
					ResetOperation(FZ_REPLY_ERROR);
					return FZ_REPLY_ERROR;
				}
			}
			if (pData->m_chunkData.size == 0)
				pData->m_chunkData.getTrailer = true;
		}

		p += i + 2;
		len -= i + 2;

		if (!len)
			break;
	}

	if (p != m_pRecvBuffer)
	{
		memmove(m_pRecvBuffer, p, len);
		m_recvBufferPos = len;
	}

	return FZ_REPLY_WOULDBLOCK;
}

int CHttpControlSocket::ResetOperation(int nErrorCode)
{
	if (m_pCurOpData && m_pCurOpData->opId == Command::transfer)
	{
		CHttpFileTransferOpData *pData = static_cast<CHttpFileTransferOpData *>(m_pCurOpData);
		delete pData->pFile;
		pData->pFile = 0;
	}

	if (!m_pCurOpData || !m_pCurOpData->pNextOpData)
	{
		if (m_pBackend)
		{
			if (nErrorCode == FZ_REPLY_OK)
				LogMessage(MessageType::Status, _("Disconnected from server"));
			else
				LogMessage(MessageType::Error, _("Disconnected from server"));
		}
		ResetSocket();
		m_pHttpOpData = 0;
	}

	return CControlSocket::ResetOperation(nErrorCode);
}

void CHttpControlSocket::OnClose(int error)
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::OnClose(%d)"), error);

	if (error) {
		LogMessage(MessageType::Error, _("Disconnected from server: %s"), CSocket::GetErrorDescription(error));
		ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
		return;
	}

	// HTTP socket isn't connected outside operations
	if (!m_pCurOpData)
		return;

	if (m_pCurOpData->pNextOpData) {
		ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
		return;
	}

	if (!m_pHttpOpData->m_gotHeader) {
		ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
		return;
	}

	if (m_pHttpOpData->m_transferEncoding == CHttpOpData::chunked) {
		if (!m_pHttpOpData->m_chunkData.getTrailer) {
			ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
			return;
		}
	}
	else {
		if (m_pHttpOpData->m_totalSize != -1 && m_pHttpOpData->m_receivedData != m_pHttpOpData->m_totalSize) {
			ResetOperation(FZ_REPLY_ERROR | FZ_REPLY_DISCONNECTED);
			return;
		}
	}

	ProcessData(0, 0);
}

void CHttpControlSocket::ResetHttpData(CHttpOpData* pData)
{
	wxASSERT(pData);

	delete [] m_pRecvBuffer;
	m_pRecvBuffer = 0;

	pData->m_gotHeader = false;
	pData->m_responseCode = -1;
	pData->m_transferEncoding = CHttpOpData::unknown;

	pData->m_chunkData.getTrailer = false;
	pData->m_chunkData.size = 0;
	pData->m_chunkData.terminateChunk = false;

	pData->m_totalSize = -1;
	pData->m_receivedData = 0;
}

int CHttpControlSocket::ProcessData(char* p, int len)
{
	int res;
	Command commandId = GetCurrentCommandId();
	switch (commandId)
	{
	case Command::transfer:
		res = FileTransferParseResponse(p, len);
		break;
	default:
		LogMessage(MessageType::Debug_Warning, _T("No action for parsing data for command %d"), (int)commandId);
		ResetOperation(FZ_REPLY_INTERNALERROR);
		res = FZ_REPLY_ERROR;
		break;
	}

	wxASSERT(p || !m_pCurOpData);

	return res;
}

int CHttpControlSocket::ParseSubcommandResult(int prevResult)
{
	LogMessage(MessageType::Debug_Verbose, _T("CHttpControlSocket::SendNextCommand(%d)"), prevResult);
	if (!m_pCurOpData)
	{
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Warning, _T("SendNextCommand called without active operation"));
		ResetOperation(FZ_REPLY_ERROR);
		return FZ_REPLY_ERROR;
	}

	switch (m_pCurOpData->opId)
	{
	case Command::transfer:
		return FileTransferSubcommandResult(prevResult);
	default:
		LogMessage(__TFILE__, __LINE__, this, MessageType::Debug_Warning, _T("Unknown opID (%d) in SendNextCommand"), m_pCurOpData->opId);
		ResetOperation(FZ_REPLY_INTERNALERROR);
		break;
	}

	return FZ_REPLY_ERROR;
}

int CHttpControlSocket::Disconnect()
{
	DoClose();
	return FZ_REPLY_OK;
}

int CHttpControlSocket::OpenFile( CHttpFileTransferOpData* pData)
{
	delete pData->pFile;
	pData->pFile = new wxFile();
	CreateLocalDir(pData->localFile);

	if (!pData->pFile->Open(pData->localFile, pData->resume ? wxFile::write_append : wxFile::write))
	{
		LogMessage(MessageType::Error, _("Failed to open \"%s\" for writing"), pData->localFile);
		ResetOperation(FZ_REPLY_ERROR);
		return FZ_REPLY_ERROR;
	}
	wxFileOffset end = pData->pFile->SeekEnd();
	if( !end ) {
		pData->resume = false;
	}
	pData->localFileSize = CLocalFileSystem::GetSize(pData->localFile);
	return FZ_REPLY_OK;
}
