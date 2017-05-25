#include "dbushandler.h"
#include "wxdbusconnection.h"
#include "wxdbusmessage.h"

BEGIN_EVENT_TABLE(CDBusHandler, wxEvtHandler)
EVT_DBUS_SIGNAL(wxID_ANY, CDBusHandler::OnSignal)
EVT_DBUS_ASYNC_RESPONSE(wxID_ANY, CDBusHandler::OnAsyncReply)
END_EVENT_TABLE()

CDBusHandler* CDBusHandler::s_handler = 0;
int CDBusHandler::s_refCount = 0;

CDBusHandler* CDBusHandler::AddRef(CDBusHandlerInterface* interface)
{
	if( !s_refCount ) {
		s_handler = new CDBusHandler;
	}

	s_refCount++;

	s_handler->m_interfaces.push_back(interface);

	return s_handler;
}

void CDBusHandler::Unref(CDBusHandlerInterface* interface)
{
	if( s_refCount > 0 ) {
		for( auto it = s_handler->m_interfaces.begin(); it != s_handler->m_interfaces.end(); ++it ) {
			if( *it == interface ) {
				s_handler->m_interfaces.erase(it);
				break;
			}
		}

		--s_refCount;
		if( !s_refCount ) {
			delete s_handler;
			s_handler = 0;
		}
	}
}

CDBusHandler::CDBusHandler()
{
#ifdef __WXDEBUG__
	m_debug = true;
#else
	m_debug = false;
	wxString v;
	if (wxGetEnv(_T("FZDEBUG"), &v) && v == _T("1"))
		m_debug = true;
#endif
	m_pConnection = new wxDBusConnection(wxID_ANY, this, false);
	if (!m_pConnection->IsConnected())
	{
		if (m_debug)
			printf("wxD-Bus: Could not connect to session bus\n");
		delete m_pConnection;
		m_pConnection = 0;
	}
}

CDBusHandler::~CDBusHandler()
{
	delete m_pConnection;
}

void CDBusHandler::OnSignal(wxDBusConnectionEvent& event)
{
	wxDBusMessage* msg = wxDBusMessage::ExtractFromEvent(&event);
	if( !msg ) {
		return;
	}

	if (msg->GetType() == DBUS_MESSAGE_TYPE_ERROR) {
		if (m_debug)
			printf("wxD-Bus: Signal: Error: %s\n", msg->GetString());
		delete msg;
		return;
	}

	const char* path = msg->GetPath();
	if (!path)
	{
		if (m_debug)
			printf("wxD-Bus: Signal contains no path\n");
		delete msg;
		return;
	}

	if (m_debug)
		printf("wxD-Bus: Signal from %s, member %s\n", msg->GetPath(), msg->GetMember());

	for( auto it = s_handler->m_interfaces.begin(); it != s_handler->m_interfaces.end(); ++it ) {
		if( (*it)->HandleSignal(*msg) ) {
			break;
		}
	}

	delete msg;
}

void CDBusHandler::OnAsyncReply(wxDBusConnectionEvent& event)
{
	wxDBusMessage* msg = wxDBusMessage::ExtractFromEvent(&event);
	if( !msg ) {
		return;
	}

	if (m_debug)
		printf("wxD-Bus: Reply with serial %u\n", msg->GetReplySerial());

	for( auto it = s_handler->m_interfaces.begin(); it != s_handler->m_interfaces.end(); ++it ) {
		if( (*it)->HandleReply(*msg) ) {
			break;
		}
	}

	delete msg;
}
