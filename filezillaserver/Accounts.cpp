#include "stdafx.h"
#include "accounts.h"
#include "iputils.h"

#include <random>

t_group::t_group()
{
	for (int i = 0; i < 2; ++i) {
		nSpeedLimitType[i] = 0;
		nSpeedLimit[i] = 10;
		nBypassServerSpeedLimit[i] = 0;
	}
}

bool t_group::BypassUserLimit() const
{
	if (!nBypassUserLimit)
		return false;
	if (nBypassUserLimit == 2 && pOwner)
		return pOwner->BypassUserLimit();
	return true;
}

int t_group::GetIpLimit() const
{
	if (nIpLimit)
		return nIpLimit;
	if (pOwner)
		return pOwner->GetIpLimit();
	return 0;
}

int t_group::GetUserLimit() const
{
	if (nUserLimit)
		return nUserLimit;
	if (pOwner)
		return pOwner->GetUserLimit();
	return 0;
}

t_user::t_user()
{
}

unsigned char * t_group::ParseBuffer(unsigned char *pBuffer, int length)
{
	unsigned char *p = pBuffer;
	unsigned char *endMarker = pBuffer + length;

	if (!ParseString(endMarker, p, group))
		return 0;

	if ((endMarker - p) < 11)
		return NULL;

	memcpy(&nIpLimit, p, 4);
	p += 4;
	memcpy(&nUserLimit, p, 4);
	p += 4;

	int options = *p++;

	nBypassUserLimit	= options & 0x03;
	nEnabled			= (options >> 2) & 0x03;

	// Parse IP filter rules.
	int numDisallowedIPs = (int(*p) << 8) + p[1];
	p += 2;
	while (numDisallowedIPs--)
	{
		CStdString ip;
		if (!ParseString(endMarker, p, ip))
			return 0;

		if (IsValidAddressFilter(ip) || ip == _T("*"))
			disallowedIPs.push_back(ip);
	}

	if ((endMarker - p) < 2)
		return NULL;

	int numAllowedIPs = (int(*p) << 8) + p[1];
	p += 2;
	while (numAllowedIPs--)
	{
		CStdString ip;
		if (!ParseString(endMarker, p, ip))
			return 0;

		if (IsValidAddressFilter(ip) || ip == _T("*"))
			allowedIPs.push_back(ip);
	}

	if ((endMarker - p) < 2)
		return NULL;

	int dircount = (int(*p) << 8) + p[1];
	p += 2;

	BOOL bGotHome = FALSE;

	for (int j = 0; j < dircount; j++)
	{
		t_directory dir;

		CStdString str;
		if (!ParseString(endMarker, p, str))
			return 0;

		str.TrimRight(_T("\\"));
		if (str == _T(""))
			return 0;

		dir.dir = str;

		// Get directory aliases.
		if ((endMarker - p) < 2)
			return NULL;

		int aliascount = (int(*p) << 8) + p[1];
		p += 2;

		for (int i = 0; i < aliascount; i++)
		{
			CStdString alias;
			if (!ParseString(endMarker, p, alias))
				return 0;

			alias.TrimRight(_T("\\"));

			if (alias == _T(""))
				return 0;

			dir.aliases.push_back(alias);
		}

		if ((endMarker - p) < 2)
			return NULL;

		int rights = (int(*p) << 8) + p[1];
		p += 2;

		dir.bDirCreate	= rights & 0x0001 ? 1:0;
		dir.bDirDelete	= rights & 0x0002 ? 1:0;
		dir.bDirList	= rights & 0x0004 ? 1:0;
		dir.bDirSubdirs	= rights & 0x0008 ? 1:0;
		dir.bFileAppend	= rights & 0x0010 ? 1:0;
		dir.bFileDelete	= rights & 0x0020 ? 1:0;
		dir.bFileRead	= rights & 0x0040 ? 1:0;
		dir.bFileWrite	= rights & 0x0080 ? 1:0;
		dir.bIsHome		= rights & 0x0100 ? 1:0;
		dir.bAutoCreate	= rights & 0x0200 ? 1:0;

		// Avoid multiple home directories.
		if (dir.bIsHome)
			if (!bGotHome)
				bGotHome = TRUE;
			else
				dir.bIsHome = FALSE;

		permissions.push_back(dir);
	}

	for (int i = 0; i < 2; i++)
	{
		if ((endMarker - p) < 5)
			return NULL;

		nSpeedLimitType[i] = *p & 3;
		nBypassServerSpeedLimit[i] = (*p++ >> 2) & 3;

		nSpeedLimit[i] = int(*p++) << 8;
		nSpeedLimit[i] |= *p++;

		if (!nSpeedLimit[i])
			nSpeedLimit[i] = 10;

		int num = (int(*p) << 8) + p[1];
		p += 2;
		while (num--)
		{
			CSpeedLimit sl;
			p = sl.ParseBuffer(p, length-(int)(p-pBuffer));
			if (!p)
				return NULL;
			SpeedLimits[i].push_back(sl);
		}
	}

	if (!ParseString(endMarker, p, comment))
		return 0;

	if (p >= endMarker)
		return 0;

	forceSsl = *p++;

	return p;
}

int t_group::GetRequiredStringBufferLen(const CStdString& str) const
{
	auto utf8 = ConvToNetwork(str);
	return utf8.size() + 2;
}

void t_group::FillString(unsigned char *& p, CStdString const& str) const
{
	auto utf8 = ConvToNetwork(str);

	size_t len = utf8.size();
	*p++ = (len >> 8) & 0xffu;
	*p++ = len & 0xffu;

	memcpy(p, utf8.c_str(), len);
	p += len;
}

unsigned char * t_group::FillBuffer(unsigned char *p) const
{
	FillString(p, group);

	memcpy(p, &nIpLimit, 4);
	p += 4;
	memcpy(p, &nUserLimit, 4);
	p += 4;

	int options = nBypassUserLimit & 3;
	options |= (nEnabled & 3) << 2;

	*p++ = options & 0xff;

	*p++ = (char)(disallowedIPs.size() >> 8);
	*p++ = (char)(disallowedIPs.size() & 0xff);
	for (auto const& disallowedIP : disallowedIPs) {
		FillString(p, disallowedIP);
	}

	*p++ = (char)(allowedIPs.size() >> 8);
	*p++ = (char)(allowedIPs.size() & 0xff);
	for (auto const& allowedIP : allowedIPs) {
		FillString(p, allowedIP);
	}

	*p++ = (char)(permissions.size() >> 8);
	*p++ = (char)(permissions.size() & 0xff);
	for (auto const& permission : permissions) {
		FillString(p, permission.dir);

		*p++ = (char)(permission.aliases.size() >> 8);
		*p++ = (char)(permission.aliases.size() & 0xff);
		for (auto const& alias : permission.aliases) {
			FillString(p, alias);
		}

		int rights = 0;
		rights |= permission.bDirCreate		? 0x0001:0;
		rights |= permission.bDirDelete		? 0x0002:0;
		rights |= permission.bDirList		? 0x0004:0;
		rights |= permission.bDirSubdirs	? 0x0008:0;
		rights |= permission.bFileAppend	? 0x0010:0;
		rights |= permission.bFileDelete	? 0x0020:0;
		rights |= permission.bFileRead		? 0x0040:0;
		rights |= permission.bFileWrite		? 0x0080:0;
		rights |= permission.bIsHome		? 0x0100:0;
		rights |= permission.bAutoCreate	? 0x0200:0;
		*p++ = (char)(rights >> 8);
		*p++ = (char)(rights & 0xff);
	}

	for (int i = 0; i < 2; ++i) {
		*p++ = (char)((nSpeedLimitType[i] & 3) + ((nBypassServerSpeedLimit[i] & 3) << 2));
		*p++ = (char)(nSpeedLimit[i] >> 8);
		*p++ = (char)(nSpeedLimit[i] & 0xff);

		*p++ = (char)(SpeedLimits[i].size() >> 8);
		*p++ = (char)(SpeedLimits[i].size() & 0xff);
		for (auto const& limit : SpeedLimits[i]){
			p = limit.FillBuffer(p);
			if (!p) {
				return 0;
			}
		}
	}

	FillString(p, comment);

	*p++ = (char)forceSsl;

	return p;
}

int t_group::GetRequiredBufferLen() const
{
	int len = 9;
	len += GetRequiredStringBufferLen(group);

	len += 4;
	for (auto const& disallowedIP : disallowedIPs) {
		len += GetRequiredStringBufferLen(disallowedIP);
	}
	for (auto const& allowedIP : allowedIPs) {
		len += GetRequiredStringBufferLen(allowedIP);
	}

	len += 2;
	for (auto const& permission : permissions) {
		len += 2;

		len += GetRequiredStringBufferLen(permission.dir);

		len += 2;
		for (auto const& alias : permission.aliases) {
			len += GetRequiredStringBufferLen(alias);
		}
	}

	// Speed limits.
	len += 6; // Basic limits.
	len += 4; // Number of rules.
	for (int i = 0; i < 2; ++i) {
		for (auto const& limit : SpeedLimits[i]) {
			len += limit.GetRequiredBufferLen();
		}
	}

	len += GetRequiredStringBufferLen(comment);

	len++; //forceSsl

	return len;
}

int t_group::GetCurrentSpeedLimit(sltype type) const
{
	switch (nSpeedLimitType[type])
	{
	case 0:
		if (pOwner)
			return pOwner->GetCurrentSpeedLimit(type);
		else
			return 0;
	case 1:
		return 0;
	case 2:
		return nSpeedLimit[type];
	case 3:
		if (!SpeedLimits[type].empty()) {
			SYSTEMTIME st;
			GetLocalTime(&st);
			for (auto const& limit : SpeedLimits[type]) {
				if (limit.IsItActive(st)) {
					return limit.m_Speed;
				}
			}
		}
		if (pOwner)
			return pOwner->GetCurrentSpeedLimit(type);
		else
			return 0;
	}
	return 0;
}

bool t_group::BypassServerSpeedLimit(sltype type) const
{
	if (nBypassServerSpeedLimit[type] == 1)
		return true;
	else if (!nBypassServerSpeedLimit[type])
		return false;
	else if (pOwner)
		return pOwner->BypassServerSpeedLimit(type);
	else
		return false;
}

bool t_group::IsEnabled() const
{
	switch (nEnabled)
	{
	default:
	case 0:
		return false;
	case 1:
		return true;
	case 2:
		if (!pOwner)
			return false;

		return pOwner->IsEnabled();
	}
}

bool t_group::AccessAllowed(CStdString const& ip) const
{
	bool disallowed = false;

	for (auto const& disallowedIP : disallowedIPs) {
		if (disallowed = MatchesFilter(disallowedIP, ip)) {
			break;
		}
	}

	if (!disallowed)
	{
		if (!pOwner)
			return true;

		if (pOwner->AccessAllowed(ip))
			return true;
	}

	for (auto const& allowedIP : allowedIPs) {
		if (MatchesFilter(allowedIP, ip)) {
			return true;
		}
	}

	if (pOwner && !disallowed)
		return pOwner->AccessAllowed(ip);

	return false;
}

unsigned char * t_user::ParseBuffer(unsigned char *pBuffer, int length)
{
	unsigned char *p = pBuffer;
	unsigned char *endMarker = pBuffer + length;

	p = t_group::ParseBuffer(p, length);
	if (!p)
		return NULL;

	if (!ParseString(endMarker, p, user))
		return 0;

	if (!ParseString(endMarker, p, password))
		return 0;

	if (!ParseString(endMarker, p, salt))
		return 0;

	return p;
}

unsigned char * t_user::FillBuffer(unsigned char *p) const
{
	p = t_group::FillBuffer(p);
	if (!p) {
		return NULL;
	}

	FillString(p, user);
	FillString(p, password);
	FillString(p, salt);

	return p;
}

int t_user::GetRequiredBufferLen() const
{
	int len = t_group::GetRequiredBufferLen();
	len += GetRequiredStringBufferLen(user);
	len += GetRequiredStringBufferLen(password);
	len += GetRequiredStringBufferLen(salt);
	return len;
}

void t_user::generateSalt()
{
	char const validChars[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
	
	std::random_device rd;
	std::uniform_int_distribution<int> dist(0, sizeof(validChars) - 2);
	
	salt = _T("");

	for (size_t i = 0; i < 64; ++i) {
		salt += validChars[dist(rd)];
	}
}

bool t_group::ParseString(const unsigned char* endMarker, unsigned char *&p, CStdString &string)
{
	if ((endMarker - p) < 2)
		return false;

	int len = *p * 256 + p[1];
	p += 2;

	if ((endMarker - p) < len)
		return false;
	char* tmp = new char[len + 1];
	tmp[len] = 0;
	memcpy(tmp, p, len);
	CStdStringW str = ConvFromNetwork((const char*)tmp);
	delete [] tmp;
	p += len;
	string = str;

	return true;
}

bool t_group::ForceSsl() const
{
	switch (forceSsl)
	{
	default:
	case 0:
		return false;
	case 1:
		return true;
	case 2:
		if (!pOwner)
			return false;

		return pOwner->ForceSsl();
	}
}
