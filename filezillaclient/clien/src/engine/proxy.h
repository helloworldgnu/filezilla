#ifndef __PROXY_H__
#define __PROXY_H__

#include "backend.h"
#include "socket.h"

class CControlSocket;
class CProxySocket final : protected CEventHandler, public CBackend
{
public:
	CProxySocket(CEventHandler* pEvtHandler, CSocket* pSocket, CControlSocket* pOwner);
	virtual ~CProxySocket();

	enum ProxyState {
		noconn,
		handshake,
		conn
	};

	enum ProxyType {
		unknown,
		HTTP,
		SOCKS5,
		SOCKS4,

		proxytype_count
	};

	int Handshake(ProxyType type, const wxString& host, unsigned int port, const wxString& user, const wxString& pass);

	ProxyState GetState() const { return m_proxyState; }

	// Past the initial handshake, proxies are transparent.
	// Class users should detach socket and use a normal socket backend instead.
	virtual void OnRateAvailable(CRateLimiter::rate_direction) {};
	virtual int Read(void *buffer, unsigned int size, int& error);
	virtual int Peek(void *buffer, unsigned int size, int& error);
	virtual int Write(const void *buffer, unsigned int size, int& error);

	void Detach();
	bool Detached() const { return m_pSocket == 0; }

	ProxyType GetProxyType() const { return m_proxyType; }
	wxString GetUser() const { return m_user; }
	wxString GetPass() const { return m_pass; }

protected:
	CSocket* m_pSocket;
	CControlSocket* m_pOwner;

	ProxyType m_proxyType{unknown};
	wxString m_host;
	int m_port{};
	wxString m_user;
	wxString m_pass;

	ProxyState m_proxyState{noconn};

	int m_handshakeState{};

	char* m_pSendBuffer{};
	int m_sendBufferLen{};

	char* m_pRecvBuffer{};
	int m_recvBufferPos{};
	int m_recvBufferLen{};

	virtual void operator()(CEventBase const& ev);
	void OnSocketEvent(CSocketEventSource* source, SocketEventType t, int error);
	void OnHostAddress(CSocketEventSource* source, wxString const& address);

	void OnReceive();
	void OnSend();

	bool m_can_write{};
	bool m_can_read{};
};

#endif //__PROXY_H__
