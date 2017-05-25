#ifndef __TLSSOCKET_H__
#define __TLSSOCKET_H__

#include <gnutls/gnutls.h>
#include "backend.h"
#include "socket.h"

class CControlSocket;
class CTlsSocket final : protected CEventHandler, public CBackend
{
public:
	enum class TlsState
	{
		noconn,
		handshake,
		verifycert,
		conn,
		closing,
		closed
	};

	CTlsSocket(CEventHandler* pEvtHandler, CSocket* pSocket, CControlSocket* pOwner);
	virtual ~CTlsSocket();

	bool Init();
	void Uninit();

	int Handshake(const CTlsSocket* pPrimarySocket = 0, bool try_resume = 0);

	virtual int Read(void *buffer, unsigned int size, int& error);
	virtual int Peek(void *buffer, unsigned int size, int& error);
	virtual int Write(const void *buffer, unsigned int size, int& error);

	int Shutdown();

	void TrustCurrentCert(bool trusted);

	TlsState GetState() const { return m_tlsState; }

	wxString GetProtocolName();
	wxString GetKeyExchange();
	wxString GetCipherName();
	wxString GetMacName();

	bool ResumedSession() const;

	static wxString ListTlsCiphers(wxString priority);

protected:

	bool InitSession();
	void UninitSession();
	bool CopySessionData(const CTlsSocket* pPrimarySocket);

	virtual void OnRateAvailable(enum CRateLimiter::rate_direction direction);

	int ContinueHandshake();
	void ContinueShutdown();

	int VerifyCertificate();

	TlsState m_tlsState{TlsState::noconn};

	CControlSocket* m_pOwner{};

	bool m_initialized{};
	gnutls_session_t m_session{};

	gnutls_certificate_credentials_t m_certCredentials{};

	void LogError(int code, const wxString& function, MessageType logLegel = MessageType::Error);
	void PrintAlert(MessageType logLevel);

	// Failure logs the error, uninits the session and sends a close event
	void Failure(int code, bool send_close, const wxString& function = wxString());

	static ssize_t PushFunction(gnutls_transport_ptr_t ptr, const void* data, size_t len);
	static ssize_t PullFunction(gnutls_transport_ptr_t ptr, void* data, size_t len);
	ssize_t PushFunction(const void* data, size_t len);
	ssize_t PullFunction(void* data, size_t len);

	int DoCallGnutlsRecordRecv(void* data, size_t len);

	void TriggerEvents();

	virtual void operator()(CEventBase const& ev);
	void OnSocketEvent(CSocketEventSource* source, SocketEventType t, int error);

	void OnRead();
	void OnSend();

	bool ExtractCert(gnutls_datum_t const* datum, CCertificate& out);
	std::vector<wxString> GetCertSubjectAltNames(gnutls_x509_crt_t cert);

	bool m_canReadFromSocket{true};
	bool m_canWriteToSocket{true};
	bool m_canCheckCloseSocket{false};

	bool m_canTriggerRead{false};
	bool m_canTriggerWrite{true};

	bool m_socketClosed{};

	CSocketBackend* m_pSocketBackend{};
	CSocket* m_pSocket{};

	bool m_shutdown_requested{};

	// Due to the strange gnutls_record_send semantics, call it again
	// with 0 data and 0 length after GNUTLS_E_AGAIN and store the number
	// of bytes written. These bytes get skipped on next write from the
	// application.
	// This avoids the rule to call it again with the -same- data after
	// GNUTLS_E_AGAIN.
	void CheckResumeFailedReadWrite();
	bool m_lastReadFailed{true};
	bool m_lastWriteFailed{false};
	unsigned int m_writeSkip{};

	// Peek data
	char* m_peekData{};
	unsigned int m_peekDataLen{};

	gnutls_datum_t m_implicitTrustedCert;

	bool m_socket_eof{};
	int m_socket_error{ECONNABORTED}; // Set in the push and pull functions if reading/writing fails fatally
};

#endif //__TLSSOCKET_H__
