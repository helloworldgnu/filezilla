#ifndef __NOTIFICATION_H__
#define __NOTIFICATION_H__

// Notification overview
// ---------------------

// To inform the application about what's happening, the engine sends
// some notifications to the application using events.
// For example the event table entry could look like this:
//     EVT_FZ_NOTIFICATION(wxID_ANY, CMainFrame::OnEngineEvent)
// and the handler function has the following declaration:
//     void OnEngineEvent(wxFzEvent& event);
// You can get the engine which sent the event by reading
//     event.engine_
// Whenever you get a notification event,
// CFileZillaEngine::GetNextNotification has to be called until it returns 0,
// or you will lose important notifications or your memory will fill with
// pending notifications.

// A special class of notifications are the asynchronous requests. These
// requests have to be answered. Once proessed, call
// CFileZillaEngine::SetAsyncRequestReply to continue the current operation.

#include "local_path.h"
#include "timeex.h"

class CFileZillaEngine;
class wxFzEvent final : public wxEvent
{
public:
	wxFzEvent();

	explicit wxFzEvent(int id);
	explicit wxFzEvent(CFileZillaEngine const* engine);

	virtual wxEvent *Clone() const;

	CFileZillaEngine const* engine_;
};

wxDECLARE_EVENT(fzEVT_NOTIFICATION, wxFzEvent);

#define EVT_FZ_NOTIFICATION(id, func) \
	wx__DECLARE_EVT1(fzEVT_NOTIFICATION, id, &func)

enum NotificationId
{
	nId_logmsg,				// notification about new messages for the message log
	nId_operation,			// operation reply codes
	nId_connection,			// connection information: connects, disconnects, timeouts etc..
	nId_transferstatus,		// transfer information: bytes transferes, transfer speed and such
	nId_listing,			// directory listings
	nId_asyncrequest,		// asynchronous request
	nId_active,				// sent if data gets either received or sent
	nId_data,				// for memory downloads, indicates that new data is available.
	nId_sftp_encryption,	// information about key exchange, encryption algorithms and so on for SFTP
	nId_local_dir_created	// local directory has been created
};

// Async request IDs
enum RequestId
{
	reqId_fileexists,		// Target file already exists, awaiting further instructions
	reqId_interactiveLogin, // gives a challenge prompt for a password
	reqId_hostkey,			// used only by SSH/SFTP to indicate new host key
	reqId_hostkeyChanged,	// used only by SSH/SFTP to indicate changed host key
	reqId_certificate		// sent after a successful TLS handshake to allow certificate
							// validation
};

class CNotification
{
public:
	virtual ~CNotification() {}; // TODO: One GCC >= 4.8 is in Debian Stable (Jessie by then), make default and add testcase to configure.
	virtual NotificationId GetID() const = 0;

protected:
	CNotification() = default;
	CNotification(CNotification const&) = default;
	CNotification& operator=(CNotification const&) = default;
};

template<NotificationId id>
class CNotificationHelper : public CNotification
{
public:
	virtual NotificationId GetID() const final { return id; }

protected:
	CNotificationHelper<id>() = default;
	CNotificationHelper<id>(CNotificationHelper<id> const&) = default;
	CNotificationHelper<id>& operator=(CNotificationHelper<id> const&) = default;
};

class CLogmsgNotification final : public CNotificationHelper<nId_logmsg>
{
public:
	explicit CLogmsgNotification(MessageType t)
		: msgType(t)
	{}

	template<typename String>
	CLogmsgNotification(MessageType t, String && m)
		: msg(std::forward<String>(m))
		, msgType(t)
	{
	}

	wxString msg;
	MessageType msgType{MessageType::Status}; // Type of message, see logging.h for details
};

// If CFileZillaEngine does return with FZ_REPLY_WOULDBLOCK, you will receive
// a nId_operation notification once the operation ends.
class COperationNotification final : public CNotificationHelper<nId_operation>
{
public:
	int nReplyCode{};
	Command commandId{Command::none};
};

// You get this type of notification everytime a directory listing has been
// requested explicitely or when a directory listing was retrieved implicitely
// during another operation, e.g. file transfers.
class CDirectoryListing;
class CDirectoryListingNotification final : public CNotificationHelper<nId_listing>
{
public:
	explicit CDirectoryListingNotification(const CServerPath& path, const bool modified = false, const bool failed = false);
	bool Modified() const { return m_modified; }
	bool Failed() const { return m_failed; }
	const CServerPath GetPath() const { return m_path; }

protected:
	bool m_modified{};
	bool m_failed{};
	CServerPath m_path;
};

class CAsyncRequestNotification : public CNotificationHelper<nId_asyncrequest>
{
public:
	virtual enum RequestId GetRequestID() const = 0;
	unsigned int requestNumber{}; // Do never change this

protected:
	CAsyncRequestNotification() = default;
	CAsyncRequestNotification(CAsyncRequestNotification const&) = default;
	CAsyncRequestNotification& operator=(CAsyncRequestNotification const&) = default;
};

class CFileExistsNotification final : public CAsyncRequestNotification
{
public:
	virtual enum RequestId GetRequestID() const;

	bool download{};

	wxString localFile;
	wxLongLong localSize{-1};
	CDateTime localTime;

	wxString remoteFile;
	CServerPath remotePath;
	wxLongLong remoteSize{-1};
	CDateTime remoteTime;

	bool ascii{};

	bool canResume{};

	// overwriteAction will be set by the request handler
	enum OverwriteAction : signed char
	{
		unknown = -1,
		ask,
		overwrite,
		overwriteNewer,	// Overwrite if source file is newer than target file
		overwriteSize,	// Overwrite if source file is is different in size than target file
		overwriteSizeOrNewer,	// Overwrite if source file is different in size or newer than target file
		resume, // Overwrites if cannot be resumed
		rename,
		skip,

		ACTION_COUNT
	};

	// Set overwriteAction to the desired action
	OverwriteAction overwriteAction{unknown};

	// Set to new filename if overwriteAction is rename. Might trigger further
	// file exists notifications if new target file exists as well.
	wxString newName;
};

class CInteractiveLoginNotification final : public CAsyncRequestNotification
{
public:
	CInteractiveLoginNotification(const wxString& challenge);
	virtual enum RequestId GetRequestID() const;

	// Set to true if you have set a password
	bool passwordSet{};

	// Set password by calling server.SetUser
	CServer server;

	const wxString& GetChallenge() const { return m_challenge; }

protected:
	// Password prompt string as given by the server
	const wxString m_challenge;
};

// Indicate network action.
class CActiveNotification final : public CNotificationHelper<nId_active>
{
public:
	explicit CActiveNotification(int direction);

	int GetDirection() const { return m_direction; }
protected:
	const int m_direction;
};

class CTransferStatus final
{
public:
	CTransferStatus() {}
	CTransferStatus(wxFileOffset total, wxFileOffset start, bool l)
		: totalSize(total)
		, startOffset(start)
		, currentOffset(start)
		, list(l)
	{}

	wxDateTime started;
	wxFileOffset totalSize{-1};		// Total size of the file to transfer, -1 if unknown
	wxFileOffset startOffset{-1};
	wxFileOffset currentOffset{-1};

	void clear() { startOffset = -1; }
	bool empty() const { return startOffset < 0; }

	explicit operator bool() const { return !empty(); }

	// True on download notifications iff currentOffset != startOffset.
	// True on FTP upload notifications iff currentOffset != startOffset
	// AND after the first accepted data after the first wxSOCKET_WOULDBLOCK.
	// SFTP uploads: Set to true if currentOffset >= startOffset + 65536.
	bool madeProgress{};

	bool list{};
};

class CTransferStatusNotification final : public CNotificationHelper<nId_transferstatus>
{
public:
	CTransferStatusNotification() {}
	CTransferStatusNotification(CTransferStatus const& status);

	CTransferStatus const& GetStatus() const;

protected:
	CTransferStatus const status_;
};

// Notification about new or changed hostkeys, only used by SSH/SFTP transfers.
// GetRequestID() returns either reqId_hostkey or reqId_hostkeyChanged
class CHostKeyNotification final : public CAsyncRequestNotification
{
public:
	CHostKeyNotification(wxString host, int port, wxString fingerprint, bool changed = false);
	virtual enum RequestId GetRequestID() const;

	wxString GetHost() const;
	int GetPort() const;
	wxString GetFingerprint() const;

	// Set to true if you trust the server
	bool m_trust{};

	// If m_truest is true, set this to true to always trust this server
	// in future.
	bool m_alwaysTrust{};

protected:

	const wxString m_host;
	const int m_port;
	const wxString m_fingerprint;
	const bool m_changed;
};

class CDataNotification final : public CNotificationHelper<nId_data>
{
public:
	CDataNotification(char* pData, int len);
	virtual ~CDataNotification();

	char* Detach(int& len);

protected:
	char* m_pData;
	unsigned int m_len;
};

class CCertificate final
{
public:
	CCertificate() = default;
	CCertificate(
		unsigned char const* rawData, unsigned int len,
		wxDateTime const& activationTime, wxDateTime const& expirationTime,
		wxString const& serial,
		wxString const& pkalgoname, unsigned int bits,
		wxString const& signalgoname,
		wxString const& fingerprint_sha256,
		wxString const& fingerprint_sha1,
		wxString const& issuer,
		wxString const& subject,
		std::vector<wxString> const& altSubjectNames);

	CCertificate(CCertificate const& op);
	~CCertificate();

	const unsigned char* GetRawData(unsigned int& len) const { len = m_len; return m_rawData; }
	wxDateTime GetActivationTime() const { return m_activationTime; }
	wxDateTime GetExpirationTime() const { return m_expirationTime; }

	const wxString& GetSerial() const { return m_serial; }
	const wxString& GetPkAlgoName() const { return m_pkalgoname; }
	unsigned int GetPkAlgoBits() const { return m_pkalgobits; }

	const wxString& GetSignatureAlgorithm() const { return m_signalgoname; }

	const wxString& GetFingerPrintSHA256() const { return m_fingerprint_sha256; }
	const wxString& GetFingerPrintSHA1() const { return m_fingerprint_sha1; }

	const wxString& GetSubject() const { return m_subject; }
	const wxString& GetIssuer() const { return m_issuer; }

	CCertificate& operator=(CCertificate const& op);

	std::vector<wxString> const& GetAltSubjectNames() const { return m_altSubjectNames; }

private:
	wxDateTime m_activationTime;
	wxDateTime m_expirationTime;

	unsigned char* m_rawData{};
	unsigned int m_len{};

	wxString m_serial;
	wxString m_pkalgoname;
	unsigned int m_pkalgobits{};

	wxString m_signalgoname;

	wxString m_fingerprint_sha256;
	wxString m_fingerprint_sha1;

	wxString m_issuer;
	wxString m_subject;

	std::vector<wxString> m_altSubjectNames;
};

class CCertificateNotification final : public CAsyncRequestNotification
{
public:
	CCertificateNotification(const wxString& host, unsigned int port,
		const wxString& protocol,
		const wxString& keyExchange,
		const wxString& sessionCipher,
		const wxString& sessionMac,
		const std::vector<CCertificate> &certificates);
	virtual enum RequestId GetRequestID() const { return reqId_certificate; }

	const wxString& GetHost() const { return m_host; }
	unsigned int GetPort() const { return m_port; }

	const wxString& GetSessionCipher() const { return m_sessionCipher; }
	const wxString& GetSessionMac() const { return m_sessionMac; }

	bool m_trusted{};

	const std::vector<CCertificate> GetCertificates() const { return m_certificates; }

	const wxString& GetProtocol() const { return m_protocol; }
	const wxString& GetKeyExchange() const { return m_keyExchange; }

protected:
	wxString m_host;
	unsigned int m_port{};

	wxString m_protocol;
	wxString m_keyExchange;
	wxString m_sessionCipher;
	wxString m_sessionMac;

	std::vector<CCertificate> m_certificates;
};

class CSftpEncryptionNotification : public CNotificationHelper<nId_sftp_encryption>
{
public:
	wxString hostKey;
	wxString kexAlgorithm;
	wxString kexHash;
	wxString cipherClientToServer;
	wxString cipherServerToClient;
	wxString macClientToServer;
	wxString macServerToClient;
};

class CLocalDirCreatedNotification : public CNotificationHelper<nId_local_dir_created>
{
public:
	CLocalPath dir;
};

#endif
