#ifndef ACCOUNTS_H_INCLUDED
#define ACCOUNTS_H_INCLUDED

#ifdef _AFX
#define CStdString CString
#define CStdStringW CStringW
#define CStdStringA CStringA
#endif //_AFX

#include "SpeedLimit.h"

class t_directory
{
public:
	CStdString dir;
	std::vector<CStdString> aliases;
	BOOL bFileRead{}, bFileWrite{}, bFileDelete{}, bFileAppend{};
	BOOL bDirCreate{}, bDirDelete{}, bDirList{}, bDirSubdirs{}, bIsHome{};
	BOOL bAutoCreate{};
};

enum sltype
{
	download = 0,
	upload = 1
};

class t_group
{
public:
	t_group();
	virtual ~t_group() {}

	virtual int GetRequiredBufferLen() const;
    virtual int GetRequiredStringBufferLen(const CStdString& str) const;
	virtual unsigned char * FillBuffer(unsigned char *p) const;
    virtual void FillString(unsigned char *&p, CStdString const& str) const;
	virtual unsigned char * ParseBuffer(unsigned char *pBuffer, int length);

	virtual bool BypassUserLimit() const;
	virtual int GetUserLimit() const;
	virtual int GetIpLimit() const;
	virtual bool IsEnabled() const;
	virtual bool ForceSsl() const;

	virtual int GetCurrentSpeedLimit(sltype type) const;
	virtual bool BypassServerSpeedLimit(sltype type) const;

	bool AccessAllowed(const CStdString& ip) const;

	CStdString group;
	std::vector<t_directory> permissions;
	int nBypassUserLimit{};
	int nUserLimit{}, nIpLimit{};
	int nEnabled{};
	int forceSsl{};

	int nSpeedLimitType[2];
	int nSpeedLimit[2];
	SPEEDLIMITSLIST SpeedLimits[2];
	int nBypassServerSpeedLimit[2];

	std::vector<CStdString> allowedIPs, disallowedIPs;

	CStdString comment;

	t_group const* pOwner{};

	bool b8plus3{};

protected:
	bool ParseString(const unsigned char* endMarker, unsigned char *&p, CStdString &string);
};

class t_user : public t_group
{
public:
	t_user();

	virtual int GetRequiredBufferLen() const;
	virtual unsigned char * FillBuffer(unsigned char *p) const;
	virtual unsigned char * ParseBuffer(unsigned char *pBuffer, int length);
	void generateSalt(); // Generates a new random salt of length 64, using all printable ASCII characters.

	CStdString user;
	CStdString password;
	CStdString salt;
};

#endif //#define ACCOUNTS_H_INCLUDED
