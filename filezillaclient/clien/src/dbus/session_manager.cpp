/* This class uses the D-BUS API of the GNOME Session Manager.
 * See http://www.gnome.org/~mccann/gnome-session/docs/gnome-session.html for details
 */
#include <wx/wx.h>
#include "session_manager.h"
#include "dbushandler.h"
#include "wxdbusconnection.h"
#include "wxdbusmessage.h"
#include "dbushandler.h"

enum state
{
	error,
	register_client,
	initialized,
	query_end_session,
	end_session,
	ended_session,
	unregister
};

class CSessionManagerImpl : public CDBusHandlerInterface
{
public:
	CSessionManagerImpl();
	virtual ~CSessionManagerImpl();

	CDBusHandler* m_handler;

	bool ConfirmEndSession();
	bool Unregister();

	bool SendEvent(bool query, bool* veto = 0);

	bool HandleReply(wxDBusMessage& msg);
	bool HandleSignal(wxDBusMessage& msg);

	enum state m_state;

	char* m_client_object_path;

	unsigned int m_serial;
};

static CSessionManagerImpl* the_gnome_session_manager;

CSessionManager::CSessionManager()
{
}

CSessionManager::~CSessionManager()
{
}

CSessionManagerImpl::CSessionManagerImpl()
	: m_client_object_path()
	, m_serial()
{
	m_handler = CDBusHandler::AddRef(this);

	wxDBusConnection* c = m_handler->Conn();
	if( c ) {
		wxDBusMethodCall call(
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			"RegisterClient");

		m_state = register_client;

		char pid[sizeof(unsigned long) * 8 + 10];
		sprintf(pid, "FileZilla %lu", wxGetProcessId());
		call.AddString(pid);
		call.AddString(pid); // What's this "startup identifier"?

		if (!call.CallAsync(c, 1000))
			m_state = error;
		else {
			m_serial = call.GetSerial();
		}
	}
	else {
		m_state = error;
	}
}

CSessionManagerImpl::~CSessionManagerImpl()
{
	CDBusHandler::Unref(this);
	delete [] m_client_object_path;
}

bool CSessionManager::Init()
{
	if (!the_gnome_session_manager)
		the_gnome_session_manager = new CSessionManagerImpl;
	return true;
}

bool CSessionManager::Uninit()
{
	if (the_gnome_session_manager)
	{
		if (!the_gnome_session_manager->ConfirmEndSession())
			the_gnome_session_manager->Unregister();
	}

	delete the_gnome_session_manager;
	the_gnome_session_manager = 0;
	return true;
}

bool CSessionManagerImpl::HandleSignal(wxDBusMessage& msg)
{
	if (!strcmp(msg.GetPath(), "org.gnome.SessionManager"))
		return false;

	if (!m_client_object_path || strcmp(msg.GetPath(), m_client_object_path)) {
		return false;
	}

	if (m_state == error || m_state == unregister)
	{
		if (m_handler->Debug())
			printf("wxD-Bus: OnSignal during bad state: %d\n", m_state);
		return true;
	}


	const char* member = msg.GetMember();
	if (!strcmp(member, "QueryEndSession"))
	{
		m_state = query_end_session;

		bool veto;

		wxUint32 flags;
		if (msg.GetUInt(flags) && flags & 1)
			veto = true;
		else
			veto = false;

		bool res = SendEvent(true, &veto);

		wxDBusMethodCall call(
			"org.gnome.SessionManager",
			m_client_object_path,
			0,//"org.gnome.SessionManager.ClientPrivate",
			"EndSessionResponse");
		call.AddBool(!res || !veto);
		call.AddString("No reason given");
		call.CallAsync(m_handler->Conn(), 1000);
		m_serial = call.GetSerial();
	}
	else if (!strcmp(member, "EndSession"))
	{
		m_state = end_session;

		if (!SendEvent(false))
			ConfirmEndSession();
	}
	return true;
}

bool CSessionManagerImpl::HandleReply(wxDBusMessage& msg)
{
	if( msg.GetReplySerial() != m_serial ) {
		return false;
	}

	if (msg.GetType() == DBUS_MESSAGE_TYPE_ERROR)
	{
		if (m_handler->Debug())
			printf("wxD-Bus: Signal: Error: %s\n", msg.GetString());
		return true;
	}

	if (m_state == register_client)
	{
		const char* obj_path = msg.GetObjectPath();
		if (!obj_path)
		{
			if (m_handler->Debug())
				printf("wxD-Bus: Reply to RegisterClient does not contain our object path\n");
			m_state = error;
		}
		else
		{
			m_client_object_path = new char[strlen(obj_path) + 1];
			strcpy(m_client_object_path, obj_path);
			if (m_handler->Debug())
				printf("wxD-Bus: Reply to RegisterClient, our object path is %s\n", msg.GetObjectPath());
			m_state = initialized;
		}
	}
	else if (m_state == query_end_session)
	{
		m_state = initialized;
	}
	else if (m_state == ended_session)
	{
		if (m_handler->Debug())
			printf("wxD-Bus: Session is over\n");
		m_state = unregister;

		wxDBusMethodCall call(
			"org.gnome.SessionManager",
			"/org/gnome/SessionManager",
			"org.gnome.SessionManager",
			"UnregisterClient");
		call.AddObjectPath(m_client_object_path);

		call.CallAsync(m_handler->Conn(), 1000);
		m_serial = call.GetSerial();
	}
	else if (m_state == unregister)
	{
		// We're so done
		m_state = error;
		if (m_handler->Debug())
			printf("wxD-Bus: Unregistered\n");
	}
	else
	{
		if (m_handler->Debug())
			printf("wxD-Bus: Unexpected reply in state %d\n", m_state);
		m_state = error;
	}

	return true;
}

bool CSessionManagerImpl::ConfirmEndSession()
{
	if (m_state != end_session)
		return false;

	if (m_handler->Debug())
		printf("wxD-Bus: Confirm session end\n");

	m_state = ended_session;
	wxDBusMethodCall call(
		"org.gnome.SessionManager",
		m_client_object_path,
		"org.gnome.SessionManager.ClientPrivate",
		"EndSessionResponse");
	call.AddBool(true);
	call.AddString("");
	call.CallAsync(m_handler->Conn(), 1000);

	return true;
}

bool CSessionManagerImpl::SendEvent(bool query, bool* veto /*=0*/)
{
	wxApp* pApp = (wxApp*)wxApp::GetInstance();
	if (!pApp)
		return false;

	wxWindow* pTop = pApp->GetTopWindow();
	if (!pTop)
		return false;

	wxCloseEvent evt(query ? wxEVT_QUERY_END_SESSION : wxEVT_END_SESSION);
	evt.SetCanVeto(veto && *veto);

	if (!pTop->GetEventHandler()->ProcessEvent(evt))
		return false;

	if (veto)
		*veto = evt.GetVeto();

	return true;
}

bool CSessionManagerImpl::Unregister()
{
	if (m_state == error || m_state == unregister || m_state == register_client)
		return false;

	m_state = unregister;

	wxDBusMethodCall call(
		"org.gnome.SessionManager",
		"/org/gnome/SessionManager",
		"org.gnome.SessionManager",
		"UnregisterClient");
	call.AddObjectPath(m_client_object_path);

	wxDBusMessage *result = call.Call(m_handler->Conn(), 1000);
	if (result)
	{
		delete result;
		if (m_handler->Debug())
			printf("wxD-Bus: Unregistered\n");
	}

	m_state = error;

	return true;
}

