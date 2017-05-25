#define FZSFTP_PROTOCOL_VERSION 2

typedef enum
{
    sftpUnknown = -1,
    sftpReply = 0,
    sftpDone,
    sftpError,
    sftpVerbose,
    sftpStatus,
    sftpRecv, /* socket */
    sftpSend, /* socket */
    sftpClose,
    sftpRequest,
    sftpListentry,
    sftpTransfer, /* payload: when written to local file (download) or acknowledged by server (upload) */
    sftpRequestPreamble,
    sftpRequestInstruction,
    sftpUsedQuotaRecv,
    sftpUsedQuotaSend,
    sftpKexAlgorithm,
    sftpKexHash,
    sftpCipherClientToServer,
    sftpCipherServerToClient,
    sftpMacClientToServer,
    sftpMacServerToClient,
    sftpHostkey
} sftpEventTypes;

enum sftpRequestTypes
{
    sftpReqPassword,
    sftpReqHostkey,
    sftpReqHostkeyChanged,
    sftpReqUnknown
};

int fznotify(sftpEventTypes type);
int fzprintf(sftpEventTypes type, const char* p, ...);
int fzprintf_raw(sftpEventTypes type, const char* p, ...);
int fzprintf_raw_untrusted(sftpEventTypes type, const char* p, ...);
int fznotify1(sftpEventTypes type, int data);
