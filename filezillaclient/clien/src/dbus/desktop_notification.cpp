#include <wx/wx.h>
#include "dbushandler.h"
#include "desktop_notification.h"
#include "wxdbusconnection.h"
#include "wxdbusmessage.h"
#include <list>

class CDesktopNotificationImpl : private CDBusHandlerInterface
{
public:
	CDesktopNotificationImpl();
	virtual ~CDesktopNotificationImpl();

	void Notify(const wxString& summary, const wxString& body, const wxString& category);

protected:
	CDBusHandler* m_handler;

	void EmitNotifications();

	enum _state
	{
		error,
		idle,
		busy
	} m_state;

	struct _notification
	{
		wxString summary;
		wxString body;
		wxString category;
	};
	std::list<struct _notification> m_notifications;

	bool HandleReply(wxDBusMessage& msg);
	bool HandleSignal(wxDBusMessage& msg);

	unsigned int m_serial;
};

CDesktopNotification::CDesktopNotification()
{
	impl = new CDesktopNotificationImpl;
}

CDesktopNotification::~CDesktopNotification()
{
	delete impl;
}

void CDesktopNotification::Notify(const wxString& summary, const wxString& body, const wxString& category)
{
	impl->Notify(summary, body, category);
}

CDesktopNotificationImpl::CDesktopNotificationImpl()
	: m_serial()
{
	m_handler = CDBusHandler::AddRef(this);

	if (m_handler->Conn()) {
		m_state = idle;
	}
	else {
		m_state = error;
	}
}

CDesktopNotificationImpl::~CDesktopNotificationImpl()
{
	CDBusHandler::Unref(this);
}

void CDesktopNotificationImpl::Notify(const wxString& summary, const wxString& body, const wxString& category)
{
	if (m_state == error)
		return;

	struct _notification notification;
	notification.summary = summary;
	notification.body = body;
	notification.category = category;

	m_notifications.push_back(notification);

	EmitNotifications();
}

void CDesktopNotificationImpl::EmitNotifications()
{
	if (m_state != idle)
		return;

	if (m_notifications.empty())
		return;

	m_state = busy;

	struct _notification notification = m_notifications.front();
	m_notifications.pop_front();

	wxDBusMethodCall *call = new wxDBusMethodCall(
		"org.freedesktop.Notifications",
		"/org/freedesktop/Notifications",
		"org.freedesktop.Notifications",
		"Notify");

	call->AddString("FileZilla");
	call->AddUnsignedInt(0);
	call->AddString("filezilla");
	call->AddString(notification.summary.mb_str(wxConvUTF8));
	call->AddString(notification.body.mb_str(wxConvUTF8));
	call->AddArrayOfString(0, 0);

	if (!notification.category.empty())
	{
		const wxWX2MBbuf category = notification.category.mb_str(wxConvUTF8);
		const char *hints[2];
		hints[0] = "category";
		hints[1] = (const char*)category;

		call->AddDict(hints, 2);
	}
	else
		call->AddDict(0, 0);

	call->AddInt(-1);

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

bool CDesktopNotificationImpl::HandleSignal(wxDBusMessage&)
{
	return false;
}

bool CDesktopNotificationImpl::HandleReply(wxDBusMessage& msg)
{
	if( msg.GetReplySerial() != m_serial ) {
		return false;
	}

	if (msg.GetType() == DBUS_MESSAGE_TYPE_ERROR)
	{
		if (m_handler->Debug())
			printf("wxD-Bus: Reply: Error: %s\n", msg.GetString());

		m_state = error;
	}
	else {
		m_state = idle;
	}
	EmitNotifications();
	return true;
}
