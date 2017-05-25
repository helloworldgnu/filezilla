#ifndef DBUSHANDLER_HEADER
#define DBUSHANDLER_HEADER

#include <wx/wx.h>

#include <vector>

class wxDBusConnection;
class wxDBusConnectionEvent;
class wxDBusMessage;

class CDBusHandlerInterface
{
public:
	virtual bool HandleReply(wxDBusMessage&) = 0;
	virtual bool HandleSignal(wxDBusMessage&) = 0;
};

class CDBusHandler : public wxEvtHandler
{
private:
	CDBusHandler();
	virtual ~CDBusHandler();

public:
	bool ConfirmEndSession();
	bool Unregister();

	bool SendEvent(bool query, bool* veto = 0);

	wxDBusConnection* Conn() { return m_pConnection; }

	static CDBusHandler* AddRef(CDBusHandlerInterface* interface);
	static void Unref(CDBusHandlerInterface* interface);

	bool Debug() const { return m_debug; }

private:
	static int s_refCount;
	static CDBusHandler* s_handler;
	wxDBusConnection* m_pConnection;
	bool m_debug;

	std::vector<CDBusHandlerInterface*> m_interfaces;

	DECLARE_EVENT_TABLE()
	void OnSignal(wxDBusConnectionEvent& event);
	void OnAsyncReply(wxDBusConnectionEvent& event);
};

#endif
