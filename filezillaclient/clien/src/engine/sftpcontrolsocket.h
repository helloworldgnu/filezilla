#ifndef __SFTPCONTROLSOCKET_H__
#define __SFTPCONTROLSOCKET_H__

#include "ControlSocket.h"
#include <wx/process.h>

enum class sftpEvent {
	Unknown = -1,
	Reply = 0,
	Done,
	Error,
	Verbose,
	Status,
	Recv,
	Send,
	Close,
	Request,
	Listentry,
	Transfer,
	RequestPreamble,
	RequestInstruction,
	UsedQuotaRecv,
	UsedQuotaSend,
	KexAlgorithm,
	KexHash,
	CipherClientToServer,
	CipherServerToClient,
	MacClientToServer,
	MacServerToClient,
	Hostkey,

	max = Hostkey
};

enum sftpRequestTypes
{
	sftpReqPassword,
	sftpReqHostkey,
	sftpReqHostkeyChanged,
	sftpReqUnknown
};

class CProcess;
class CSftpInputThread;

class CSftpControlSocket final : public CControlSocket, public CRateLimiterObject
{
public:
	CSftpControlSocket(CFileZillaEnginePrivate & engine);
	virtual ~CSftpControlSocket();

	virtual int Connect(const CServer &server);

	virtual int List(CServerPath path = CServerPath(), wxString subDir = _T(""), int flags = 0);
	virtual int Delete(const CServerPath& path, const std::list<wxString>& files);
	virtual int RemoveDir(const CServerPath& path = CServerPath(), const wxString& subDir = _T(""));
	virtual int Mkdir(const CServerPath& path);
	virtual int Rename(const CRenameCommand& command);
	virtual int Chmod(const CChmodCommand& command);
	virtual void Cancel();

	virtual bool Connected() { return m_pInputThread != 0; }

	virtual bool SetAsyncRequestReply(CAsyncRequestNotification *pNotification);

protected:
	// Replaces filename"with"quotes with
	// "filename""with""quotes"
	wxString QuoteFilename(wxString filename);

	virtual int DoClose(int nErrorCode = FZ_REPLY_DISCONNECTED);

	virtual int ResetOperation(int nErrorCode);
	virtual int SendNextCommand();
	virtual int ParseSubcommandResult(int prevResult);

	int ProcessReply(bool successful, const wxString& reply = _T(""));

	int ConnectParseResponse(bool successful, const wxString& reply);
	int ConnectSend();

	virtual int FileTransfer(const wxString localFile, const CServerPath &remotePath,
							 const wxString &remoteFile, bool download,
							 const CFileTransferCommand::t_transferSettings& transferSettings);
	int FileTransferSubcommandResult(int prevResult);
	int FileTransferSend();
	int FileTransferParseResponse(bool successful, const wxString& reply);

	int ListSubcommandResult(int prevResult);
	int ListSend();
	int ListParseResponse(bool successful, const wxString& reply);
	int ListParseEntry(const wxString& entry);
	int ListCheckTimezoneDetection();

	int ChangeDir(CServerPath path = CServerPath(), wxString subDir = _T(""), bool link_discovery = false);
	int ChangeDirParseResponse(bool successful, const wxString& reply);
	int ChangeDirSubcommandResult(int prevResult);
	int ChangeDirSend();

	int MkdirParseResponse(bool successful, const wxString& reply);
	int MkdirSend();

	int DeleteParseResponse(bool successful, const wxString& reply);
	int DeleteSend();

	int RemoveDirParseResponse(bool successful, const wxString& reply);

	int ChmodParseResponse(bool successful, const wxString& reply);
	int ChmodSubcommandResult(int prevResult);
	int ChmodSend();

	int RenameParseResponse(bool successful, const wxString& reply);
	int RenameSubcommandResult(int prevResult);
	int RenameSend();

	bool SendCommand(wxString const& cmd, const wxString& show = wxString());
	bool AddToStream(const wxString& cmd, bool force_utf8 = false);

	virtual void OnRateAvailable(CRateLimiter::rate_direction direction);
	void OnQuotaRequest(CRateLimiter::rate_direction direction);

	// see src/putty/wildcard.c
	wxString WildcardEscape(const wxString& file);

	CProcess* m_pProcess{};
	CSftpInputThread* m_pInputThread{};

	virtual void operator()(CEventBase const& ev);
	void OnSftpEvent();
	void OnTerminate();

	wxString m_requestPreamble;
	wxString m_requestInstruction;

	CSftpEncryptionNotification m_sftpEncryptionDetails;
};

#endif //__SFTPCONTROLSOCKET_H__
