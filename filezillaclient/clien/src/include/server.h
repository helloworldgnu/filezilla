#ifndef __SERVER_H__
#define __SERVER_H__

enum ServerProtocol
{
	// Never change any existing values or user's saved sites will become
	// corrupted.
	UNKNOWN = -1,
	FTP, // FTP, attempts AUTH TLS
	SFTP,
	HTTP,
	FTPS, // Implicit SSL
	FTPES, // Explicit SSL
	HTTPS,
	INSECURE_FTP, // Insecure, as the name suggests

	MAX_VALUE = INSECURE_FTP
};

enum ServerType
{
	DEFAULT,
	UNIX,
	VMS,
	DOS,
	MVS,
	VXWORKS,
	ZVM,
	HPNONSTOP,
	DOS_VIRTUAL,
	CYGWIN,

	SERVERTYPE_MAX
};

enum LogonType
{
	ANONYMOUS,
	NORMAL,
	ASK, // ASK should not be sent to the engine, it's intendet to be used by the interface
	INTERACTIVE,
	ACCOUNT,

	LOGONTYPE_MAX
};

enum PasvMode
{
	MODE_DEFAULT,
	MODE_ACTIVE,
	MODE_PASSIVE
};

enum CharsetEncoding
{
	ENCODING_AUTO,
	ENCODING_UTF8,
	ENCODING_CUSTOM
};

class CServerPath;
class CServer final
{
public:

	// No error checking is done in the constructors
	CServer();
	CServer(wxString host, unsigned int);
	CServer(wxString host, unsigned int, wxString user, wxString pass = wxString());
	CServer(ServerProtocol protocol, ServerType type, wxString host, unsigned int);
	CServer(ServerProtocol protocol, ServerType type, wxString host, unsigned int, wxString user, wxString pass = wxString(), wxString account = wxString());

	void SetType(ServerType type);

	ServerProtocol GetProtocol() const;
	ServerType GetType() const;
	wxString GetHost() const;
	unsigned int GetPort() const;
	LogonType GetLogonType() const;
	wxString GetUser() const;
	wxString GetPass() const;
	wxString GetAccount() const;
	int GetTimezoneOffset() const;
	PasvMode GetPasvMode() const;
	int MaximumMultipleConnections() const;
	bool GetBypassProxy() const;

	// Return true if URL could be parsed correctly, false otherwise.
	// If parsing fails, pError is filled with the reason and the CServer instance may be left an undefined state.
	bool ParseUrl(wxString host, unsigned int port, wxString user, wxString pass, wxString &error, CServerPath &path);
	bool ParseUrl(wxString host, wxString port, wxString user, wxString pass, wxString &error, CServerPath &path);

	void SetProtocol(ServerProtocol serverProtocol);
	bool SetHost(wxString Host, unsigned int port);

	void SetLogonType(LogonType logonType);
	bool SetUser(const wxString& user, const wxString& pass = wxString());
	bool SetAccount(const wxString& account);

	CServer& operator=(const CServer &op);
	bool operator==(const CServer &op) const;
	bool operator<(const CServer &op) const;
	bool operator!=(const CServer &op) const;
	bool EqualsNoPass(const CServer &op) const;

	bool SetTimezoneOffset(int minutes);
	void SetPasvMode(PasvMode pasvMode);
	void MaximumMultipleConnections(int maximum);

	wxString FormatHost(bool always_omit_port = false) const;
	wxString FormatServer(const bool always_include_prefix = false) const;

	bool SetEncodingType(CharsetEncoding type, const wxString& encoding = wxString());
	bool SetCustomEncoding(const wxString& encoding);
	CharsetEncoding GetEncodingType() const;
	wxString GetCustomEncoding() const;

	static unsigned int GetDefaultPort(ServerProtocol protocol);
	static ServerProtocol GetProtocolFromPort(unsigned int port, bool defaultOnly = false);

	static wxString GetProtocolName(ServerProtocol protocol);
	static ServerProtocol GetProtocolFromName(const wxString& name);

	static ServerProtocol GetProtocolFromPrefix(const wxString& prefix);
	static wxString GetPrefixFromProtocol(const ServerProtocol protocol);

	// Some protocol distinguish between ASCII and binary files for line-ending
	// conversion.
	static bool ProtocolHasDataTypeConcept(ServerProtocol const protocol);

	// These commands will be executed after a successful login.
	const std::vector<wxString>& GetPostLoginCommands() const { return m_postLoginCommands; }
	bool SetPostLoginCommands(const std::vector<wxString>& postLoginCommands);
	static bool SupportsPostLoginCommands(ServerProtocol const protocol);

	void SetBypassProxy(bool val);

	// Abstract server name.
	// Not compared in ==, < and related operators
	void SetName(const wxString& name) { m_name = name; }
	wxString GetName() const { return m_name; }

	static wxString GetNameFromServerType(ServerType type);
	static ServerType GetServerTypeFromName(const wxString& name);

	static wxString GetNameFromLogonType(LogonType type);
	static LogonType GetLogonTypeFromName(const wxString& name);

protected:
	void Initialize();

	ServerProtocol m_protocol;
	ServerType m_type;
	wxString m_host;
	unsigned int m_port;
	LogonType m_logonType;
	wxString m_user;
	wxString m_pass;
	wxString m_account;
	int m_timezoneOffset;
	PasvMode m_pasvMode;
	int m_maximumMultipleConnections;
	CharsetEncoding m_encodingType;
	wxString m_customEncoding;
	wxString m_name;

	std::vector<wxString> m_postLoginCommands;
	bool m_bypassProxy;
};

#endif
