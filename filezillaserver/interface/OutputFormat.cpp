#include "stdafx.h"

#include "OutputFormat.h"


// This function adds delimiters by thousands base.
// Delimiter based on user locale settings.
CString makeUserFriendlyString(__int64 val)
{
	int delimLen = ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, NULL, 0);

	CString delimStr;
	delimStr.Preallocate(delimLen);

	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, delimStr.GetBuffer(), delimLen);
	delimStr.ReleaseBuffer();

	CString str;
	str.Format(_T("%I64d"), val);

	CString result_str;
	int dec_count = 2 - ((str.GetLength() + 2) % 3);
	for (int idx = 0; idx < str.GetLength(); idx++)
	{
		if (dec_count > 2)
		{
			dec_count = 0;
			result_str += delimStr;
		}
		result_str += str[idx];
		dec_count++;
	}
	return result_str;
}



