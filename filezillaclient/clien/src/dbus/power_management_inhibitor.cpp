#include <wx/wx.h>
#include "dbushandler.h"
#include "power_management_inhibitor.h"
#include "wxdbusconnection.h"
#include "wxdbusmessage.h"

class CPowerManagementInhibitorImpl : private CDBusHandlerInterface
{
public:
	CPowerManagementInhibitorImpl();
	virtual ~CPowerManagementInhibitorImpl();

	void RequestIdle();
	void RequestBusy();
private:
	CDBusHandler* m_handler;

	enum _state
	{
		error,
		idle,
		request_busy,
		busy,
		request_idle
	};

	bool HandleReply(wxDBusMessage& msg);
	bool HandleSignal(wxDBusMessage& msg);

	enum _state m_state;

	enum _state m_intended_state;
	unsigned int m_cookie;

	bool m_use_gsm;

	unsigned int m_serial;
};

CPowerManagementInhibitor::CPowerManagementInhibitor()
{
	impl = new CPowerManagementInhibitorImpl();
}

CPowerManagementInhibitor::~CPowerManagementInhibitor()
{
	delete impl;
}

void CPowerManagementInhibitor::RequestBusy()
{
	impl->RequestBusy();
}

void CPowerManagementInhibitor::RequestIdle()
{
	impl->RequestIdle();
}

CPowerManagementInhibitorImpl::CPowerManagementInhibitorImpl()
	: m_serial()
{
	m_handler = CDBusHandler::AddRef(this);

	if (m_handler->Conn()) {
		m_state = idle;
	}
	else {
		m_state = error;
	}

	m_intended_state = idle;
	m_cookie = 0;
	m_use_gsm = false;
}

CPowerManagementInhibitorImpl::~CPowerManagementInhibitorImpl()
{
	// Closing connection clears the inhibition
	CDBusHandler::Unref(this);
}

void CPowerManagementInhibitorImpl::RequestIdle()
{
	m_intended_state = idle;
	if (m_state == error || m_state == idle || m_state == request_idle || m_state == request_busy)
		return;

	if (m_handler->Debug())
		printf("wxD-Bus: CPowerManagementInhibitor: Requesting idle\n");

	wxDBusMethodCall *call;
	if (!m_use_gsm)
		call = new wxDBusMethodCall(
			"org.freedesktop.PowerManagement",
			"/org/freedesktop/PowerManagement/Inhibit",
			"org.freedesktop.PowerManagement.Inhibit",
			"UnInhibit");
	else
		call = new wxDBusMethodCall(
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			"Uninhibit");

	m_state = request_idle;

	call->AddUnsignedInt(m_cookie);

	if (!call->CallAsync(m_handler->Conn(), 1000))
	{
		m_state = error;
		if (m_handler->Debug())
			printf("wxD-Bus: CPowerManagementInhibitor: Request failed\n");
	}
	else {
		m_serial = call->GetSerial();
	}

	delete call;
}


void CPowerManagementInhibitorImpl::RequestBusy()
{
	m_intended_state = busy;
	if (m_state == error || m_state == busy || m_state == request_busy || m_state == request_idle)
		return;

	if (m_handler->Debug())
		printf("wxD-Bus: CPowerManagementInhibitor: Requesting busy\n");

	wxDBusMethodCall *call;
	if (!m_use_gsm)
		call = new wxDBusMethodCall(
			"org.freedesktop.PowerManagement",
			"/org/freedesktop/PowerManagement/Inhibit",
			"org.freedesktop.PowerManagement.Inhibit",
			"Inhibit");
	else
		call = new wxDBusMethodCall(
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			"Inhibit");

	m_state = request_busy;

	call->AddString("FileZilla");
	if (m_use_gsm)
		call->AddUnsignedInt(0);
	call->AddString("File transfer or remote operation in progress");
	if (m_use_gsm)
		call->AddUnsignedInt(8);

	if (!call->CallAsync(m_handler->Conn(), 1000))
	{
		if (m_handler->Debug())
			printf("wxD-Bus: CPowerManagementInhibitor: Request failed\n");
		if (m_use_gsm)
			m_state = error;
		else
		{
			if (m_handler->Debug())
				printf("wxD-Bus: Falling back to org.gnome.SessionManager\n");
			m_use_gsm = true;
			RequestBusy();
		}
	}
	else {
		m_serial = call->GetSerial();
	}

	delete call;
}

bool CPowerManagementInhibitorImpl::HandleSignal(wxDBusMessage&)
{
	return false;
}

bool CPowerManagementInhibitorImpl::HandleReply(wxDBusMessage& msg)
{
	if( msg.GetReplySerial() != m_serial ) {
		return false;
	}

	if (msg.GetType() == DBUS_MESSAGE_TYPE_ERROR)
	{
		if (m_handler->Debug())
			printf("wxD-Bus: Reply: Error: %s\n", msg.GetString());

		if (m_state == request_busy && !m_use_gsm)
		{
			if (m_handler->Debug())
				printf("wxD-Bus: Falling back to org.gnome.SessionManager\n");
			m_use_gsm = true;
			m_state = idle;
			if (m_intended_state == busy)
				RequestBusy();
		}
		else
			m_state = error;
	}
	else if (m_state == request_idle)
	{
		m_state = idle;
		if (m_handler->Debug())
			printf("wxD-Bus: CPowerManagementInhibitor: Request successful\n");
		if (m_intended_state == busy)
			RequestBusy();
	}
	else if (m_state == request_busy)
	{
		m_state = busy;
		msg.GetUInt(m_cookie);
		if (m_handler->Debug())
			printf("wxD-Bus: CPowerManagementInhibitor: Request successful, cookie is %u\n", m_cookie);
		if (m_intended_state == idle)
			RequestIdle();
	}
	else
	{
		if (m_handler->Debug())
			printf("wxD-Bus: Unexpected reply in state %d\n", m_state);
		m_state = error;
	}

	return true;
}
