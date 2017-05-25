#include <filezilla.h>
#include "engineprivate.h"
#include "tlssocket.h"
#include "ControlSocket.h"

#include <gnutls/x509.h>

char const ciphers[] = "SECURE256:+SECURE128:-ARCFOUR-128:-3DES-CBC:-MD5:+SIGN-ALL:-SIGN-RSA-MD5:+CTYPE-X509:-CTYPE-OPENPGP:-VERS-SSL3.0";

#define TLSDEBUG 0
#if TLSDEBUG
// This is quite ugly
CControlSocket* pLoggingControlSocket;
void log_func(int level, const char* msg)
{
	if (!msg || !pLoggingControlSocket)
		return;
	wxString s(msg, wxConvLocal);
	s.Trim();
	pLoggingControlSocket->LogMessage(MessageType::Debug_Debug, _T("tls: %d %s"), level, s);
}
#endif

CTlsSocket::CTlsSocket(CEventHandler* pEvtHandler, CSocket* pSocket, CControlSocket* pOwner)
	: CEventHandler(pOwner->event_loop_)
	, CBackend(pEvtHandler)
	, m_pOwner(pOwner)
	, m_pSocket(pSocket)
{
	wxASSERT(pSocket);
	m_pSocketBackend = new CSocketBackend(this, *m_pSocket, m_pOwner->GetEngine().GetRateLimiter());

	m_implicitTrustedCert.data = 0;
	m_implicitTrustedCert.size = 0;
}

CTlsSocket::~CTlsSocket()
{
	RemoveHandler();

	Uninit();
	delete m_pSocketBackend;
}

bool CTlsSocket::Init()
{
	// This function initializes GnuTLS
	m_initialized = true;
	int res = gnutls_global_init();
	if (res) {
		LogError(res, _T("gnutls_global_init"));
		Uninit();
		return false;
	}

#if TLSDEBUG
	if (!pLoggingControlSocket) {
		pLoggingControlSocket = m_pOwner;
		gnutls_global_set_log_function(log_func);
		gnutls_global_set_log_level(99);
	}
#endif
	res = gnutls_certificate_allocate_credentials(&m_certCredentials);
	if (res < 0) {
		LogError(res, _T("gnutls_certificate_allocate_credentials"));
		Uninit();
		return false;
	}

	if (!InitSession())
		return false;

	m_shutdown_requested = false;

	// At this point, we can start shaking hands.

	return true;
}

bool CTlsSocket::InitSession()
{
	int res = gnutls_init(&m_session, GNUTLS_CLIENT);
	if (res) {
		LogError(res, _T("gnutls_init"));
		Uninit();
		return false;
	}

	// Even though the name gnutls_db_set_cache_expiration
	// implies expiration of some cache, it also governs
	// the actual session lifetime, independend whether the
	// session is cached or not.
	gnutls_db_set_cache_expiration(m_session, 100000000);

	res = gnutls_priority_set_direct(m_session, ciphers, 0);
	if (res) {
		LogError(res, _T("gnutls_priority_set_direct"));
		Uninit();
		return false;
	}

	gnutls_dh_set_prime_bits(m_session, 512);

	gnutls_credentials_set(m_session, GNUTLS_CRD_CERTIFICATE, m_certCredentials);

	// Setup transport functions
	gnutls_transport_set_push_function(m_session, PushFunction);
	gnutls_transport_set_pull_function(m_session, PullFunction);
	gnutls_transport_set_ptr(m_session, (gnutls_transport_ptr_t)this);

	return true;
}

void CTlsSocket::Uninit()
{
	UninitSession();

	if (m_certCredentials) {
		gnutls_certificate_free_credentials(m_certCredentials);
		m_certCredentials = 0;
	}

	if (m_initialized) {
		m_initialized = false;
		gnutls_global_deinit();
	}

	m_tlsState = TlsState::noconn;

	delete [] m_peekData;
	m_peekData = 0;
	m_peekDataLen = 0;

	delete [] m_implicitTrustedCert.data;
	m_implicitTrustedCert.data = 0;

#if TLSDEBUG
	if (pLoggingControlSocket == m_pOwner)
		pLoggingControlSocket = 0;
#endif
}


void CTlsSocket::UninitSession()
{
	if (m_session) {
		gnutls_deinit(m_session);
		m_session = 0;
	}
}


void CTlsSocket::LogError(int code, const wxString& function, MessageType logLevel)
{
	if (code == GNUTLS_E_WARNING_ALERT_RECEIVED || code == GNUTLS_E_FATAL_ALERT_RECEIVED) {
		PrintAlert(logLevel);
	}
	else if (code == GNUTLS_E_PULL_ERROR) {
		if (function.empty()) {
			m_pOwner->LogMessage(MessageType::Debug_Warning, _T("GnuTLS could not read from socket: %s"), CSocket::GetErrorDescription(m_socket_error));
		}
		else {
			m_pOwner->LogMessage(MessageType::Debug_Warning, _T("GnuTLS could not read from socket in %s: %s"), function, CSocket::GetErrorDescription(m_socket_error));
		}
	}
	else if (code == GNUTLS_E_PUSH_ERROR) {
		if (function.empty()) {
			m_pOwner->LogMessage(MessageType::Debug_Warning, _T("GnuTLS could not write to socket: %s"), CSocket::GetErrorDescription(m_socket_error));
		}
		else {
			m_pOwner->LogMessage(MessageType::Debug_Warning, _T("GnuTLS could not write to socket in %s: %s"), function, CSocket::GetErrorDescription(m_socket_error));
		}
	}
	else {
		const char* error = gnutls_strerror(code);
		if (error) {
			wxString str(error, wxConvLocal);
			if (function.empty())
				m_pOwner->LogMessage(logLevel, _("GnuTLS error %d: %s"), code, str);
			else
				m_pOwner->LogMessage(logLevel, _("GnuTLS error %d in %s: %s"), code, function, str);
		}
		else {
			if (function.empty())
				m_pOwner->LogMessage(logLevel, _("GnuTLS error %d"), code);
			else
				m_pOwner->LogMessage(logLevel, _("GnuTLS error %d in %s"), code, function);
		}
	}
}

void CTlsSocket::PrintAlert(MessageType logLevel)
{
	gnutls_alert_description_t last_alert = gnutls_alert_get(m_session);
	const char* alert = gnutls_alert_get_name(last_alert);
	if (alert) {
		wxString str(alert, wxConvLocal);
		m_pOwner->LogMessage(logLevel, _("Received TLS alert from the server: %s (%d)"), str, last_alert);
	}
	else
		m_pOwner->LogMessage(logLevel, _("Received unknown TLS alert %d from the server"), last_alert);
}

ssize_t CTlsSocket::PushFunction(gnutls_transport_ptr_t ptr, const void* data, size_t len)
{
	return ((CTlsSocket*)ptr)->PushFunction(data, len);
}

ssize_t CTlsSocket::PullFunction(gnutls_transport_ptr_t ptr, void* data, size_t len)
{
	return ((CTlsSocket*)ptr)->PullFunction(data, len);
}

ssize_t CTlsSocket::PushFunction(const void* data, size_t len)
{
#if TLSDEBUG
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("CTlsSocket::PushFunction(%d)"), len);
#endif
	if (!m_canWriteToSocket) {
		gnutls_transport_set_errno(m_session, EAGAIN);
		return -1;
	}

	if (!m_pSocketBackend) {
		gnutls_transport_set_errno(m_session, 0);
		return -1;
	}

	int error;
	int written = m_pSocketBackend->Write(data, len, error);

	if (written < 0) {
		m_canWriteToSocket = false;
		if (error == EAGAIN) {
			m_socket_error = error;
		}
		gnutls_transport_set_errno(m_session, error);
#if TLSDEBUG
		m_pOwner->LogMessage(MessageType::Debug_Debug, _T("  returning -1 due to %d"), error);
#endif
		return -1;
	}

#if TLSDEBUG
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("  returning %d"), written);
#endif

	return written;
}

ssize_t CTlsSocket::PullFunction(void* data, size_t len)
{
#if TLSDEBUG
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("CTlsSocket::PullFunction(%d)"),  (int)len);
#endif
	if (!m_pSocketBackend) {
		gnutls_transport_set_errno(m_session, 0);
		return -1;
	}

	if (m_socketClosed)
		return 0;

	if (!m_canReadFromSocket) {
		gnutls_transport_set_errno(m_session, EAGAIN);
		return -1;
	}

	int error;
	int read = m_pSocketBackend->Read(data, len, error);
	if (read < 0) {
		m_canReadFromSocket = false;
		if (error == EAGAIN) {
			if (m_canCheckCloseSocket && !m_pSocketBackend->IsWaiting(CRateLimiter::inbound)) {
				SendEvent<CSocketEvent>(m_pSocketBackend, SocketEventType::close, 0);
			}
		}
		else {
			m_socket_error = error;
		}
		gnutls_transport_set_errno(m_session, error);
#if TLSDEBUG
		m_pOwner->LogMessage(MessageType::Debug_Debug, _T("  returning -1 due to %d"), error);
#endif
		return -1;
	}

	if (m_canCheckCloseSocket) {
		SendEvent<CSocketEvent>(m_pSocketBackend, SocketEventType::close, 0);
	}

	if (!read) {
		m_socket_eof = true;
	}

#if TLSDEBUG
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("  returning %d"), read);
#endif

	return read;
}

void CTlsSocket::operator()(CEventBase const& ev)
{
	Dispatch<CSocketEvent>(ev, this, &CTlsSocket::OnSocketEvent);
}

void CTlsSocket::OnSocketEvent(CSocketEventSource*, SocketEventType t, int error)
{
	wxASSERT(m_pSocket);
	if (!m_session)
		return;

	switch (t)
	{
	case SocketEventType::read:
		OnRead();
		break;
	case SocketEventType::write:
		OnSend();
		break;
	case SocketEventType::close:
		{
			m_canCheckCloseSocket = true;
			char tmp[100];
			int peeked = m_pSocketBackend->Peek(&tmp, 100, error);
			if (peeked >= 0) {
				if (peeked > 0)
					m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("CTlsSocket::OnSocketEvent(): pending data, postponing close event"));
				else
				{
					m_socket_eof = true;
					m_socketClosed = true;
				}
				OnRead();

				if (peeked)
					return;
			}

			m_pOwner->LogMessage(MessageType::Debug_Info, _T("CTlsSocket::OnSocketEvent(): close event received"));

			//Uninit();
			m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::close, 0);
		}
		break;
	default:
		break;
	}
}

void CTlsSocket::OnRead()
{
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("CTlsSocket::OnRead()"));

	m_canReadFromSocket = true;

	if (!m_session)
		return;

	const int direction = gnutls_record_get_direction(m_session);
	if (direction && !m_lastReadFailed) {
		m_pOwner->LogMessage(MessageType::Debug_Debug, _T("CTlsSocket::Postponing read"));
		return;
	}

	if (m_tlsState == TlsState::handshake)
		ContinueHandshake();
	if (m_tlsState == TlsState::closing)
		ContinueShutdown();
	else if (m_tlsState == TlsState::conn)
	{
		CheckResumeFailedReadWrite();
		TriggerEvents();
	}
}

void CTlsSocket::OnSend()
{
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("CTlsSocket::OnSend()"));

	m_canWriteToSocket = true;

	if (!m_session)
		return;

	const int direction = gnutls_record_get_direction(m_session);
	if (!direction && !m_lastWriteFailed)
		return;

	if (m_tlsState == TlsState::handshake)
		ContinueHandshake();
	else if (m_tlsState == TlsState::closing)
		ContinueShutdown();
	else if (m_tlsState == TlsState::conn)
	{
		CheckResumeFailedReadWrite();
		TriggerEvents();
	}
}

bool CTlsSocket::CopySessionData(const CTlsSocket* pPrimarySocket)
{
	gnutls_datum_t d;
	int res = gnutls_session_get_data2(pPrimarySocket->m_session, &d);
	if (res) {
		m_pOwner->LogMessage(MessageType::Debug_Warning, _T("gnutls_session_get_data2 on primary socket failed: %d"), res);
		return true;
	}

	// Set session data
	res = gnutls_session_set_data(m_session, d.data, d.size );
	gnutls_free(d.data);
	if (res) {
		m_pOwner->LogMessage(MessageType::Debug_Info, _T("gnutls_session_set_data failed: %d. Going to reinitialize session."), res);
		UninitSession();
		if (!InitSession())
			return false;
	}
	else
		m_pOwner->LogMessage(MessageType::Debug_Info, _T("Trying to resume existing TLS session."));

	return true;
}

bool CTlsSocket::ResumedSession() const
{
	return gnutls_session_is_resumed(m_session) != 0;
}

int CTlsSocket::Handshake(const CTlsSocket* pPrimarySocket, bool try_resume)
{
	m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("CTlsSocket::Handshake()"));
	wxASSERT(m_session);

	m_tlsState = TlsState::handshake;

	wxString hostname;

	if (pPrimarySocket) {
		// Implicitly trust certificate of primary socket
		unsigned int cert_list_size;
		const gnutls_datum_t* const cert_list = gnutls_certificate_get_peers(pPrimarySocket->m_session, &cert_list_size);
		if (cert_list && cert_list_size)
		{
			delete [] m_implicitTrustedCert.data;
			m_implicitTrustedCert.data = new unsigned char[cert_list[0].size];
			memcpy(m_implicitTrustedCert.data, cert_list[0].data, cert_list[0].size);
			m_implicitTrustedCert.size = cert_list[0].size;
		}

		if (try_resume)
		{
			if (!CopySessionData(pPrimarySocket))
				return FZ_REPLY_ERROR;
		}

		hostname = pPrimarySocket->m_pSocket->GetPeerHost();
	}
	else {
		hostname = m_pSocket->GetPeerHost();
	}

	if( !hostname.empty() && !IsIpAddress(hostname) ) {
		const wxWX2MBbuf utf8 = hostname.mb_str(wxConvUTF8);
		if( utf8 ) {
			int res = gnutls_server_name_set( m_session, GNUTLS_NAME_DNS, utf8, strlen(utf8) );
			if( res ) {
				LogError(res, _T("gnutls_server_name_set"), MessageType::Debug_Warning );
			}
		}
	}

	return ContinueHandshake();
}

int CTlsSocket::ContinueHandshake()
{
	m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("CTlsSocket::ContinueHandshake()"));
	wxASSERT(m_session);
	wxASSERT(m_tlsState == TlsState::handshake);

	int res = gnutls_handshake(m_session);
	while (res == GNUTLS_E_AGAIN || res == GNUTLS_E_INTERRUPTED) {
		if (!(gnutls_record_get_direction(m_session) ? m_canWriteToSocket : m_canReadFromSocket)) {
			break;
		}
		res = gnutls_handshake(m_session);
	}
	if (!res) {
		m_pOwner->LogMessage(MessageType::Debug_Info, _T("TLS Handshake successful"));

		if (ResumedSession())
			m_pOwner->LogMessage(MessageType::Debug_Info, _T("TLS Session resumed"));

		const wxString protocol = GetProtocolName();
		const wxString keyExchange = GetKeyExchange();
		const wxString cipherName = GetCipherName();
		const wxString macName = GetMacName();

		m_pOwner->LogMessage(MessageType::Debug_Info, _T("Protocol: %s, Key exchange: %s, Cipher: %s, MAC: %s"), protocol, keyExchange, cipherName, macName);

		res = VerifyCertificate();
		if (res != FZ_REPLY_OK)
			return res;

		if (m_shutdown_requested) {
			int error = Shutdown();
			if (!error || error != EAGAIN) {
				m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::close, 0);
			}
		}

		return FZ_REPLY_OK;
	}
	else if (res == GNUTLS_E_AGAIN || res == GNUTLS_E_INTERRUPTED)
		return FZ_REPLY_WOULDBLOCK;

	Failure(res, true);

	return FZ_REPLY_ERROR;
}

int CTlsSocket::Read(void *buffer, unsigned int len, int& error)
{
	if (m_tlsState == TlsState::handshake || m_tlsState == TlsState::verifycert) {
		error = EAGAIN;
		return -1;
	}
	else if (m_tlsState != TlsState::conn) {
		error = ENOTCONN;
		return -1;
	}
	else if (m_lastReadFailed) {
		error = EAGAIN;
		return -1;
	}

	if (m_peekDataLen) {
		unsigned int min = wxMin(len, m_peekDataLen);
		memcpy(buffer, m_peekData, min);

		if (min == m_peekDataLen) {
			m_peekDataLen = 0;
			delete [] m_peekData;
			m_peekData = 0;
		}
		else {
			memmove(m_peekData, m_peekData + min, m_peekDataLen - min);
			m_peekDataLen -= min;
		}

		TriggerEvents();

		error = 0;
		return min;
	}

	int res = DoCallGnutlsRecordRecv(buffer, len);
	if (res >= 0) {
		if (res > 0)
			TriggerEvents();
		else {
			// Peer did already initiate a shutdown, reply to it
			gnutls_bye(m_session, GNUTLS_SHUT_WR);
			// Note: Theoretically this could return a write error.
			// But we ignore it, since it is perfectly valid for peer
			// to close the connection after sending its shutdown
			// notification.
		}

		error = 0;
		return res;
	}

	if (res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN) {
		error = EAGAIN;
		m_lastReadFailed = true;
	}
	else {
		Failure(res, false, _T("gnutls_record_recv"));
		error = m_socket_error;
	}

	return -1;
}

int CTlsSocket::Write(const void *buffer, unsigned int len, int& error)
{
	if (m_tlsState == TlsState::handshake || m_tlsState == TlsState::verifycert) {
		error = EAGAIN;
		return -1;
	}
	else if (m_tlsState != TlsState::conn) {
		error = ENOTCONN;
		return -1;
	}

	if (m_lastWriteFailed) {
		error = EAGAIN;
		return -1;
	}

	if (m_writeSkip >= len) {
		m_writeSkip -= len;
		return len;
	}

	len -= m_writeSkip;
	buffer = (char*)buffer + m_writeSkip;

	int res = gnutls_record_send(m_session, buffer, len);

	while ((res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN) && m_canWriteToSocket)
		res = gnutls_record_send(m_session, 0, 0);

	if (res >= 0) {
		error = 0;
		int written = res + m_writeSkip;
		m_writeSkip = 0;

		TriggerEvents();
		return written;
	}

	if (res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN) {
		if (m_writeSkip) {
			error = 0;
			int written = m_writeSkip;
			m_writeSkip = 0;
			return written;
		}
		else {
			error = EAGAIN;
			m_lastWriteFailed = true;
			return -1;
		}
	}
	else {
		Failure(res, false, _T("gnutls_record_send"));
		error = m_socket_error;
		return -1;
	}
}

void CTlsSocket::TriggerEvents()
{
	if (m_tlsState != TlsState::conn)
		return;

	if (m_canTriggerRead) {
		m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::read, 0);
		m_canTriggerRead = false;
	}

	if (m_canTriggerWrite) {
		m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::write, 0);
		m_canTriggerWrite = false;
	}
}

void CTlsSocket::CheckResumeFailedReadWrite()
{
	if (m_lastWriteFailed) {
		int res = GNUTLS_E_AGAIN;
		while ((res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN) && m_canWriteToSocket)
			res = gnutls_record_send(m_session, 0, 0);

		if (res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN)
			return;

		if (res < 0) {
			Failure(res, true);
			return;
		}

		m_writeSkip += res;
		m_lastWriteFailed = false;
		m_canTriggerWrite = true;
	}
	if (m_lastReadFailed) {
		wxASSERT(!m_peekData);

		m_peekDataLen = 65536;
		m_peekData = new char[m_peekDataLen];

		int res = DoCallGnutlsRecordRecv(m_peekData, m_peekDataLen);
		if (res < 0) {
			m_peekDataLen = 0;
			delete [] m_peekData;
			m_peekData = 0;
			if (res != GNUTLS_E_INTERRUPTED && res != GNUTLS_E_AGAIN)
				Failure(res, true);
			return;
		}

		if (!res) {
			m_peekDataLen = 0;
			delete [] m_peekData;
			m_peekData = 0;
		}
		else
			m_peekDataLen = res;

		m_lastReadFailed = false;
		m_canTriggerRead = true;
	}
}

void CTlsSocket::Failure(int code, bool send_close, const wxString& function)
{
	m_pOwner->LogMessage(MessageType::Debug_Debug, _T("CTlsSocket::Failure(%d)"), code);
	if (code) {
		LogError(code, function);
		if (m_socket_eof) {
			if (code == GNUTLS_E_UNEXPECTED_PACKET_LENGTH
#ifdef GNUTLS_E_PREMATURE_TERMINATION
				|| code == GNUTLS_E_PREMATURE_TERMINATION
#endif
				)
			{
				m_pOwner->LogMessage(MessageType::Status, _("Server did not properly shut down TLS connection"));
			}
		}
	}
	Uninit();

	if (send_close) {
		m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::close, m_socket_error);
	}
}

int CTlsSocket::Peek(void *buffer, unsigned int len, int& error)
{
	if (m_peekData) {
		int min = wxMin(len, m_peekDataLen);
		memcpy(buffer, m_peekData, min);

		error = 0;
		return min;
	}

	int read = Read(buffer, len, error);
	if (read <= 0)
		return read;

	m_peekDataLen = read;
	m_peekData = new char[m_peekDataLen];
	memcpy(m_peekData, buffer, m_peekDataLen);

	return read;
}

int CTlsSocket::Shutdown()
{
	m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("CTlsSocket::Shutdown()"));

	if (m_tlsState == TlsState::closed)
		return 0;

	if (m_tlsState == TlsState::closing)
		return EAGAIN;

	if (m_tlsState == TlsState::handshake || m_tlsState == TlsState::verifycert) {
		// Shutdown during handshake is not a good idea.
		m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("Shutdown during handshake, postponing"));
		m_shutdown_requested = true;
		return EAGAIN;
	}

	if (m_tlsState != TlsState::conn)
		return ECONNABORTED;

	m_tlsState = TlsState::closing;

	int res = gnutls_bye(m_session, GNUTLS_SHUT_WR);
	while ((res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN) && m_canWriteToSocket)
		res = gnutls_bye(m_session, GNUTLS_SHUT_WR);
	if (!res) {
		m_tlsState = TlsState::closed;
		return 0;
	}

	if (res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN)
		return EAGAIN;

	Failure(res, false);
	return m_socket_error;
}

void CTlsSocket::ContinueShutdown()
{
	m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("CTlsSocket::ContinueShutdown()"));

	int res = gnutls_bye(m_session, GNUTLS_SHUT_WR);
	while ((res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN) && m_canWriteToSocket)
		res = gnutls_bye(m_session, GNUTLS_SHUT_WR);
	if (!res) {
		m_tlsState = TlsState::closed;

		m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::close, 0);

		return;
	}

	if (res != GNUTLS_E_INTERRUPTED && res != GNUTLS_E_AGAIN)
		Failure(res, true);
}

void CTlsSocket::TrustCurrentCert(bool trusted)
{
	if (m_tlsState != TlsState::verifycert) {
		m_pOwner->LogMessage(MessageType::Debug_Warning, _T("TrustCurrentCert called at wrong time."));
		return;
	}

	if (trusted) {
		m_tlsState = TlsState::conn;

		if (m_lastWriteFailed)
			m_lastWriteFailed = false;
		CheckResumeFailedReadWrite();

		if (m_tlsState == TlsState::conn) {
			m_pEvtHandler->SendEvent<CSocketEvent>(this, SocketEventType::connection, 0);
		}

		TriggerEvents();

		return;
	}

	m_pOwner->LogMessage(MessageType::Error, _("Remote certificate not trusted."));
	Failure(0, true);
}

static wxString bin2hex(const unsigned char* in, size_t size)
{
	wxString str;
	for (size_t i = 0; i < size; i++)
	{
		if (i)
			str += _T(":");
		str += wxString::Format(_T("%.2x"), (int)in[i]);
	}

	return str;
}


bool CTlsSocket::ExtractCert(gnutls_datum_t const* datum, CCertificate& out)
{
	gnutls_x509_crt_t cert;
	if (gnutls_x509_crt_init(&cert)) {
		m_pOwner->LogMessage(MessageType::Error, _("Could not initialize structure for peer certificates, gnutls_x509_crt_init failed"));
		return false;
	}

	if (gnutls_x509_crt_import(cert, datum, GNUTLS_X509_FMT_DER)) {
		m_pOwner->LogMessage(MessageType::Error, _("Could not import peer certificates, gnutls_x509_crt_import failed"));
		gnutls_x509_crt_deinit(cert);
		return false;
	}

	wxDateTime expirationTime = gnutls_x509_crt_get_expiration_time(cert);
	wxDateTime activationTime = gnutls_x509_crt_get_activation_time(cert);

	// Get the serial number of the certificate
	unsigned char buffer[40];
	size_t size = sizeof(buffer);
	int res = gnutls_x509_crt_get_serial(cert, buffer, &size);
	if( res != 0 ) {
		size = 0;
	}

	wxString serial = bin2hex(buffer, size);

	unsigned int pkBits;
	int pkAlgo = gnutls_x509_crt_get_pk_algorithm(cert, &pkBits);
	wxString pkAlgoName;
	if (pkAlgo >= 0) {
		const char* pAlgo = gnutls_pk_algorithm_get_name((gnutls_pk_algorithm_t)pkAlgo);
		if (pAlgo)
			pkAlgoName = wxString(pAlgo, wxConvUTF8);
	}

	int signAlgo = gnutls_x509_crt_get_signature_algorithm(cert);
	wxString signAlgoName;
	if (signAlgo >= 0) {
		const char* pAlgo = gnutls_sign_algorithm_get_name((gnutls_sign_algorithm_t)signAlgo);
		if (pAlgo)
			signAlgoName = wxString(pAlgo, wxConvUTF8);
	}

	wxString subject, issuer;

	size = 0;
	res = gnutls_x509_crt_get_dn(cert, 0, &size);
	if (size) {
		char* dn = new char[size + 1];
		dn[size] = 0;
		if (!(res = gnutls_x509_crt_get_dn(cert, dn, &size)))
		{
			dn[size] = 0;
			subject = wxString(dn, wxConvUTF8);
		}
		else
			LogError(res, _T("gnutls_x509_crt_get_dn"));
		delete [] dn;
	}
	else
		LogError(res, _T("gnutls_x509_crt_get_dn"));
	if (subject.empty()) {
		m_pOwner->LogMessage(MessageType::Error, _("Could not get distinguished name of certificate subject, gnutls_x509_get_dn failed"));
		gnutls_x509_crt_deinit(cert);
		return false;
	}

	std::vector<wxString> alt_subject_names = GetCertSubjectAltNames(cert);

	size = 0;
	res = gnutls_x509_crt_get_issuer_dn(cert, 0, &size);
	if (size) {
		char* dn = new char[++size + 1];
		dn[size] = 0;
		if (!(res = gnutls_x509_crt_get_issuer_dn(cert, dn, &size))) {
			dn[size] = 0;
			issuer = wxString(dn, wxConvUTF8);
		}
		else
			LogError(res, _T("gnutls_x509_crt_get_issuer_dn"));
		delete [] dn;
	}
	else
		LogError(res, _T("gnutls_x509_crt_get_issuer_dn"));
	if (issuer.empty() ) {
		m_pOwner->LogMessage(MessageType::Error, _("Could not get distinguished name of certificate issuer, gnutls_x509_get_issuer_dn failed"));
		gnutls_x509_crt_deinit(cert);
		return false;
	}

	wxString fingerprint_sha256;
	wxString fingerprint_sha1;

	unsigned char digest[100];
	size = sizeof(digest) - 1;
	if (!gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA256, digest, &size)) {
		digest[size] = 0;
		fingerprint_sha256 = bin2hex(digest, size);
	}
	size = sizeof(digest) - 1;
	if (!gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA1, digest, &size)) {
		digest[size] = 0;
		fingerprint_sha1 = bin2hex(digest, size);
	}

	gnutls_x509_crt_deinit(cert);

	out = CCertificate(
		datum->data, datum->size,
		activationTime, expirationTime,
		serial,
		pkAlgoName, pkBits,
		signAlgoName,
		fingerprint_sha256,
		fingerprint_sha1,
		issuer,
		subject,
		alt_subject_names);

	return true;
}


std::vector<wxString> CTlsSocket::GetCertSubjectAltNames(gnutls_x509_crt_t cert)
{
	std::vector<wxString> ret;

	char san[4096];
	for (unsigned int i = 0; i < 10000; ++i) { // I assume this is a sane limit
		size_t san_size = sizeof(san) - 1;
		int const type_or_error = gnutls_x509_crt_get_subject_alt_name(cert, i, san, &san_size, 0);
		if (type_or_error == GNUTLS_E_SHORT_MEMORY_BUFFER) {
			continue;
		}
		else if (type_or_error < 0) {
			break;
		}

		if (type_or_error == GNUTLS_SAN_DNSNAME || type_or_error == GNUTLS_SAN_RFC822NAME) {
			wxString dns = wxString(san, wxConvUTF8);
			if (!dns.empty()) {
				ret.push_back(dns);
			}
		}
		else if (type_or_error == GNUTLS_SAN_IPADDRESS) {
			wxString ip = CSocket::AddressToString(san, san_size);
			if (!ip.empty()) {
				ret.push_back(ip);
			}
		}
	}

	return ret;
}

int CTlsSocket::VerifyCertificate()
{
	if (m_tlsState != TlsState::handshake) {
		m_pOwner->LogMessage(MessageType::Debug_Warning, _T("VerifyCertificate called at wrong time"));
		return FZ_REPLY_ERROR;
	}

	m_tlsState = TlsState::verifycert;

	if (gnutls_certificate_type_get(m_session) != GNUTLS_CRT_X509) {
		m_pOwner->LogMessage(MessageType::Error, _("Unsupported certificate type"));
		Failure(0, true);
		return FZ_REPLY_ERROR;
	}

	unsigned int status = 0;
	if (gnutls_certificate_verify_peers2(m_session, &status) < 0) {
		m_pOwner->LogMessage(MessageType::Error, _("Failed to verify peer certificate"));
		Failure(0, true);
		return FZ_REPLY_ERROR;
	}

	if (status & GNUTLS_CERT_REVOKED) {
		m_pOwner->LogMessage(MessageType::Error, _("Beware! Certificate has been revoked"));
		Failure(0, true);
		return FZ_REPLY_ERROR;
	}

	if (status & GNUTLS_CERT_SIGNER_NOT_CA) {
		m_pOwner->LogMessage(MessageType::Error, _("Incomplete chain, top certificate is not self-signed certificate authority certificate"));
		Failure(0, true);
		return FZ_REPLY_ERROR;
	}

	unsigned int cert_list_size;
	const gnutls_datum_t* cert_list = gnutls_certificate_get_peers(m_session, &cert_list_size);
	if (!cert_list || !cert_list_size) {
		m_pOwner->LogMessage(MessageType::Error, _("gnutls_certificate_get_peers returned no certificates"));
		Failure(0, true);
		return FZ_REPLY_ERROR;
	}

	if (m_implicitTrustedCert.data) {
		if (m_implicitTrustedCert.size != cert_list[0].size ||
			memcmp(m_implicitTrustedCert.data, cert_list[0].data, cert_list[0].size))
		{
			m_pOwner->LogMessage(MessageType::Error, _("Primary connection and data connection certificates don't match."));
			Failure(0, true);
			return FZ_REPLY_ERROR;
		}

		TrustCurrentCert(true);

		if (m_tlsState != TlsState::conn)
			return FZ_REPLY_ERROR;
		return FZ_REPLY_OK;
	}

	std::vector<CCertificate> certificates;
	for (unsigned int i = 0; i < cert_list_size; ++i) {
		CCertificate cert;
		if (ExtractCert(cert_list, cert))
			certificates.push_back(cert);
		else {
			Failure(0, true);
			return FZ_REPLY_ERROR;
		}

		++cert_list;
	}

	CCertificateNotification *pNotification = new CCertificateNotification(
		m_pOwner->GetCurrentServer()->GetHost(),
		m_pOwner->GetCurrentServer()->GetPort(),
		GetProtocolName(),
		GetKeyExchange(),
		GetCipherName(),
		GetMacName(),
		certificates);

	m_pOwner->SendAsyncRequest(pNotification);

	m_pOwner->LogMessage(MessageType::Status, _("Verifying certificate..."));

	return FZ_REPLY_WOULDBLOCK;
}

void CTlsSocket::OnRateAvailable(enum CRateLimiter::rate_direction)
{
}

wxString CTlsSocket::GetProtocolName()
{
	wxString protocol = _("unknown");

	const char* s = gnutls_protocol_get_name( gnutls_protocol_get_version( m_session ) );
	if (s && *s)
		protocol = wxString(s, wxConvUTF8);

	return protocol;
}

wxString CTlsSocket::GetKeyExchange()
{
	wxString keyExchange = _("unknown");

	const char* s = gnutls_kx_get_name( gnutls_kx_get( m_session ) );
	if (s && *s)
		keyExchange = wxString(s, wxConvUTF8);

	return keyExchange;
}

wxString CTlsSocket::GetCipherName()
{
	const char* cipher = gnutls_cipher_get_name(gnutls_cipher_get(m_session));
	if (cipher && *cipher)
		return wxString(cipher, wxConvUTF8);
	else
		return _("unknown");
}

wxString CTlsSocket::GetMacName()
{
	const char* mac = gnutls_mac_get_name(gnutls_mac_get(m_session));
	if (mac && *mac)
		return wxString(mac, wxConvUTF8);
	else
		return _T("unknown");
}

wxString CTlsSocket::ListTlsCiphers(wxString priority)
{
	if (priority.empty())
		priority = wxString::FromUTF8(ciphers);

	wxString list = wxString::Format(_T("Ciphers for %s:\n"), priority);

	gnutls_priority_t pcache;
	const char *err = 0;
	int ret = gnutls_priority_init(&pcache, priority.mb_str(), &err);
	if (ret < 0) {
		list += wxString::Format(_T("gnutls_priority_init failed with code %d: %s"), ret, err ? wxString::FromUTF8(err) : _T("Unknown error"));
		return list;
	}
	else {
		for (size_t i = 0; ; i++) {
			unsigned int idx;
			ret = gnutls_priority_get_cipher_suite_index(pcache, i, &idx);
			if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
				break;
			if (ret == GNUTLS_E_UNKNOWN_CIPHER_SUITE)
				continue;

			gnutls_protocol_t version;
			unsigned char id[2];
			const char* name = gnutls_cipher_suite_info(idx, id, NULL, NULL, NULL, &version);

			if (name != 0)
			{
				list += wxString::Format(
					_T("%-50s    0x%02x, 0x%02x    %s\n"),
					wxString::FromUTF8(name),
					(unsigned char)id[0],
					(unsigned char)id[1],
					wxString::FromUTF8(gnutls_protocol_get_name(version)));
			}
		}
	}

	return list;
}

int CTlsSocket::DoCallGnutlsRecordRecv(void* data, size_t len)
{
	int res = gnutls_record_recv(m_session, data, len);
	while( (res == GNUTLS_E_AGAIN || res == GNUTLS_E_INTERRUPTED) && m_canReadFromSocket && !gnutls_record_get_direction(m_session)) {
		// Spurious EAGAIN. Can happen if GnuTLS gets a partial
		// record and the socket got closed.
		// The unexpected close is being ignored in this case, unless
		// gnutls_record_recv is being called again.
		// Manually call gnutls_record_recv as in case of eof on the socket,
		// we are not getting another receive event.
		m_pOwner->LogMessage(MessageType::Debug_Verbose, _T("gnutls_record_recv returned spurious EAGAIN"));
		res = gnutls_record_recv(m_session, data, len);
	}

	return res;
}
