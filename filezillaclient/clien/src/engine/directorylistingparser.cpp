#include <filezilla.h>
#include "directorylistingparser.h"
#include "ControlSocket.h"

#include <algorithm>
#include <vector>

std::map<wxString, int> CDirectoryListingParser::m_MonthNamesMap;

//#define LISTDEBUG_MVS
//#define LISTDEBUG
#ifdef LISTDEBUG
static char data[][150]={
	"" // Has to be terminated with empty string
};

#endif

namespace {
struct ObjectCache
{
	CRefcountObject<wxString> const& get(wxString const& v)
	{
		auto it = std::lower_bound( cache.begin(), cache.end(), v );

		if( it == cache.end() || !(*it == v) ) {
			it = cache.emplace(it, v);
		}
		return *it;
	}

	// Vector coupled with binary search and sorted insertion is fastest
	// alternative as we expect a relatively low amount of inserts.
	// Note that we cannot use set, as it it cannot search based on a different type.
	std::vector<CRefcountObject<wxString>> cache;
};


ObjectCache objcache;
}

class CToken
{
protected:
	enum TokenInformation
	{
		Unknown,
		Yes,
		No
	};

public:
	CToken()
		: m_pToken()
		, m_len()
		, m_numeric(Unknown)
		, m_leftNumeric(Unknown)
		, m_rightNumeric(Unknown)
		, m_number(-1)
	{
	}

	enum t_numberBase
	{
		decimal,
		hex
	};

	CToken(const wxChar* p, unsigned int len)
		: m_pToken(p)
		, m_len(len)
		, m_numeric(Unknown)
		, m_leftNumeric(Unknown)
		, m_rightNumeric(Unknown)
		, m_number(-1)
	{}

	const wxChar* GetToken() const
	{
		return m_pToken;
	}

	unsigned int GetLength() const
	{
		return m_len;
	}

	wxString GetString()
	{
		if (!m_pToken)
			return wxString();

		if( m_str.empty() ) {
			m_str.assign(m_pToken, m_len);
		}

		return m_str;
	}

	bool IsNumeric(t_numberBase base = decimal)
	{
		switch (base)
		{
		case decimal:
		default:
			if (m_numeric == Unknown)
			{
				m_numeric = Yes;
				for (unsigned int i = 0; i < m_len; ++i)
					if (m_pToken[i] < '0' || m_pToken[i] > '9')
					{
						m_numeric = No;
						break;
					}
			}
			return m_numeric == Yes;
		case hex:
			for (unsigned int i = 0; i < m_len; ++i)
			{
				const char c = m_pToken[i];
				if ((c < '0' || c > '9') && (c < 'A' || c > 'F') && (c < 'a' || c > 'f'))
					return false;
			}
			return true;
		}
	}

	bool IsNumeric(unsigned int start, unsigned int len)
	{
		for (unsigned int i = start; i < wxMin(start + len, m_len); ++i)
			if (m_pToken[i] < '0' || m_pToken[i] > '9')
				return false;
		return true;
	}

	bool IsLeftNumeric()
	{
		if (m_leftNumeric == Unknown)
		{
			if (m_len < 2)
				m_leftNumeric = No;
			else if (m_pToken[0] < '0' || m_pToken[0] > '9')
				m_leftNumeric = No;
			else
				m_leftNumeric = Yes;
		}
		return m_leftNumeric == Yes;
	}

	bool IsRightNumeric()
	{
		if (m_rightNumeric == Unknown)
		{
			if (m_len < 2)
				m_rightNumeric = No;
			else if (m_pToken[m_len - 1] < '0' || m_pToken[m_len - 1] > '9')
				m_rightNumeric = No;
			else
				m_rightNumeric = Yes;
		}
		return m_rightNumeric == Yes;
	}

	int Find(const wxChar* chr, int start = 0) const
	{
		if (!chr)
			return -1;

		for (unsigned int i = start; i < m_len; ++i)
		{
			for (int c = 0; chr[c]; ++c)
			{
				if (m_pToken[i] == chr[c])
					return i;
			}
		}
		return -1;
	}

	int Find(wxChar chr, int start = 0) const
	{
		if (!m_pToken)
			return -1;

		for (unsigned int i = start; i < m_len; ++i)
			if (m_pToken[i] == chr)
				return i;

		return -1;
	}

	wxLongLong GetNumber(unsigned int start, int len)
	{
		if (len == -1)
			len = m_len - start;
		if (len < 1)
			return -1;

		if (start + static_cast<unsigned int>(len) > m_len)
			return -1;

		if (m_pToken[start] < '0' || m_pToken[start] > '9')
			return -1;

		wxLongLong number = 0;
		for (unsigned int i = start; i < (start + len); ++i)
		{
			if (m_pToken[i] < '0' || m_pToken[i] > '9')
				break;
			number *= 10;
			number += m_pToken[i] - '0';
		}
		return number;
	}

	wxLongLong GetNumber(t_numberBase base = decimal)
	{
		switch (base)
		{
		default:
		case decimal:
			if (m_number == -1)
			{
				if (IsNumeric() || IsLeftNumeric())
				{
					m_number = 0;
					for (unsigned int i = 0; i < m_len; ++i)
					{
						if (m_pToken[i] < '0' || m_pToken[i] > '9')
							break;
						m_number *= 10;
						m_number += m_pToken[i] - '0';
					}
				}
				else if (IsRightNumeric())
				{
					m_number = 0;
					int start = m_len - 1;
					while (m_pToken[start - 1] >= '0' && m_pToken[start - 1] <= '9')
						--start;
					for (unsigned int i = start; i < m_len; ++i)
					{
						m_number *= 10;
						m_number += m_pToken[i] - '0';
					}
				}
			}
			return m_number;
		case hex:
			{
				wxLongLong number = 0;
				for (unsigned int i = 0; i < m_len; ++i)
				{
					const wxChar& c = m_pToken[i];
					if (c >= '0' && c <= '9')
					{
						number *= 16;
						number += c - '0';
					}
					else if (c >= 'a' && c <= 'f')
					{
						number *= 16;
						number += c - '0' + 10;
					}
					else if (c >= 'A' && c <= 'F')
					{
						number *= 16;
						number += c - 'A' + 10;
					}
					else
						return -1;
				}
				return number;
			}
		}
	}

	wxChar operator[](unsigned int n) const
	{
		if (n >= m_len)
			return 0;

		return m_pToken[n];
	}

protected:
	const wxChar* m_pToken;
	unsigned int m_len;

	TokenInformation m_numeric;
	TokenInformation m_leftNumeric;
	TokenInformation m_rightNumeric;
	wxLongLong m_number;
	wxString m_str;
};

class CLine
{
public:
	CLine(wxChar* p, int len = -1, int trailing_whitespace = 0)
	{
		m_pLine = p;
		if (len != -1)
			m_len = len;
		else
			m_len = wxStrlen(p);

		m_parsePos = 0;

		m_Tokens.reserve(10);
		m_LineEndTokens.reserve(10);
		m_trailing_whitespace = trailing_whitespace;
	}

	~CLine()
	{
		delete [] m_pLine;

		std::vector<CToken *>::iterator iter;
		for (iter = m_Tokens.begin(); iter != m_Tokens.end(); ++iter)
			delete *iter;
		for (iter = m_LineEndTokens.begin(); iter != m_LineEndTokens.end(); ++iter)
			delete *iter;
	}

	bool GetToken(unsigned int n, CToken &token, bool toEnd = false, bool include_whitespace = false)
	{
		n += offset_;
		if (!toEnd) {
			if (m_Tokens.size() > n) {
				token = *(m_Tokens[n]);
				return true;
			}

			int start = m_parsePos;
			while (m_parsePos < m_len) {
				if (m_pLine[m_parsePos] == ' ' || m_pLine[m_parsePos] == '\t') {
					CToken *pToken = new CToken(m_pLine + start, m_parsePos - start);
					m_Tokens.push_back(pToken);

					while (m_parsePos < m_len && (m_pLine[m_parsePos] == ' ' || m_pLine[m_parsePos] == '\t'))
						++m_parsePos;

					if (m_Tokens.size() > n) {
						token = *(m_Tokens[n]);
						return true;
					}

					start = m_parsePos;
				}
				++m_parsePos;
			}
			if (m_parsePos != start) {
				CToken *pToken = new CToken(m_pLine + start, m_parsePos - start);
					m_Tokens.push_back(pToken);
			}

			if (m_Tokens.size() > n) {
				token = *(m_Tokens[n]);
				return true;
			}

			return false;
		}
		else {
			if (include_whitespace) {
				const wxChar* p;

				int prev = n - offset_;
				if (prev)
					--prev;

				CToken ref;
				if (!GetToken(prev, ref))
					return false;
				p = ref.GetToken() + ref.GetLength() + 1;

				token = CToken(p, m_len - (p - m_pLine));
				return true;
			}

			if (m_LineEndTokens.size() > n) {
				token = *(m_LineEndTokens[n]);
				return true;
			}

			if (m_Tokens.size() <= n)
				if (!GetToken(n - offset_, token))
					return false;

			for (unsigned int i = static_cast<unsigned int>(m_LineEndTokens.size()); i <= n; ++i) {
				const CToken *refToken = m_Tokens[i];
				const wxChar* p = refToken->GetToken();
				CToken *pToken = new CToken(p, m_len - (p - m_pLine) - m_trailing_whitespace);
				m_LineEndTokens.push_back(pToken);
			}
			token = *(m_LineEndTokens[n]);
			return true;
		}
	};

	CLine *Concat(const CLine *pLine) const
	{
		int newLen = m_len + pLine->m_len + 1;
		wxChar* p = new wxChar[newLen];
		memcpy(p, m_pLine, m_len * sizeof(wxChar));
		p[m_len] = ' ';
		memcpy(p + m_len + 1, pLine->m_pLine, pLine->m_len * sizeof(wxChar));

		return new CLine(p, m_len + pLine->m_len + 1, pLine->m_trailing_whitespace);
	}

	void SetTokenOffset(unsigned int offset)
	{
		offset_ = offset;
	}

protected:
	std::vector<CToken *> m_Tokens;
	std::vector<CToken *> m_LineEndTokens;
	int m_parsePos;
	int m_len;
	int m_trailing_whitespace;
	wxChar* m_pLine;
	unsigned int offset_{};
};

CDirectoryListingParser::CDirectoryListingParser(CControlSocket* pControlSocket, const CServer& server, listingEncoding::type encoding, bool sftp_mode)
	: m_pControlSocket(pControlSocket)
	, m_currentOffset(0)
	, m_totalData()
	, m_prevLine(0)
	, m_server(server)
	, m_fileListOnly(true)
	, m_maybeMultilineVms(false)
	, m_listingEncoding(encoding)
	, sftp_mode_(sftp_mode)
	, today_(wxDateTime::Today())
{
	if (m_MonthNamesMap.empty()) {
		//Fill the month names map

		//English month names
		m_MonthNamesMap[_T("jan")] = 1;
		m_MonthNamesMap[_T("feb")] = 2;
		m_MonthNamesMap[_T("mar")] = 3;
		m_MonthNamesMap[_T("apr")] = 4;
		m_MonthNamesMap[_T("may")] = 5;
		m_MonthNamesMap[_T("jun")] = 6;
		m_MonthNamesMap[_T("june")] = 6;
		m_MonthNamesMap[_T("jul")] = 7;
		m_MonthNamesMap[_T("july")] = 7;
		m_MonthNamesMap[_T("aug")] = 8;
		m_MonthNamesMap[_T("sep")] = 9;
		m_MonthNamesMap[_T("sept")] = 9;
		m_MonthNamesMap[_T("oct")] = 10;
		m_MonthNamesMap[_T("nov")] = 11;
		m_MonthNamesMap[_T("dec")] = 12;

		//Numerical values for the month
		m_MonthNamesMap[_T("1")] = 1;
		m_MonthNamesMap[_T("01")] = 1;
		m_MonthNamesMap[_T("2")] = 2;
		m_MonthNamesMap[_T("02")] = 2;
		m_MonthNamesMap[_T("3")] = 3;
		m_MonthNamesMap[_T("03")] = 3;
		m_MonthNamesMap[_T("4")] = 4;
		m_MonthNamesMap[_T("04")] = 4;
		m_MonthNamesMap[_T("5")] = 5;
		m_MonthNamesMap[_T("05")] = 5;
		m_MonthNamesMap[_T("6")] = 6;
		m_MonthNamesMap[_T("06")] = 6;
		m_MonthNamesMap[_T("7")] = 7;
		m_MonthNamesMap[_T("07")] = 7;
		m_MonthNamesMap[_T("8")] = 8;
		m_MonthNamesMap[_T("08")] = 8;
		m_MonthNamesMap[_T("9")] = 9;
		m_MonthNamesMap[_T("09")] = 9;
		m_MonthNamesMap[_T("10")] = 10;
		m_MonthNamesMap[_T("11")] = 11;
		m_MonthNamesMap[_T("12")] = 12;

		//German month names
		m_MonthNamesMap[_T("mrz")] = 3;
		m_MonthNamesMap[_T("m\xe4r")] = 3;
		m_MonthNamesMap[_T("m\xe4rz")] = 3;
		m_MonthNamesMap[_T("mai")] = 5;
		m_MonthNamesMap[_T("juni")] = 6;
		m_MonthNamesMap[_T("juli")] = 7;
		m_MonthNamesMap[_T("okt")] = 10;
		m_MonthNamesMap[_T("dez")] = 12;

		//Austrian month names
		m_MonthNamesMap[_T("j\xe4n")] = 1;

		//French month names
		m_MonthNamesMap[_T("janv")] = 1;
		m_MonthNamesMap[_T("f\xe9") _T("b")] = 1;
		m_MonthNamesMap[_T("f\xe9v")] = 2;
		m_MonthNamesMap[_T("fev")] = 2;
		m_MonthNamesMap[_T("f\xe9vr")] = 2;
		m_MonthNamesMap[_T("fevr")] = 2;
		m_MonthNamesMap[_T("mars")] = 3;
		m_MonthNamesMap[_T("mrs")] = 3;
		m_MonthNamesMap[_T("avr")] = 4;
		m_MonthNamesMap[_T("avril")] = 4;
		m_MonthNamesMap[_T("juin")] = 6;
		m_MonthNamesMap[_T("juil")] = 7;
		m_MonthNamesMap[_T("jui")] = 7;
		m_MonthNamesMap[_T("ao\xfb")] = 8;
		m_MonthNamesMap[_T("ao\xfbt")] = 8;
		m_MonthNamesMap[_T("aout")] = 8;
		m_MonthNamesMap[_T("d\xe9") _T("c")] = 12;
		m_MonthNamesMap[_T("dec")] = 12;

		//Italian month names
		m_MonthNamesMap[_T("gen")] = 1;
		m_MonthNamesMap[_T("mag")] = 5;
		m_MonthNamesMap[_T("giu")] = 6;
		m_MonthNamesMap[_T("lug")] = 7;
		m_MonthNamesMap[_T("ago")] = 8;
		m_MonthNamesMap[_T("set")] = 9;
		m_MonthNamesMap[_T("ott")] = 10;
		m_MonthNamesMap[_T("dic")] = 12;

		//Spanish month names
		m_MonthNamesMap[_T("ene")] = 1;
		m_MonthNamesMap[_T("fbro")] = 2;
		m_MonthNamesMap[_T("mzo")] = 3;
		m_MonthNamesMap[_T("ab")] = 4;
		m_MonthNamesMap[_T("abr")] = 4;
		m_MonthNamesMap[_T("agto")] = 8;
		m_MonthNamesMap[_T("sbre")] = 9;
		m_MonthNamesMap[_T("obre")] = 9;
		m_MonthNamesMap[_T("nbre")] = 9;
		m_MonthNamesMap[_T("dbre")] = 9;

		//Polish month names
		m_MonthNamesMap[_T("sty")] = 1;
		m_MonthNamesMap[_T("lut")] = 2;
		m_MonthNamesMap[_T("kwi")] = 4;
		m_MonthNamesMap[_T("maj")] = 5;
		m_MonthNamesMap[_T("cze")] = 6;
		m_MonthNamesMap[_T("lip")] = 7;
		m_MonthNamesMap[_T("sie")] = 8;
		m_MonthNamesMap[_T("wrz")] = 9;
		m_MonthNamesMap[_T("pa\x9f")] = 10;
		m_MonthNamesMap[_T("pa\xbc")] = 10; // ISO-8859-2
		m_MonthNamesMap[_T("paz")] = 10; // ASCII
		m_MonthNamesMap[_T("pa\xc5\xba")] = 10; // UTF-8
		m_MonthNamesMap[_T("pa\x017a")] = 10; // some servers send this
		m_MonthNamesMap[_T("lis")] = 11;
		m_MonthNamesMap[_T("gru")] = 12;

		//Russian month names
		m_MonthNamesMap[_T("\xff\xed\xe2")] = 1;
		m_MonthNamesMap[_T("\xf4\xe5\xe2")] = 2;
		m_MonthNamesMap[_T("\xec\xe0\xf0")] = 3;
		m_MonthNamesMap[_T("\xe0\xef\xf0")] = 4;
		m_MonthNamesMap[_T("\xec\xe0\xe9")] = 5;
		m_MonthNamesMap[_T("\xe8\xfe\xed")] = 6;
		m_MonthNamesMap[_T("\xe8\xfe\xeb")] = 7;
		m_MonthNamesMap[_T("\xe0\xe2\xe3")] = 8;
		m_MonthNamesMap[_T("\xf1\xe5\xed")] = 9;
		m_MonthNamesMap[_T("\xee\xea\xf2")] = 10;
		m_MonthNamesMap[_T("\xed\xee\xff")] = 11;
		m_MonthNamesMap[_T("\xe4\xe5\xea")] = 12;

		//Dutch month names
		m_MonthNamesMap[_T("mrt")] = 3;
		m_MonthNamesMap[_T("mei")] = 5;

		//Portuguese month names
		m_MonthNamesMap[_T("out")] = 10;

		//Finnish month names
		m_MonthNamesMap[_T("tammi")] = 1;
		m_MonthNamesMap[_T("helmi")] = 2;
		m_MonthNamesMap[_T("maalis")] = 3;
		m_MonthNamesMap[_T("huhti")] = 4;
		m_MonthNamesMap[_T("touko")] = 5;
		m_MonthNamesMap[_T("kes\xe4")] = 6;
		m_MonthNamesMap[_T("hein\xe4")] = 7;
		m_MonthNamesMap[_T("elo")] = 8;
		m_MonthNamesMap[_T("syys")] = 9;
		m_MonthNamesMap[_T("loka")] = 10;
		m_MonthNamesMap[_T("marras")] = 11;
		m_MonthNamesMap[_T("joulu")] = 12;

		//Slovenian month names
		m_MonthNamesMap[_T("avg")] = 8;

		//Icelandic
		m_MonthNamesMap[_T("ma\x00ed")] = 5;
		m_MonthNamesMap[_T("j\x00fan")] = 6;
		m_MonthNamesMap[_T("j\x00fal")] = 7;
		m_MonthNamesMap[_T("\x00e1g")] = 8;
		m_MonthNamesMap[_T("n\x00f3v")] = 11;
		m_MonthNamesMap[_T("des")] = 12;

		//Lithuanian
		m_MonthNamesMap[_T("sau")] = 1;
		m_MonthNamesMap[_T("vas")] = 2;
		m_MonthNamesMap[_T("kov")] = 3;
		m_MonthNamesMap[_T("bal")] = 4;
		m_MonthNamesMap[_T("geg")] = 5;
		m_MonthNamesMap[_T("bir")] = 6;
		m_MonthNamesMap[_T("lie")] = 7;
		m_MonthNamesMap[_T("rgp")] = 8;
		m_MonthNamesMap[_T("rgs")] = 9;
		m_MonthNamesMap[_T("spa")] = 10;
		m_MonthNamesMap[_T("lap")] = 11;
		m_MonthNamesMap[_T("grd")] = 12;

		// Hungarian
		m_MonthNamesMap[_T("szept")] = 9;

		//There are more languages and thus month
		//names, but as long as nobody reports a
		//problem, I won't add them, there are way
		//too many languages

		// Some servers send a combination of month name and number,
		// Add corresponding numbers to the month names.
		std::map<wxString, int> combo;
		for (auto iter = m_MonthNamesMap.begin(); iter != m_MonthNamesMap.end(); ++iter)
		{
			// January could be 1 or 0, depends how the server counts
			combo[wxString::Format(_T("%s%02d"), iter->first, iter->second)] = iter->second;
			combo[wxString::Format(_T("%s%02d"), iter->first, iter->second - 1)] = iter->second;
			if (iter->second < 10)
				combo[wxString::Format(_T("%s%d"), iter->first, iter->second)] = iter->second;
			else
				combo[wxString::Format(_T("%s%d"), iter->first, iter->second % 10)] = iter->second;
			if (iter->second <= 10)
				combo[wxString::Format(_T("%s%d"), iter->first, iter->second - 1)] = iter->second;
			else
				combo[wxString::Format(_T("%s%d"), iter->first, (iter->second - 1) % 10)] = iter->second;
		}
		m_MonthNamesMap.insert(combo.begin(), combo.end());

		m_MonthNamesMap[_T("1")] = 1;
		m_MonthNamesMap[_T("2")] = 2;
		m_MonthNamesMap[_T("3")] = 3;
		m_MonthNamesMap[_T("4")] = 4;
		m_MonthNamesMap[_T("5")] = 5;
		m_MonthNamesMap[_T("6")] = 6;
		m_MonthNamesMap[_T("7")] = 7;
		m_MonthNamesMap[_T("8")] = 8;
		m_MonthNamesMap[_T("9")] = 9;
		m_MonthNamesMap[_T("10")] = 10;
		m_MonthNamesMap[_T("11")] = 11;
		m_MonthNamesMap[_T("12")] = 12;
	}

#ifdef LISTDEBUG
	for (unsigned int i = 0; data[i][0]; ++i)
	{
		unsigned int len = (unsigned int)strlen(data[i]);
		char *pData = new char[len + 3];
		strcpy(pData, data[i]);
		strcat(pData, "\r\n");
		AddData(pData, len + 2);
	}
#endif
}

CDirectoryListingParser::~CDirectoryListingParser()
{
	for (auto iter = m_DataList.begin(); iter != m_DataList.end(); ++iter)
		delete [] iter->p;

	delete m_prevLine;
}

bool CDirectoryListingParser::ParseData(bool partial)
{
	DeduceEncoding();

	bool error = false;
	CLine *pLine = GetLine(partial, error);
	while (pLine) {
		bool res = ParseLine(*pLine, m_server.GetType(), false);
		if (!res) {
			if (m_prevLine) {
				CLine* pConcatenatedLine = m_prevLine->Concat(pLine);
				res = ParseLine(*pConcatenatedLine, m_server.GetType(), true);
				delete pConcatenatedLine;
				delete m_prevLine;

				if (res) {
					delete pLine;
					m_prevLine = 0;
				}
				else
					m_prevLine = pLine;
			}
			else if (!sftp_mode_) {
				m_prevLine = pLine;
			}
			else {
				delete pLine;
			}
		}
		else {
			delete m_prevLine;
			m_prevLine = 0;
			delete pLine;
		}
		pLine = GetLine(partial, error);
	};

	return !error;
}

CDirectoryListing CDirectoryListingParser::Parse(const CServerPath &path)
{
	CDirectoryListing listing;
	listing.path = path;
	listing.m_firstListTime = CMonotonicTime::Now();

	if (!ParseData(false)){
		listing.m_flags |= CDirectoryListing::listing_failed;
		return listing;
	}

	if (!m_fileList.empty()) {
		wxASSERT(m_entryList.empty());

		listing.SetCount(m_fileList.size());
		unsigned int i = 0;
		for (std::list<wxString>::const_iterator iter = m_fileList.begin(); iter != m_fileList.end(); ++iter, ++i)
		{
			CDirentry entry;
			entry.name = *iter;
			entry.flags = 0;
			entry.size = -1;
			listing[i] = entry;
		}
	}
	else {
		listing.Assign(m_entryList);
	}

	return listing;
}

bool CDirectoryListingParser::ParseLine(CLine &line, const enum ServerType serverType, bool concatenated)
{
	CRefcountObject<CDirentry> refEntry;
	CDirentry & entry = refEntry.Get();

	bool res;
	int ires;

	if (sftp_mode_) {
		line.SetTokenOffset(1);
	}

	if (serverType == ZVM) {
		res = ParseAsZVM(line, entry);
		if (res)
			goto done;
	}
	else if (serverType == HPNONSTOP) {
		res = ParseAsHPNonstop(line, entry);
		if (res)
			goto done;
	}

	ires = ParseAsMlsd(line, entry);
	if (ires == 1)
		goto done;
	else if (ires == 2)
		goto skip;
	res = ParseAsUnix(line, entry, true); // Common 'ls -l'
	if (res)
		goto done;
	res = ParseAsDos(line, entry);
	if (res)
		goto done;
	res = ParseAsEplf(line, entry);
	if (res)
		goto done;
	res = ParseAsVms(line, entry);
	if (res)
		goto done;
	res = ParseOther(line, entry);
	if (res)
		goto done;
	res = ParseAsIbm(line, entry);
	if (res)
		goto done;
	res = ParseAsWfFtp(line, entry);
	if (res)
		goto done;
	res = ParseAsIBM_MVS(line, entry);
	if (res)
		goto done;
	res = ParseAsIBM_MVS_PDS(line, entry);
	if (res)
		goto done;
	res = ParseAsOS9(line, entry);
	if (res)
		goto done;
#ifndef LISTDEBUG_MVS
	if (serverType == MVS)
#endif //LISTDEBUG_MVS
	{
		res = ParseAsIBM_MVS_Migrated(line, entry);
		if (res)
			goto done;
		res = ParseAsIBM_MVS_PDS2(line, entry);
		if (res)
			goto done;
		res = ParseAsIBM_MVS_Tape(line, entry);
		if (res)
			goto done;
	}
	res = ParseAsUnix(line, entry, false); // 'ls -l' but without the date/time
	if (res)
		goto done;

	// Some servers just send a list of filenames. If a line could not be parsed,
	// check if it's a filename. If that's the case, store it for later, else clear
	// list of stored files.
	// If parsing finishes and no entries could be parsed and none of the lines
	// contained a space, assume it's a raw filelisting.

	if (!concatenated) {
		CToken token;
		if (!line.GetToken(0, token, true) || token.Find(' ') != -1) {
			m_maybeMultilineVms = false;
			m_fileList.clear();
			m_fileListOnly = false;
		}
		else {
			m_maybeMultilineVms = token.Find(';') != -1;
			if (m_fileListOnly)
				m_fileList.emplace_back(token.GetString());
		}
	}
	else
		m_maybeMultilineVms = false;

	return false;
done:

	m_maybeMultilineVms = false;
	m_fileList.clear();
	m_fileListOnly = false;

	// Don't add . or ..
	if (entry.name == _T(".") || entry.name == _T(".."))
		return true;

	if (serverType == VMS && entry.is_dir()) {
		// Trim version information from directories
		int pos = entry.name.Find(';', true);
		if (pos > 0)
			entry.name = entry.name.Left(pos);
	}

	if (sftp_mode_) {
		line.SetTokenOffset(0);

		CToken t;
		if (line.GetToken(0, t)) {
			wxLongLong seconds = t.GetNumber();
			if (seconds > 0 && seconds.GetValue() <= 0xffffffffll) {
				CDateTime time(wxDateTime(static_cast<time_t>(seconds.GetValue())), CDateTime::seconds);
				if (time.IsValid()) {
					entry.time = time;
				}
			}
		}
	}

	{
		auto const timezoneOffset = m_server.GetTimezoneOffset();
		if (timezoneOffset) {
			entry.time += wxTimeSpan(0, timezoneOffset, 0, 0);
		}
	}

	m_entryList.emplace_back(std::move(refEntry));

skip:
	m_maybeMultilineVms = false;
	m_fileList.clear();
	m_fileListOnly = false;

	return true;
}

bool CDirectoryListingParser::ParseAsUnix(CLine &line, CDirentry &entry, bool expect_date)
{
	int index = 0;
	CToken token;
	if (!line.GetToken(index, token))
		return false;

	wxChar chr = token[0];
	if (chr != 'b' &&
		chr != 'c' &&
		chr != 'd' &&
		chr != 'l' &&
		chr != 'p' &&
		chr != 's' &&
		chr != '-')
		return false;

	wxString permissions = token.GetString();

	entry.flags = 0;

	if (chr == 'd' || chr == 'l')
		entry.flags |= CDirentry::flag_dir;

	if (chr == 'l')
		entry.flags |= CDirentry::flag_link;

	// Check for netware servers, which split the permissions into two parts
	bool netware = false;
	if (token.GetLength() == 1)
	{
		if (!line.GetToken(++index, token))
			return false;
		permissions += _T(" ") + token.GetString();
		netware = true;
	}
	entry.permissions = objcache.get(permissions);

	int numOwnerGroup = 3;
	if (!netware)
	{
		// Filter out link count, we don't need it
		if (!line.GetToken(++index, token))
			return false;

		if (!token.IsNumeric())
			--index;
	}

	// Repeat until numOwnerGroup is 0 since not all servers send every possible field
	int startindex = index;
	do
	{
		// Reset index
		index = startindex;

		wxString ownerGroup;
		for (int i = 0; i < numOwnerGroup; ++i)
		{
			if (!line.GetToken(++index, token))
				return false;
			if (i)
				ownerGroup += _T(" ");
			ownerGroup += token.GetString();
		}

		if (!line.GetToken(++index, token))
			return false;

		// Check for concatenated groupname and size fields
		if (!ParseComplexFileSize(token, entry.size))
		{
			if (!token.IsRightNumeric())
				continue;
			entry.size = token.GetNumber();
		}

		// Append missing group to ownerGroup
		if (!token.IsNumeric() && token.IsRightNumeric())
		{
			if (!ownerGroup.empty())
				ownerGroup += _T(" ");

			wxString const group = token.GetString();
			int i;
			for( i = group.size() - 1;
				 i >= 0 && group[i] >= '0' && group[i] <= '9';
				 --i ) {}

			ownerGroup += group.Left(i + 1);
		}

		if (expect_date) {
			entry.time = CDateTime();
			if (!ParseUnixDateTime(line, index, entry))
				continue;
		}

		// Get the filename
		if (!line.GetToken(++index, token, 1))
			continue;

		entry.name = token.GetString();

		// Filter out cpecial chars at the end of the filenames
		chr = token[token.GetLength() - 1];
		if (chr == '/' ||
			chr == '|' ||
			chr == '*')
			entry.name.RemoveLast();

		if (entry.is_link()) {
			int pos;
			if ((pos = entry.name.Find(_T(" -> "))) != -1) {
				entry.target = CSparseOptional<wxString>(entry.name.Mid(pos + 4));
				entry.name = entry.name.Left(pos);
			}
		}

		entry.time += m_timezoneOffset;

		entry.ownerGroup = objcache.get(ownerGroup);
		return true;
	}
	while (numOwnerGroup--);

	return false;
}

bool CDirectoryListingParser::ParseUnixDateTime(CLine & line, int &index, CDirentry &entry)
{
	bool mayHaveTime = true;
	bool bHasYearAndTime = false;

	CToken token;

	// Get the month date field
	CToken dateMonth;
	if (!line.GetToken(++index, token))
		return false;

	int year = -1;
	int month = -1;
	int day = -1;
	long hour = -1;
	long minute = -1;

	// Some servers use the following date formats:
	// 26-05 2002, 2002-10-14, 01-jun-99 or 2004.07.15
	// slashes instead of dashes are also possible
	int pos = token.Find(_T("-/."));
	if (pos != -1) {
		int pos2 = token.Find(_T("-/."), pos + 1);
		if (pos2 == -1) {
			if (token[pos] != '.') {
				// something like 26-05 2002
				day = token.GetNumber(pos + 1, token.GetLength() - pos - 1).GetLo();
				if (day < 1 || day > 31)
					return false;
				dateMonth = CToken(token.GetToken(), pos);
			}
			else
				dateMonth = token;
		}
		else if (token[pos] != token[pos2])
			return false;
		else {
			if (!ParseShortDate(token, entry))
				return false;

			if (token[pos] == '.')
				return true;

			wxDateTime::Tm t = entry.time.Degenerate().GetTm();
			year = t.year;
			month = t.mon + 1;
			day = t.mday;
		}
	}
	else if (token.IsNumeric()) {
		if (token.GetNumber() > 1000 && token.GetNumber() < 10000) {
			// Two possible variants:
			// 1) 2005 3 13
			// 2) 2005 13 3
			// assume first one.
			year = token.GetNumber().GetLo();
			if (!line.GetToken(++index, dateMonth))
				return false;
			mayHaveTime = false;
		}
		else
			dateMonth = token;
	}
	else {
		if (token.IsLeftNumeric() && (unsigned int)token[token.GetLength() - 1] > 127 &&
			token.GetNumber() > 1000)
		{
			if (token.GetNumber() > 10000)
				return false;

			// Asian date format: 2005xxx 5xx 20xxx with some non-ascii characters following
			year = token.GetNumber().GetLo();
			if (!line.GetToken(++index, dateMonth))
				return false;
			mayHaveTime = false;
		}
		else
			dateMonth = token;
	}

	if (day < 1) {
		// Get day field
		if (!line.GetToken(++index, token))
			return false;

		int dateDay;

		// Check for non-numeric day
		if (!token.IsNumeric() && !token.IsLeftNumeric()) {
			int offset = 0;
			if (dateMonth.GetString().Right(1) == _T("."))
				++offset;
			if (!dateMonth.IsNumeric(0, dateMonth.GetLength() - offset))
				return false;
			dateDay = dateMonth.GetNumber(0, dateMonth.GetLength() - offset).GetLo();
			dateMonth = token;
		}
		else if( token.GetLength() == 5 && token[2] == ':' && token.IsRightNumeric() ) {
			// This is a time. We consumed too much already.
			return false;
		}
		else {
			dateDay = token.GetNumber().GetLo();
			if (token[token.GetLength() - 1] == ',')
				bHasYearAndTime = true;
		}

		if (dateDay < 1 || dateDay > 31)
			return false;
		day = dateDay;
	}

	if (month < 1) {
		wxString strMonth = dateMonth.GetString();
		if (dateMonth.IsLeftNumeric() && (unsigned int)strMonth[strMonth.Length() - 1] > 127) {
			// Most likely an Asian server sending some unknown language specific
			// suffix at the end of the monthname. Filter it out.
			int i;
			for (i = strMonth.Length() - 1; i > 0; --i) {
				if (strMonth[i] >= '0' && strMonth[i] <= '9')
					break;
			}
			strMonth = strMonth.Left(i + 1);
		}
		// Check month name
		while (strMonth.Right(1) == _T(",") || strMonth.Right(1) == _T("."))
			strMonth.RemoveLast();
		if (!GetMonthFromName(strMonth, month))
			return false;
	}

	// Get time/year field
	if (!line.GetToken(++index, token))
		return false;

	pos = token.Find(_T(":.-"));
	if (pos != -1 && mayHaveTime) {
		// token is a time
		if (!pos || static_cast<size_t>(pos) == (token.GetLength() - 1))
			return false;

		wxString str = token.GetString();
		if (!str.Left(pos).ToLong(&hour))
			return false;
		if (!str.Mid(pos + 1).ToLong(&minute))
			return false;

		if (hour < 0 || hour > 23)
			return false;
		if (minute < 0 || minute > 59)
			return false;

		// Some servers use times only for files newer than 6 months
		if( year <= 0 ) {
			wxASSERT( month != -1 && day != -1 );
			year = wxDateTime::Now().GetYear();
			int currentDayOfYear = wxDateTime::Now().GetDay() + 31 * (wxDateTime::Now().GetMonth() - wxDateTime::Jan);
			int fileDayOfYear = (month - 1) * 31 + day;

			// We have to compare with an offset of one. In the worst case,
			// the server's timezone might be up to 24 hours ahead of the
			// client.
			// Problem: Servers which do send the time but not the year even
			// one day away from getting 1 year old. This is far more uncommon
			// however.
			if ((currentDayOfYear + 1) < fileDayOfYear)
				year -= 1;
		}
	}
	else if (year <= 0) {
		// token is a year
		if (!token.IsNumeric() && !token.IsLeftNumeric())
			return false;

		year = token.GetNumber().GetLo();

		if (year > 3000)
			return false;
		if (year < 1000)
			year += 1900;

		if (bHasYearAndTime) {
			if (!line.GetToken(++index, token))
				return false;

			if (token.Find(':') == 2 && token.GetLength() == 5 && token.IsLeftNumeric() && token.IsRightNumeric()) {
				pos = token.Find(':');
				// token is a time
				if (!pos || static_cast<size_t>(pos) == (token.GetLength() - 1))
					return false;

				wxString str = token.GetString();

				if (!str.Left(pos).ToLong(&hour))
					return false;
				if (!str.Mid(pos + 1).ToLong(&minute))
					return false;

				if (hour < 0 || hour > 23)
					return false;
				if (minute < 0 || minute > 59)
					return false;
			}
			else
				--index;
		}
	}
	else
		--index;

	if (!entry.time.Set(year, month, day, hour, minute)) {
		return false;
	}

	return true;
}

bool CDirectoryListingParser::ParseShortDate(CToken &token, CDirentry &entry, bool saneFieldOrder /*=false*/)
{
	if (token.GetLength() < 1)
		return false;

	bool gotYear = false;
	bool gotMonth = false;
	bool gotDay = false;
	bool gotMonthName = false;

	int year = 0;
	int month = 0;
	int day = 0;

	int pos = token.Find(_T("-./"));
	if (pos < 1)
		return false;

	if (!token.IsNumeric(0, pos)) {
		// Seems to be monthname-dd-yy

		// Check month name
		wxString dateMonth = token.GetString().Mid(0, pos);
		if (!GetMonthFromName(dateMonth, month))
			return false;
		gotMonth = true;
		gotMonthName = true;
	}
	else if (pos == 4) {
		// Seems to be yyyy-mm-dd
		year = token.GetNumber(0, pos).GetLo();
		if (year < 1900 || year > 3000)
			return false;
		gotYear = true;
	}
	else if (pos <= 2) {
		wxLongLong value = token.GetNumber(0, pos);
		if (token[pos] == '.') {
			// Maybe dd.mm.yyyy
			if (value < 1 || value > 31)
				return false;
			day = value.GetLo();
			gotDay = true;
		}
		else {
			if (saneFieldOrder) {
				year = value.GetLo();
				if (year < 50)
					year += 2000;
				else
					year += 1900;
				gotYear = true;
			}
			else {
				// Detect mm-dd-yyyy or mm/dd/yyyy and
				// dd-mm-yyyy or dd/mm/yyyy
				if (value < 1)
					return false;
				if (value > 12) {
					if (value > 31)
						return false;

					day = value.GetLo();
					gotDay = true;
				}
				else {
					month = value.GetLo();
					gotMonth = true;
				}
			}
		}
	}
	else
		return false;

	int pos2 = token.Find(_T("-./"), pos + 1);
	if (pos2 == -1 || (pos2 - pos) == 1)
		return false;
	if (static_cast<size_t>(pos2) == (token.GetLength() - 1))
		return false;

	// If we already got the month and the second field is not numeric,
	// change old month into day and use new token as month
	if (!token.IsNumeric(pos + 1, pos2 - pos - 1) && gotMonth) {
		if (gotMonthName)
			return false;

		if (gotDay)
			return false;

		gotDay = true;
		gotMonth = false;
		day = month;
	}

	if (gotYear || gotDay) {
		// Month field in yyyy-mm-dd or dd-mm-yyyy
		// Check month name
		wxString dateMonth = token.GetString().Mid(pos + 1, pos2 - pos - 1);
		if (!GetMonthFromName(dateMonth, month))
			return false;
		gotMonth = true;
	}
	else {
		wxLongLong value = token.GetNumber(pos + 1, pos2 - pos - 1);
		// Day field in mm-dd-yyyy
		if (value < 1 || value > 31)
			return false;
		day = value.GetLo();
		gotDay = true;
	}

	auto value = token.GetNumber(pos2 + 1, token.GetLength() - pos2 - 1).GetLo();
	if (gotYear) {
		// Day field in yyy-mm-dd
		if (!value || value > 31)
			return false;
		day = value;
		gotDay = true;
	}
	else {
		if (value < 0)
			return false;

		if (value < 50)
			value += 2000;
		else if (value < 1000)
			value += 1900;
		year = value;

		gotYear = true;
	}

	wxASSERT(gotYear);
	wxASSERT(gotMonth);
	wxASSERT(gotDay);

	if (!entry.time.Set( year, month, day )) {
		return false;
	}

	return true;
}

bool CDirectoryListingParser::ParseAsDos(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// Get first token, has to be a valid date
	if (!line.GetToken(index, token))
		return false;

	entry.flags = 0;

	if (!ParseShortDate(token, entry))
		return false;

	// Extract time
	if (!line.GetToken(++index, token))
		return false;

	if (!ParseTime(token, entry))
		return false;

	// If next token is <DIR>, entry is a directory
	// else, it should be the filesize.
	if (!line.GetToken(++index, token))
		return false;

	if (token.GetString() == _T("<DIR>"))
	{
		entry.flags |= CDirentry::flag_dir;
		entry.size = -1;
	}
	else if (token.IsNumeric() || token.IsLeftNumeric())
	{
		// Convert size, filter out separators
		wxLongLong size = 0;
		int len = token.GetLength();
		for (int i = 0; i < len; ++i)
		{
			char chr = token[i];
			if (chr == ',' || chr == '.')
				continue;
			if (chr < '0' || chr > '9')
				return false;

			size *= 10;
			size += chr - '0';
		}
		entry.size = size;
	}
	else
		return false;

	// Extract filename
	if (!line.GetToken(++index, token, true))
		return false;
	entry.name = token.GetString();

	entry.target.clear();
	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.time += m_timezoneOffset;

	return true;
}

bool CDirectoryListingParser::ParseTime(CToken &token, CDirentry &entry)
{
	if (!entry.has_date())
		return false;

	int pos = token.Find(':');
	if (pos < 1 || static_cast<unsigned int>(pos) >= (token.GetLength() - 1))
		return false;

	wxLongLong hour = token.GetNumber(0, pos);
	if (hour < 0 || hour > 23)
		return false;

	// See if we got seconds
	int pos2 = token.Find(':', pos + 1);
	int len;
	if (pos2 == -1)
		len = -1;
	else
		len = pos2 - pos - 1;

	if (!len)
		return false;

	wxLongLong minute = token.GetNumber(pos + 1, len);
	if (minute < 0 || minute > 59)
		return false;

	wxLongLong seconds = -1;
	if (pos2 != -1) {
		// Parse seconds
		seconds = token.GetNumber(pos2 + 1, -1);
		if (seconds < 0 || seconds > 60)
			return false;
	}

	// Convert to 24h format
	if (!token.IsRightNumeric())
	{
		if (token[token.GetLength() - 2] == 'P')
		{
			if (hour < 12)
				hour += 12;
		}
		else
			if (hour == 12)
				hour = 0;
	}

	return entry.time.ImbueTime( hour.GetLo(), minute.GetLo(), seconds.GetLo() );
}

bool CDirectoryListingParser::ParseAsEplf(CLine &line, CDirentry &entry)
{
	CToken token;
	if (!line.GetToken(0, token, true))
		return false;

	if (token[0] != '+')
		return false;

	int pos = token.Find('\t');
	if (pos == -1 || static_cast<size_t>(pos) == (token.GetLength() - 1))
		return false;

	entry.name = token.GetString().Mid(pos + 1);

	entry.flags = 0;
	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.size = -1;

	int fact = 1;
	while (fact < pos)
	{
		int separator = token.Find(',', fact);
		int len;
		if (separator == -1)
			len = pos - fact;
		else
			len = separator - fact;

		if (!len)
		{
			++fact;
			continue;
		}

		char type = token[fact];

		if (type == '/')
			entry.flags |= CDirentry::flag_dir;
		else if (type == 's')
			entry.size = token.GetNumber(fact + 1, len - 1);
		else if (type == 'm')
		{
			wxLongLong number = token.GetNumber(fact + 1, len - 1);
			if (number < 0)
				return false;
			entry.time = CDateTime(wxDateTime((time_t)number.GetValue()), CDateTime::seconds);
		}
		else if (type == 'u' && len > 2 && token[fact + 1] == 'p')
			entry.permissions = objcache.get(token.GetString().Mid(fact + 2, len - 2));

		fact += len + 1;
	}

	return true;
}

wxString Unescape(const wxString& str, wxChar escape)
{
	wxString res;
	for (unsigned int i = 0; i < str.Len(); ++i)
	{
		wxChar c = str[i];
		if (c == escape)
		{
			c = str[++i];
			if (!c)
				break;
		}
		res += c;
	}

	return res;
}

bool CDirectoryListingParser::ParseAsVms(CLine &line, CDirentry &entry)
{
	CToken token;
	int index = 0;

	if (!line.GetToken(index, token))
		return false;

	int pos = token.Find(';');
	if (pos == -1)
		return false;

	entry.flags = 0;

	if (pos > 4 && token.GetString().Mid(pos - 4, 4) == _T(".DIR")) {
		entry.flags |= CDirentry::flag_dir;
		if (token.GetString().Mid(pos) == _T(";1"))
			entry.name = token.GetString().Left(pos - 4);
		else
			entry.name = token.GetString().Left(pos - 4) + token.GetString().Mid(pos);
	}
	else
		entry.name = token.GetString();

	// Some VMS servers escape special characters like additional dots with ^
	entry.name = Unescape(entry.name, '^');

	if (!line.GetToken(++index, token))
		return false;

	wxString ownerGroup;
	wxString permissions;

	// This field can either be the filesize, a username (at least that's what I think) enclosed in [] or a date.
	if (!token.IsNumeric() && !token.IsLeftNumeric()) {
		// Must be username
		const int len = token.GetLength();
		if (len < 3 || token[0] != '[' || token[len - 1] != ']')
			return false;
		ownerGroup = token.GetString().Mid(1, len - 2);

		if (!line.GetToken(++index, token))
			return false;
		if (!token.IsNumeric() && !token.IsLeftNumeric())
			return false;
	}

	// Current token is either size or date
	bool gotSize = false;
	pos = token.Find('/');

	if (!pos)
		return false;

	if (token.IsNumeric() || (pos != -1 && token.Find('/', pos + 1) == -1)) {
		// Definitely size
		CToken sizeToken;
		if (pos == -1)
			sizeToken = token;
		else
			sizeToken = CToken(token.GetToken(), pos);
		if (!ParseComplexFileSize(sizeToken, entry.size, 512))
			return false;
		gotSize = true;

		if (!line.GetToken(++index, token))
			return false;
	}
	else if (pos == -1 && token.IsLeftNumeric()) {
		// Perhaps size
		if (ParseComplexFileSize(token, entry.size, 512)) {
			gotSize = true;

			if (!line.GetToken(++index, token))
				return false;
		}
	}

	// Get date
	if (!ParseShortDate(token, entry))
		return false;

	// Get time
	if (!line.GetToken(++index, token))
		return true;

	if (!ParseTime(token, entry)) {
		int len = token.GetLength();
		if (token[0] == '[' && token[len - 1] != ']')
			return false;
		if (token[0] == '(' && token[len - 1] != ')')
			return false;
		if (token[0] != '[' && token[len - 1] == ']')
			return false;
		if (token[0] != '(' && token[len - 1] == ')')
			return false;
		--index;
	}

	if (!gotSize) {
		// Get size
		if (!line.GetToken(++index, token))
			return false;

		if (!token.IsNumeric() && !token.IsLeftNumeric())
			return false;

		pos = token.Find('/');
		if (!pos)
			return false;

		CToken sizeToken;
		if (pos == -1)
			sizeToken = token;
		else
			sizeToken = CToken(token.GetToken(), pos);
		if (!ParseComplexFileSize(sizeToken, entry.size, 512))
			return false;
	}

	// Owner / group and permissions
	while (line.GetToken(++index, token)) {
		const int len = token.GetLength();
		if (len > 2 && token[0] == '(' && token[len - 1] == ')') {
			if (!permissions.empty())
				permissions += _T(" ");
			permissions += token.GetString().Mid(1, len - 2);
		}
		else if (len > 2 && token[0] == '[' && token[len - 1] == ']') {
			if (!ownerGroup.empty())
				ownerGroup += _T(" ");
			ownerGroup += token.GetString().Mid(1, len - 2);
		}
		else {
			if (!ownerGroup.empty())
				ownerGroup += _T(" ");
			ownerGroup += token.GetString();
		}
	}
	entry.permissions = objcache.get(permissions);
	entry.ownerGroup = objcache.get(ownerGroup);

	entry.time += m_timezoneOffset;

	return true;
}

bool CDirectoryListingParser::ParseAsIbm(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// Get owner
	if (!line.GetToken(index, token))
		return false;

	entry.ownerGroup = objcache.get(token.GetString());

	// Get size
	if (!line.GetToken(++index, token))
		return false;

	if (!token.IsNumeric())
		return false;

	entry.size = token.GetNumber();

	// Get date
	if (!line.GetToken(++index, token))
		return false;

	entry.flags = 0;

	if (!ParseShortDate(token, entry))
		return false;

	// Get time
	if (!line.GetToken(++index, token))
		return false;

	if (!ParseTime(token, entry))
		return false;

	// Get filename
	if (!line.GetToken(index + 2, token, 1))
		return false;

	entry.name = token.GetString();
	if (token[token.GetLength() - 1] == '/') {
		entry.name.RemoveLast();
		entry.flags |= CDirentry::flag_dir;
	}

	entry.time += m_timezoneOffset;

	return true;
}

bool CDirectoryListingParser::ParseOther(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken firstToken;

	if (!line.GetToken(index, firstToken))
		return false;

	if (!firstToken.IsNumeric())
		return false;

	// Possible formats: Numerical unix, VShell or OS/2

	CToken token;
	if (!line.GetToken(++index, token))
		return false;

	entry.flags = 0;

	// If token is a number, than it's the numerical Unix style format,
	// else it's the VShell, OS/2 or nortel.VxWorks format
	if (token.IsNumeric()) {
		entry.permissions = objcache.get(firstToken.GetString());
		if (firstToken.GetLength() >= 2 && firstToken[1] == '4')
			entry.flags |= CDirentry::flag_dir;

		wxString ownerGroup = token.GetString();

		if (!line.GetToken(++index, token))
			return false;

		ownerGroup += _T(" ") + token.GetString();
		entry.ownerGroup = objcache.get(ownerGroup);

		// Get size
		if (!line.GetToken(++index, token))
			return false;

		if (!token.IsNumeric())
			return false;

		entry.size = token.GetNumber();

		// Get date/time
		if (!line.GetToken(++index, token))
			return false;

		wxLongLong number = token.GetNumber();
		if (number < 0)
			return false;
		entry.time = CDateTime(wxDateTime((time_t)number.GetValue()), CDateTime::seconds);

		// Get filename
		if (!line.GetToken(++index, token, true))
			return false;

		entry.name = token.GetString();
		entry.target.clear();
	}
	else {
		// Possible conflict with multiline VMS listings
		if (m_maybeMultilineVms)
			return false;

		// VShell, OS/2 or nortel.VxWorks style format
		entry.size = firstToken.GetNumber();

		// Get date
		wxString dateMonth = token.GetString();
		int month = 0;
		if (!GetMonthFromName(dateMonth, month)) {
			// OS/2 or nortel.VxWorks
			int skippedCount = 0;
			do {
				if (token.GetString() == _T("DIR"))
					entry.flags |= CDirentry::flag_dir;
				else if (token.Find(_T("-/.")) != -1)
					break;

				++skippedCount;

				if (!line.GetToken(++index, token))
					return false;
			} while (true);

			if (!ParseShortDate(token, entry))
				return false;

			// Get time
			if (!line.GetToken(++index, token))
				return false;

			if (!ParseTime(token, entry))
				return false;

			// Get filename
			if (!line.GetToken(++index, token, true))
				return false;

			entry.name = token.GetString();
			wxString type = entry.name.Right(5);
			MakeLowerAscii(type);
			if (!skippedCount && type == _T("<dir>")) {
				entry.flags |= CDirentry::flag_dir;
				entry.name = entry.name.Left(entry.name.Length() - 5);
				while (!entry.name.empty() && entry.name.Last() == ' ')
					entry.name.RemoveLast();
			}
		}
		else {
			// Get day
			if (!line.GetToken(++index, token))
				return false;

			if (!token.IsNumeric() && !token.IsLeftNumeric())
				return false;

			wxLongLong day = token.GetNumber();
			if (day < 0 || day > 31)
				return false;

			// Get Year
			if (!line.GetToken(++index, token))
				return false;

			if (!token.IsNumeric())
				return false;

			wxLongLong year = token.GetNumber();
			if (year < 50)
				year += 2000;
			else if (year < 1000)
				year += 1900;

			if( !entry.time.Set( year.GetLo(), month, day.GetLo() ) ) {
				return false;
			}

			// Get time
			if (!line.GetToken(++index, token))
				return false;

			if (!ParseTime(token, entry))
				return false;

			// Get filename
			if (!line.GetToken(++index, token, 1))
				return false;

			entry.name = token.GetString();
			char chr = token[token.GetLength() - 1];
			if (chr == '/' || chr == '\\')
			{
				entry.flags |= CDirentry::flag_dir;
				entry.name.RemoveLast();
			}
		}
		entry.target.clear();
		entry.ownerGroup = objcache.get(wxString());
		entry.permissions = objcache.get(wxString());
		entry.time += m_timezoneOffset;
	}

	return true;
}

bool CDirectoryListingParser::AddData(char *pData, int len)
{
	ConvertEncoding( pData, len );

	m_DataList.emplace_back(pData, len);
	m_totalData += len;

	if (m_totalData < 512)
		return true;

	return ParseData(true);
}

bool CDirectoryListingParser::AddLine(wxChar const* pLine)
{
	if (m_pControlSocket)
		m_pControlSocket->LogMessageRaw(MessageType::RawList, pLine);

	while (*pLine == ' ' || *pLine == '\t')
		++pLine;

	if (!*pLine)
		return false;

	const int len = wxStrlen(pLine);

	wxChar* p = new wxChar[len + 1];

	wxStrcpy(p, pLine);

	CLine line(p, len);

	ParseLine(line, m_server.GetType(), false);

	return true;
}

CLine *CDirectoryListingParser::GetLine(bool breakAtEnd /*=false*/, bool &error)
{
	while (!m_DataList.empty()) {
		// Trim empty lines and spaces
		auto iter = m_DataList.begin();
		int len = iter->len;
		while (iter->p[m_currentOffset] == '\r' || iter->p[m_currentOffset] == '\n' || iter->p[m_currentOffset] == ' ' || iter->p[m_currentOffset] == '\t') {
			++m_currentOffset;
			if (m_currentOffset >= len) {
				delete [] iter->p;
				++iter;
				m_currentOffset = 0;
				if (iter == m_DataList.end()) {
					m_DataList.clear();
					return 0;
				}
				len = iter->len;
			}
		}
		m_DataList.erase(m_DataList.begin(), iter);
		iter = m_DataList.begin();

		// Remember start offset and find next linebreak
		int startpos = m_currentOffset;
		int reslen = 0;

		int emptylen = 0;

		int currentOffset = m_currentOffset;
		while ((iter->p[currentOffset] != '\n') && (iter->p[currentOffset] != '\r')) {
			if (iter->p[currentOffset] == ' ' || iter->p[currentOffset] == '\t')
				++emptylen;
			else
				emptylen = 0;
			++reslen;

			++currentOffset;
			if (currentOffset >= len) {
				++iter;
				if (iter == m_DataList.end()) {
					if (reslen > 10000) {
						m_pControlSocket->LogMessage(MessageType::Error, _("Received a line exceeding 10000 characters, aborting."));
						error = true;
						return 0;
					}
					if (breakAtEnd)
						return 0;
					break;
				}
				len = iter->len;
				currentOffset = 0;
			}
		}

		if (reslen > 10000) {
			m_pControlSocket->LogMessage(MessageType::Error, _("Received a line exceeding 10000 characters, aborting."));
			error = true;
			return 0;
		}
		m_currentOffset = currentOffset;

		// Reslen is now the length of the line, including any terminating whitespace
		int const buflen = reslen + 1;
		char *res = new char[buflen];
		res[reslen] = 0;

		int respos = 0;

		// Copy line data
		auto i = m_DataList.begin();
		while (i != iter && reslen) {
			int copylen = i->len - startpos;
			if (copylen > reslen)
				copylen = reslen;
			memcpy(&res[respos], &i->p[startpos], copylen);
			reslen -= copylen;
			respos += i->len - startpos;
			startpos = 0;

			delete [] i->p;
			++i;
		}

		// Copy last chunk
		if (iter != m_DataList.end() && reslen) {
			int copylen = m_currentOffset-startpos;
			if (copylen > reslen)
				copylen = reslen;
			memcpy(&res[respos], &iter->p[startpos], copylen);
			if (reslen >= iter->len) {
				delete [] iter->p;
				m_DataList.erase(m_DataList.begin(), ++iter);
			}
			else
				m_DataList.erase(m_DataList.begin(), iter);
		}
		else
			m_DataList.erase(m_DataList.begin(), iter);

		size_t lineLength{};
		wxChar* buffer;
		if (m_pControlSocket) {
			buffer = m_pControlSocket->ConvToLocalBuffer(res, buflen, lineLength);
			m_pControlSocket->LogMessageRaw(MessageType::RawList, buffer);
		}
		else {
			wxString str(res, wxConvUTF8);
			if (str.empty())
			{
				str = wxString(res, wxConvLocal);
				if (str.empty())
					str = wxString(res, wxConvISO8859_1);
			}
			lineLength = str.Len() + 1;
			buffer = new wxChar[str.Len() + 1];
			wxStrcpy(buffer, str.c_str());
		}
		delete [] res;

		if (!buffer) {
			// Line contained no usable data, start over
			continue;
		}

		return new CLine(buffer, lineLength - 1, emptylen);
	}

	return 0;
}

bool CDirectoryListingParser::ParseAsWfFtp(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// Get filename
	if (!line.GetToken(index++, token))
		return false;

	entry.name = token.GetString();

	// Get filesize
	if (!line.GetToken(index++, token))
		return false;

	if (!token.IsNumeric())
		return false;

	entry.size = token.GetNumber();

	entry.flags = 0;

	// Parse date
	if (!line.GetToken(index++, token))
		return false;

	if (!ParseShortDate(token, entry))
		return false;

	// Unused token
	if (!line.GetToken(index++, token))
		return false;

	if (token.GetString().Right(1) != _T("."))
		return false;

	// Parse time
	if (!line.GetToken(index++, token, true))
		return false;

	if (!ParseTime(token, entry))
		return false;

	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.time += m_timezoneOffset;

	return true;
}

bool CDirectoryListingParser::ParseAsIBM_MVS(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// volume
	if (!line.GetToken(index++, token))
		return false;

	// unit
	if (!line.GetToken(index++, token))
		return false;

	// Referred date
	if (!line.GetToken(index++, token))
		return false;

	entry.flags = 0;
	if (token.GetString() != _T("**NONE**") && !ParseShortDate(token, entry))
	{
		// Perhaps of the following type:
		// TSO004 3390 VSAM FOO.BAR
		if (token.GetString() != _T("VSAM"))
			return false;

		if (!line.GetToken(index++, token))
			return false;

		entry.name = token.GetString();
		if (entry.name.Find(' ') != -1)
			return false;

		entry.size = -1;
		entry.ownerGroup = objcache.get(wxString());
		entry.permissions = objcache.get(wxString());

		return true;
	}

	// ext
	if (!line.GetToken(index++, token))
		return false;
	if (!token.IsNumeric())
		return false;

	int prevLen = token.GetLength();

	// used
	if (!line.GetToken(index++, token))
		return false;
	if (token.IsNumeric() || token.GetString() == _T("????") || token.GetString() == _T("++++") )
	{
		// recfm
		if (!line.GetToken(index++, token))
			return false;
		if (token.IsNumeric())
			return false;
	}
	else
	{
		if (prevLen < 6)
			return false;
	}

	// lrecl
	if (!line.GetToken(index++, token))
		return false;
	if (!token.IsNumeric())
		return false;

	// blksize
	if (!line.GetToken(index++, token))
		return false;
	if (!token.IsNumeric())
		return false;

	// dsorg
	if (!line.GetToken(index++, token))
		return false;

	if (token.GetString() == _T("PO") || token.GetString() == _T("PO-E"))
	{
		entry.flags |= CDirentry::flag_dir;
		entry.size = -1;
	}
	else
		entry.size = 100;

	// name of dataset or sequential file
	if (!line.GetToken(index++, token, true))
		return false;

	entry.name = token.GetString();

	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());

	return true;
}

bool CDirectoryListingParser::ParseAsIBM_MVS_PDS(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// pds member name
	if (!line.GetToken(index++, token))
		return false;
	entry.name = token.GetString();

	// vv.mm
	if (!line.GetToken(index++, token))
		return false;

	entry.flags = 0;

	// creation date
	if (!line.GetToken(index++, token))
		return false;
	if (!ParseShortDate(token, entry))
		return false;

	// modification date
	if (!line.GetToken(index++, token))
		return false;
	if (!ParseShortDate(token, entry))
		return false;

	// modification time
	if (!line.GetToken(index++, token))
		return false;
	if (!ParseTime(token, entry))
		return false;

	// size
	if (!line.GetToken(index++, token))
		return false;
	if (!token.IsNumeric())
		return false;
	entry.size = token.GetNumber();

	// init
	if (!line.GetToken(index++, token))
		return false;
	if (!token.IsNumeric())
		return false;

	// mod
	if (!line.GetToken(index++, token))
		return false;
	if (!token.IsNumeric())
		return false;

	// id
	if (!line.GetToken(index++, token, true))
		return false;

	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.time += m_timezoneOffset;

	return true;
}

bool CDirectoryListingParser::ParseAsIBM_MVS_Migrated(CLine &line, CDirentry &entry)
{
	// Migrated MVS file
	// "Migrated				SOME.NAME"

	int index = 0;
	CToken token;
	if (!line.GetToken(index, token))
		return false;

	if (token.GetString().CmpNoCase(_T("Migrated")))
		return false;

	if (!line.GetToken(++index, token))
		return false;

	entry.name = token.GetString();

	entry.flags = 0;
	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.size = -1;

	if (line.GetToken(++index, token))
		return false;

	return true;
}

bool CDirectoryListingParser::ParseAsIBM_MVS_PDS2(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;
	if (!line.GetToken(index, token))
		return false;

	entry.name = token.GetString();

	entry.flags = 0;
	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.size = -1;

	if (!line.GetToken(++index, token))
		return true;

	entry.size = token.GetNumber(CToken::hex);
	if (entry.size == -1)
		return false;

	// Unused hexadecimal token
	if (!line.GetToken(++index, token))
		return false;
	if (!token.IsNumeric(CToken::hex))
		return false;

	// Unused numeric token
	if (!line.GetToken(++index, token))
		return false;
	if (!token.IsNumeric())
		return false;

	int start = ++index;
	while (line.GetToken(index, token)) {
		++index;
	}
	if ((index - start < 2))
		return false;
	--index;

	if (!line.GetToken(index, token)) {
		return false;
	}
	if (!token.IsNumeric() && (token.GetString() != _T("ANY")))
		return false;

	if (!line.GetToken(index - 1, token)) {
		return false;
	}
	if (!token.IsNumeric() && (token.GetString() != _T("ANY")))
		return false;

	for (int i = start; i < index - 1; ++i) {
		if (!line.GetToken(i, token)) {
			return false;
		}
		int len = token.GetLength();
		for (int j = 0; j < len; ++j)
			if (token[j] < 'A' || token[j] > 'Z')
				return false;
	}

	return true;
}

bool CDirectoryListingParser::ParseAsIBM_MVS_Tape(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// volume
	if (!line.GetToken(index++, token))
		return false;

	// unit
	if (!line.GetToken(index++, token))
		return false;

	if (token.GetString().CmpNoCase(_T("Tape")))
		return false;

	// dsname
	if (!line.GetToken(index++, token))
		return false;

	entry.name = token.GetString();
	entry.flags = 0;
	entry.ownerGroup = objcache.get(wxString());
	entry.permissions = objcache.get(wxString());
	entry.size = -1;

	if (line.GetToken(index++, token))
		return false;

	return true;
}

bool CDirectoryListingParser::ParseComplexFileSize(CToken& token, wxLongLong& size, int blocksize /*=-1*/)
{
	if (token.IsNumeric())
	{
		size = token.GetNumber();
		if (blocksize != -1)
			size *= blocksize;

		return true;
	}

	int len = token.GetLength();

	char last = token[len - 1];
	if (last == 'B' || last == 'b')
	{
		if (len == 1)
			return false;

		char c = token[--len - 1];
		if (c < '0' || c > '9')
		{
			--len;
			last = c;
		}
		else
			last = 0;
	}
	else if (last >= '0' && last <= '9')
		last = 0;
	else
	{
		if (--len == 0)
			return false;
	}

	size = 0;

	int dot = -1;
	for (int i = 0; i < len; ++i)
	{
		char c = token[i];
		if (c >= '0' && c <= '9')
		{
			size *= 10;
			size += c - '0';
		}
		else if (c == '.')
		{
			if (dot != -1)
				return false;
			dot = len - i - 1;
		}
		else
			return false;
	}
	switch (last)
	{
	case 'k':
	case 'K':
		size *= 1024;
		break;
	case 'm':
	case 'M':
		size *= 1024 * 1024;
		break;
	case 'g':
	case 'G':
		size *= 1024 * 1024 * 1024;
		break;
	case 't':
	case 'T':
		size *= 1024 * 1024;
		size *= 1024 * 1024;
		break;
	case 'b':
	case 'B':
		break;
	case 0:
		if (blocksize != -1)
			size *= blocksize;
		break;
	default:
		return false;
	}
	while (dot-- > 0)
		size /= 10;

	return true;
}

int CDirectoryListingParser::ParseAsMlsd(CLine &line, CDirentry &entry)
{
	// MLSD format as described here: http://www.ietf.org/internet-drafts/draft-ietf-ftpext-mlst-16.txt

	// Parsing is done strict, abort on slightest error.

	CToken token;

	if (!line.GetToken(0, token))
		return 0;

	wxString facts = token.GetString();
	if (facts.empty())
		return 0;

	entry.flags = 0;
	entry.size = -1;

	wxString owner, group, uid, gid;
	wxString ownerGroup;
	wxString permissions;

	while (!facts.empty()) {
		int delim = facts.Find(';');
		if (delim < 3) {
			if (delim != -1)
				return 0;
			else
				delim = facts.Len();
		}

		int const pos = facts.Find('=');
		if (pos < 1 || pos > delim)
			return 0;

		wxString factname = facts.Left(pos);
		MakeLowerAscii(factname);
		wxString value = facts.Mid(pos + 1, delim - pos - 1);
		if (factname == _T("type")) {
			int colonPos = value.Find(':');
			wxString valuePrefix;
			if (colonPos == -1)
				valuePrefix = value;
			else
				valuePrefix = value.Left(colonPos);
			MakeLowerAscii(valuePrefix);

			if (valuePrefix == _T("dir") && colonPos == -1)
				entry.flags |= CDirentry::flag_dir;
			else if (valuePrefix == _T("os.unix=slink") || valuePrefix == _T("os.unix=symlink")) {
				entry.flags |= CDirentry::flag_dir | CDirentry::flag_link;
				if (colonPos != -1)
					entry.target = CSparseOptional<wxString>(value.Mid(colonPos));
			}
			else if ((valuePrefix == _T("cdir") || valuePrefix == _T("pdir")) && colonPos == -1) {
				// Current and parent directory, don't parse it
				return 2;
			}
		}
		else if (factname == _T("size")) {
			entry.size = 0;

			for (unsigned int i = 0; i < value.Len(); ++i)
			{
				if (value[i] < '0' || value[i] > '9')
					return 0;
				entry.size *= 10;
				entry.size += value[i] - '0';
			}
		}
		else if (factname == _T("modify") ||
			(!entry.has_date() && factname == _T("create")))
		{
			wxChar const* fmt;
			CDateTime::Accuracy accuracy;
			if (value.size() >= 14) {
				fmt = _T("%Y%m%d%H%M%S");
				accuracy = CDateTime::seconds;
			}
			else if (value.size() >= 12) {
				fmt = _T("%Y%m%d%H%M");
				accuracy = CDateTime::minutes;
			}
			else if (value.size() >= 10) {
				fmt = _T("%Y%m%d%H");
				accuracy = CDateTime::hours;
			}
			else {
				fmt = _T("%Y%m%d");
				accuracy = CDateTime::days;
			}

			wxDateTime dateTime(today_);
			if (!dateTime.ParseFormat(value, fmt))
				return 0;

			if (accuracy != CDateTime::days) {
				entry.time = CDateTime(dateTime.FromTimezone(wxDateTime::UTC), accuracy);
			}
			else {
				entry.time = CDateTime(dateTime, accuracy);
			}
		}
		else if (factname == _T("perm")) {
			if (!value.empty()) {
				if (!permissions.empty())
					permissions = value + _T(" (") + permissions + _T(")");
				else
					permissions = value;
			}
		}
		else if (factname == _T("unix.mode"))
		{
			if (!permissions.empty())
				permissions = permissions + _T(" (") + value + _T(")");
			else
				permissions = value;
		}
		else if (factname == _T("unix.owner") || factname == _T("unix.user"))
			owner = value;
		else if (factname == _T("unix.group"))
			group = value;
		else if (factname == _T("unix.uid"))
			uid = value;
		else if (factname == _T("unix.gid"))
			gid = value;

		facts = facts.Mid(delim + 1);
	}

	// The order of the facts is undefined, so assemble ownerGroup in correct
	// order
	if (!owner.empty())
		ownerGroup += owner;
	else if (!uid.empty())
		ownerGroup += uid;
	if (!group.empty())
		ownerGroup += _T(" ") + group;
	else if (!gid.empty())
		ownerGroup += _T(" ") + gid;

	entry.ownerGroup = objcache.get(ownerGroup);
	entry.permissions = objcache.get(permissions);

	if (!line.GetToken(1, token, true, true))
		return 0;

	entry.name = token.GetString();

	return 1;
}

bool CDirectoryListingParser::ParseAsOS9(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// Get owner
	if (!line.GetToken(index++, token))
		return false;

	// Make sure it's number.number
	int pos = token.Find('.');
	if (pos == -1 || !pos || pos == ((int)token.GetLength() - 1))
		return false;

	if (!token.IsNumeric(0, pos))
		return false;

	if (!token.IsNumeric(pos + 1, token.GetLength() - pos - 1))
		return false;

	entry.ownerGroup = objcache.get(token.GetString());

	entry.flags = 0;

	// Get date
	if (!line.GetToken(index++, token))
		return false;

	if (!ParseShortDate(token, entry, true))
		return false;

	// Unused token
	if (!line.GetToken(index++, token))
		return false;

	// Get perms
	if (!line.GetToken(index++, token))
		return false;

	entry.permissions = objcache.get(token.GetString());

	if (token[0] == 'd')
		entry.flags |= CDirentry::flag_dir;

	// Unused token
	if (!line.GetToken(index++, token))
		return false;

	// Get Size
	if (!line.GetToken(index++, token))
		return false;

	if (!token.IsNumeric())
		return false;

	entry.size = token.GetNumber();

	// Filename
	if (!line.GetToken(index++, token, true))
		return false;

	entry.name = token.GetString();

	return true;
}

void CDirectoryListingParser::Reset()
{
	for (auto iter = m_DataList.begin(); iter != m_DataList.end(); ++iter)
		delete [] iter->p;
	m_DataList.clear();

	delete m_prevLine;
	m_prevLine = 0;

	m_entryList.clear();
	m_fileList.clear();
	m_currentOffset = 0;
	m_fileListOnly = true;
	m_maybeMultilineVms = false;
}

bool CDirectoryListingParser::ParseAsZVM(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// Get name
	if (!line.GetToken(index, token))
		return false;

	entry.name = token.GetString();

	// Get filename extension
	if (!line.GetToken(++index, token))
		return false;
	entry.name += _T(".") + token.GetString();

	// File format. Unused
	if (!line.GetToken(++index, token))
		return false;
	wxString format = token.GetString();
	if (format != _T("V") && format != _T("F"))
		return false;

	// Record length
	if (!line.GetToken(++index, token))
		return false;

	if (!token.IsNumeric())
		return false;

	entry.size = token.GetNumber();

	// Number of records
	if (!line.GetToken(++index, token))
		return false;

	if (!token.IsNumeric())
		return false;

	entry.size *= token.GetNumber();

	// Unused (Block size?)
	if (!line.GetToken(++index, token))
		return false;

	if (!token.IsNumeric())
		return false;

	entry.flags = 0;

	// Date
	if (!line.GetToken(++index, token))
		return false;

	if (!ParseShortDate(token, entry, true))
		return false;

	// Time
	if (!line.GetToken(++index, token))
		return false;

	if (!ParseTime(token, entry))
		return false;

	// Owner
	if (!line.GetToken(++index, token))
		return false;

	entry.ownerGroup = objcache.get(token.GetString());

	// No further token!
	if (line.GetToken(++index, token))
		return false;

	entry.permissions = objcache.get(wxString());
	entry.target.clear();
	entry.time += m_timezoneOffset;

	return true;
}

bool CDirectoryListingParser::ParseAsHPNonstop(CLine &line, CDirentry &entry)
{
	int index = 0;
	CToken token;

	// Get name
	if (!line.GetToken(index, token))
		return false;

	entry.name = token.GetString();

	// File code, numeric, unsuded
	if (!line.GetToken(++index, token))
		return false;
	if (!token.IsNumeric())
		return false;

	// Size
	if (!line.GetToken(++index, token))
		return false;
	if (!token.IsNumeric())
		return false;

	entry.size = token.GetNumber();

	entry.flags = 0;

	// Date
	if (!line.GetToken(++index, token))
		return false;
	if (!ParseShortDate(token, entry, false))
		return false;

	// Time
	if (!line.GetToken(++index, token))
		return false;
	if (!ParseTime(token, entry))
		return false;

	// Owner
	if (!line.GetToken(++index, token))
		return false;
	wxString ownerGroup = token.GetString();

	if (token[token.GetLength() - 1] == ',')
	{
		// Owner, part 2
		if (!line.GetToken(++index, token))
			return false;
		ownerGroup += _T(" ") + token.GetString();
	}
	entry.ownerGroup = objcache.get(ownerGroup);

	// Permissions
	if (!line.GetToken(++index, token))
		return false;
	entry.permissions = objcache.get(token.GetString());

	// Nothing
	if (line.GetToken(++index, token))
		return false;

	return true;
}

bool CDirectoryListingParser::GetMonthFromName(const wxString& name, int &month)
{
	auto iter = m_MonthNamesMap.find(name.Lower());
	if (iter == m_MonthNamesMap.end())
	{
		wxString lower(name);
		MakeLowerAscii(lower);
		iter = m_MonthNamesMap.find(lower);
		if (iter == m_MonthNamesMap.end())
			return false;
	}

	month = iter->second;

	return true;
}

char ebcdic_table[256] = {
	' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // 0
	' ',  ' ',  ' ',  ' ',  ' ',  '\n', ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '\n', // 1
	' ',  ' ',  ' ',  ' ',  ' ',  '\n', ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // 2
	' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // 3
	' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '.',  '<',  '(',  '+',  '|',  // 4
	'&',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '!',  '$',  '*',  ')',  ';',  ' ',  // 5
	'-',  '/',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '|',  ',',  '%',  '_',  '>',  '?',  // 6
	' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '`',  ':',  '#',  '@',  '\'', '=',  '"',  // 7
	' ',  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // 8
	' ',  'j',  'k',  'l',  'm',  'n',  'o',  'p',  'q',  'r',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // 9
	' ',  '~',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // a
	'^',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '[',  ']',  ' ',  ' ',  ' ',  ' ',  // b
	'{',  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // c
	'}',  'J',  'K',  'L',  'M',  'N',  'O',  'P',  'Q',  'R',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // d
	'\\', ' ',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  // e
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ' ',  ' ',  ' ',  ' ',  ' ',  ' '   // f
};

void CDirectoryListingParser::ConvertEncoding(char *pData, int len)
{
	if (m_listingEncoding != listingEncoding::ebcdic)
		return;

	for (int i = 0; i < len; ++i) {
		pData[i] = ebcdic_table[static_cast<unsigned char>(pData[i])];
	}
}

void CDirectoryListingParser::DeduceEncoding()
{
	if (m_listingEncoding != listingEncoding::unknown)
		return;

	int count[256];

	memset(&count, 0, sizeof(int)*256);

	for (std::list<t_list>::const_iterator it = m_DataList.begin(); it != m_DataList.end(); ++it)
	{
		for (int i = 0; i < it->len; ++i)
			++count[static_cast<unsigned char>(it->p[i])];
	}

	int count_normal = 0;
	int count_ebcdic = 0;
	for (int i = '0'; i <= '9'; ++i ) {
		count_normal += count[i];
	}
	for (int i = 'a'; i <= 'z'; ++i ) {
		count_normal += count[i];
	}
	for (int i = 'A'; i <= 'Z'; ++i ) {
		count_normal += count[i];
	}

	for (int i = 0x81; i <= 0x89; ++i ) {
		count_ebcdic += count[i];
	}
	for (int i = 0x91; i <= 0x99; ++i ) {
		count_ebcdic += count[i];
	}
	for (int i = 0xa2; i <= 0xa9; ++i ) {
		count_ebcdic += count[i];
	}
	for (int i = 0xc1; i <= 0xc9; ++i ) {
		count_ebcdic += count[i];
	}
	for (int i = 0xd1; i <= 0xd9; ++i ) {
		count_ebcdic += count[i];
	}
	for (int i = 0xe2; i <= 0xe9; ++i ) {
		count_ebcdic += count[i];
	}
	for (int i = 0xf0; i <= 0xf9; ++i ) {
		count_ebcdic += count[i];
	}


	if ((count[0x1f] || count[0x15] || count[0x25]) && !count[0x0a] && count[static_cast<unsigned char>('@')] && count[static_cast<unsigned char>('@')] > count[static_cast<unsigned char>(' ')] && count_ebcdic > count_normal)
	{
		m_pControlSocket->LogMessage(MessageType::Status, _("Received a directory listing which appears to be encoded in EBCDIC."));
		m_listingEncoding = listingEncoding::ebcdic;
		for (auto it = m_DataList.begin(); it != m_DataList.end(); ++it)
			ConvertEncoding(it->p, it->len);
	}
	else
		m_listingEncoding = listingEncoding::normal;
}
