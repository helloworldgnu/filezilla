#include "stdafx.h"
#include "conversion.h"

CStdStringW ConvFromNetwork(const char* buffer)
{
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer, -1, 0, 0);
	if (len)
	{
		CStdStringW str;
		wchar_t* out = str.GetBuffer(len + 2);
		len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer, -1, out, len + 1);
		str.ReleaseBuffer();
		if (!len)
			str = L"";
		return str;
	}
	len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, buffer, -1, 0, 0);
	if (len)
	{
		CStdStringW str;
		wchar_t* out = str.GetBuffer(len + 2);
		len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, buffer, -1, out, len + 1);
		str.ReleaseBuffer();
		if (!len)
			str = L"";
		return str;
	}

	return L"";
}

std::string ConvToNetwork(const CStdStringW& str)
{
	std::string ret;
	int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	if (len > 0) {
		char* output = new char[len + 2];
		if (WideCharToMultiByte(CP_UTF8, 0, str, -1, output, len + 1, 0, 0) > 0)
			ret = output;
		delete [] output;
	}

	return ret;
}

CStdStringA ConvToLocal(const CStdStringW& str)
{
	int len = WideCharToMultiByte(CP_ACP, 0, str, -1, 0, 0, 0, 0);
	if (!len)
		return "";

	CStdStringA outStr;
	char* output = outStr.GetBuffer(len + 2);
	if (!WideCharToMultiByte(CP_ACP, 0, str, -1, output, len + 1, 0, 0))
	{
		output[0] = 0;
		outStr.ReleaseBuffer();
		return "";
	}
	outStr.ReleaseBuffer();

	return outStr;
}

CStdStringA ConvToLocal(const CStdStringA& str)
{
	return str;
}

CStdStringW ConvFromLocal(const CStdStringA& local)
{
	int len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, local, -1, 0, 0);
	if (len)
	{
		CStdStringW str;
		wchar_t* out = str.GetBuffer(len + 2);
		len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, local, -1, out, len + 1);
		str.ReleaseBuffer();
		if (!len)
			str = L"";
		return str;
	}

	return L"";
}
