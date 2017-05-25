// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2016 - Tim Kosse <tim.kosse@filezilla-project.org>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// Options.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "options.h"
#include "platform.h"
#include "version.h"
#include "tinyxml/tinyxml.h"
#include "iputils.h"
#include "OptionLimits.h"
#include "xml_utils.h"

std::list<COptions *> COptions::m_InstanceList;
std::recursive_mutex COptions::m_mutex;
COptions::t_OptionsCache COptions::m_sOptionsCache[OPTIONS_NUM];
BOOL COptions::m_bInitialized = FALSE;

SPEEDLIMITSLIST COptions::m_sSpeedLimits[2];

// Backslash-terminated
CStdString GetExecutableDirectory()
{
	CStdString ret;

	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	if (GetModuleFileName(0, buffer, MAX_PATH) > 0) {
		LPTSTR pos = _tcsrchr(buffer, '\\');
		if (pos) {
			*++pos = 0;
			ret = buffer;
		}
	}

	return ret;
}


/////////////////////////////////////////////////////////////////////////////
// COptionsHelperWindow

class COptionsHelperWindow
{
public:
	COptionsHelperWindow(COptions *pOptions)
	{
		ASSERT(pOptions);
		m_pOptions=pOptions;

		//Create window
		WNDCLASSEX wndclass;
		wndclass.cbSize = sizeof(wndclass);
		wndclass.style = 0;
		wndclass.lpfnWndProc = WindowProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = GetModuleHandle(0);
		wndclass.hIcon = 0;
		wndclass.hCursor = 0;
		wndclass.hbrBackground = 0;
		wndclass.lpszMenuName = 0;
		wndclass.lpszClassName = _T("COptions Helper Window");
		wndclass.hIconSm = 0;

		RegisterClassEx(&wndclass);

		m_hWnd = CreateWindow(_T("COptions Helper Window"), _T("COptions Helper Window"), 0, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0));
		ASSERT(m_hWnd);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)this);
	};

	virtual ~COptionsHelperWindow()
	{
		//Destroy window
		if (m_hWnd) {
			DestroyWindow(m_hWnd);
			m_hWnd = 0;
		}
	}

	HWND GetHwnd()
	{
		return m_hWnd;
	}

protected:
	static LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if (message == WM_USER) {
			COptionsHelperWindow *pWnd=(COptionsHelperWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (!pWnd)
				return 0;
			ASSERT(pWnd);
			ASSERT(pWnd->m_pOptions);
			for (int i=0;i<OPTIONS_NUM;i++)
				pWnd->m_pOptions->m_OptionsCache[i].bCached = FALSE;
			simple_lock lock(COptions::m_mutex);
			pWnd->m_pOptions->m_SpeedLimits[0] = COptions::m_sSpeedLimits[0];
			pWnd->m_pOptions->m_SpeedLimits[1] = COptions::m_sSpeedLimits[1];
		}
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
	COptions *m_pOptions;

private:
	HWND m_hWnd;
};

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptions

COptions::COptions()
{
	for (int i = 0; i < OPTIONS_NUM; ++i)
		m_OptionsCache[i].bCached = FALSE;

	m_pOptionsHelperWindow = new COptionsHelperWindow(this);
	simple_lock lock(m_mutex);
#ifdef _DEBUG
	for (std::list<COptions *>::iterator iter=m_InstanceList.begin(); iter != m_InstanceList.end(); ++iter)
		ASSERT(*iter != this);
#endif _DEBUG
	m_InstanceList.push_back(this);
	m_SpeedLimits[0] = m_sSpeedLimits[0];
	m_SpeedLimits[1] = m_sSpeedLimits[1];
}

COptions::~COptions()
{
	{
		simple_lock lock(m_mutex);
		std::list<COptions *>::iterator iter;
		for (iter=m_InstanceList.begin(); iter != m_InstanceList.end(); ++iter) {
			if (*iter == this)
				break;
		}

		ASSERT(iter != m_InstanceList.end());
		if (iter != m_InstanceList.end())
			m_InstanceList.erase(iter);
	}

	if (m_pOptionsHelperWindow)
		delete m_pOptionsHelperWindow;
	m_pOptionsHelperWindow=0;
}

void COptions::SetOption(int nOptionID, _int64 value, bool save /*=true*/)
{
	switch (nOptionID)
	{
	case OPTION_MAXUSERS:
		if (value < 0)
			value = 0;
		break;
	case OPTION_THREADNUM:
		if (value < 1)
			value = 2;
		else if (value > 50)
			value = 2;
		break;
	case OPTION_TIMEOUT:
		if (value < 0)
			value = 120;
		else if (value > 9999)
			value = 120;
		break;
	case OPTION_NOTRANSFERTIMEOUT:
		if (value < 600 && value != 0)
			value = 600;
		else if (value > 9999)
			value = 600;
		break;
	case OPTION_LOGINTIMEOUT:
		if (value < 0)
			value = 60;
		else if (value > 9999)
			value = 60;
		break;
	case OPTION_ADMINPORT:
		if (value > 65535)
			value = 14147;
		else if (value < 1)
			value = 14147;
		break;
	case OPTION_LOGTYPE:
		if (value != 0 && value != 1)
			value = 0;
		break;
	case OPTION_LOGLIMITSIZE:
		if ((value > 999999 || value < 10) && value != 0)
			value = 100;
		break;
	case OPTION_LOGDELETETIME:
		if (value > 999 || value < 0)
			value = 14;
		break;
	case OPTION_DOWNLOADSPEEDLIMITTYPE:
	case OPTION_UPLOADSPEEDLIMITTYPE:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_DOWNLOADSPEEDLIMIT:
	case OPTION_UPLOADSPEEDLIMIT:
		if (value < 1)
			value = 1;
		else if (value > 1048576)
			value = 1048576;
		break;
	case OPTION_BUFFERSIZE:
		if (value < 256 || value > (1024*1024))
			value = 32768;
		break;
	case OPTION_BUFFERSIZE2:
		if (value < 256 || value > (1024*1024*128))
			value = 262144;
		break;
	case OPTION_CUSTOMPASVIPTYPE:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_MODEZ_USE:
		if (value < 0 || value > 1)
			value = 0;
		break;
	case OPTION_MODEZ_LEVELMIN:
		if (value < 0 || value > 8)
			value = 1;
		break;
	case OPTION_MODEZ_LEVELMAX:
		if (value < 8 || value > 9)
			value = 9;
		break;
	case OPTION_MODEZ_ALLOWLOCAL:
		if (value < 0 || value > 1)
			value = 0;
		break;
	case OPTION_AUTOBAN_ATTEMPTS:
		if (value < OPTION_AUTOBAN_ATTEMPTS_MIN)
			value = OPTION_AUTOBAN_ATTEMPTS_MIN;
		if (value > OPTION_AUTOBAN_ATTEMPTS_MAX)
			value = OPTION_AUTOBAN_ATTEMPTS_MAX;
		break;
	case OPTION_AUTOBAN_BANTIME:
		if (value < 1)
			value = 1;
		if (value > 999)
			value = 999;
		break;
	}

	Init();

	{
		simple_lock lock(m_mutex);
		m_sOptionsCache[nOptionID-1].nType = 1;
		m_sOptionsCache[nOptionID-1].value = value;
		m_sOptionsCache[nOptionID-1].bCached = TRUE;
		m_OptionsCache[nOptionID-1] = m_sOptionsCache[nOptionID - 1];
	}

	if (!save)
		return;

	CStdString valuestr;
	valuestr.Format( _T("%I64d"), value);

	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	TiXmlDocument document;
	if (!XML::Load(document, xmlFileName))
		return;

	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot)
		return;

	TiXmlElement* pSettings = pRoot->FirstChildElement("Settings");
	if (!pSettings)
		pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName)
			continue;
		CStdString name(pName);
		if (name != m_Options[nOptionID-1].name)
			continue;

		break;
	}

	if (!pItem)
		pItem = pSettings->LinkEndChild(new TiXmlElement("Item"))->ToElement();
	pItem->Clear();
	pItem->SetAttribute("name", ConvToNetwork(m_Options[nOptionID-1].name).c_str());
	pItem->SetAttribute("type", "numeric");
	pItem->LinkEndChild(new TiXmlText(ConvToNetwork(valuestr).c_str()));

	XML::Save(document, xmlFileName);
}

void COptions::SetOption(int nOptionID, LPCTSTR value, bool save /*=true*/)
{
	CStdString str = value;
	Init();

	switch (nOptionID)
	{
	case OPTION_SERVERPORT:
	case OPTION_TLSPORTS:
		{
			std::set<int> portSet;

			str.TrimLeft(_T(" ,"));

			int pos = str.FindOneOf(_T(" ,"));
			while (pos != -1)
			{
				int port = _ttoi(str.Left(pos));
				if (port >= 1 && port <= 65535)
					portSet.insert(port);
				str = str.Mid(pos + 1);
				str.TrimLeft(_T(" ,"));
				pos = str.FindOneOf(_T(" ,"));
			}
			if (str != _T(""))
			{
				int port = _ttoi(str);
				if (port >= 1 && port <= 65535)
					portSet.insert(port);
			}

			str = _T("");
			for (int port : portSet) {
				CStdString tmp;
				tmp.Format(_T("%d "), port);
				str += tmp;
			}
			str.TrimRight(' ');
		}
		break;
	case OPTION_WELCOMEMESSAGE:
		{
			std::vector<CStdString> msgLines;
			int oldpos = 0;
			str.Replace(_T("\r\n"), _T("\n"));
			int pos = str.Find(_T("\n"));
			CStdString line;
			while (pos != -1) {
				if (pos)
				{
					line = str.Mid(oldpos, pos - oldpos);
					line = line.Left(CONST_WELCOMEMESSAGE_LINESIZE);
					line.TrimRight(_T(" "));
					if (msgLines.size() || line != _T(""))
						msgLines.push_back(line);
				}
				oldpos = pos + 1;
				pos = str.Find(_T("\n"), oldpos);
			}
			line = str.Mid(oldpos);
			if (line != _T("")) {
				line = line.Left(CONST_WELCOMEMESSAGE_LINESIZE);
				msgLines.push_back(line);
			}
			str = _T("");
			for (unsigned int i = 0; i < msgLines.size(); i++)
				str += msgLines[i] + _T("\r\n");
			str.TrimRight(_T("\r\n"));
			if (str == _T("")) {
				str = _T("%v");
				str += _T("\r\nwritten by Tim Kosse (tim.kosse@filezilla-project.org)");
				str += _T("\r\nPlease visit https://filezilla-project.org/");
			}
			str.Replace(_T("tim.kosse@gmx.de"), _T("tim.kosse@filezilla-project.org"));
			str.Replace(_T("http://sourceforge.net/projects/filezilla/"), _T("https://filezilla-project.org/"));
		}
		break;
	case OPTION_ADMINIPBINDINGS:
		{
			CStdString sub;
			std::list<CStdString> ipBindList;
			for (unsigned int i = 0; i<_tcslen(value); i++)
			{
				TCHAR cur = value[i];
				if ((cur < '0' || cur > '9') && cur != '.' && cur != ':')
				{
					if (sub == _T("") && cur == '*')
					{
						ipBindList.clear();
						ipBindList.push_back(_T("*"));
						break;
					}

					if (sub != _T(""))
					{
						if (IsIpAddress(sub))
							ipBindList.push_back(sub);
						sub = _T("");
					}
				}
				else
					sub += cur;
			}
			if (sub != _T(""))
			{
				if (IsIpAddress(sub))
					ipBindList.push_back(sub);
			}
			str = _T("");
			for (std::list<CStdString>::iterator iter = ipBindList.begin(); iter!=ipBindList.end(); iter++)
				if (!IsLocalhost(*iter))
					str += *iter + _T(" ");

			str.TrimRight(_T(" "));
		}
		break;
	case OPTION_ADMINPASS:
		if (str != _T("") && str.GetLength() < 6)
			return;
		break;
	case OPTION_MODEZ_DISALLOWED_IPS:
	case OPTION_IPFILTER_ALLOWED:
	case OPTION_IPFILTER_DISALLOWED:
	case OPTION_ADMINIPADDRESSES:
		{
			str.Replace('\r', ' ');
			str.Replace('\n', ' ');
			str.Replace('\r', ' ');
			while (str.Replace(_T("  "), _T(" ")));
			str += _T(" ");

			CStdString ips;

			int pos = str.Find(' ');
			while (pos != -1)
			{
				CStdString sub = str.Left(pos);
				str = str.Mid(pos + 1);
				str.TrimLeft(' ');

				if (sub == _T("*"))
					ips += _T(" ") + sub;
				else
				{
					if (IsValidAddressFilter(sub))
						ips += " " + sub;
					pos = str.Find(' ');
				}
			}
			ips.TrimLeft(' ');

			str = ips;
		}
		break;
	case OPTION_IPBINDINGS:
		{
			std::list<CStdString> ipBindList;

			str += _T(" ");
			while (!str.empty()) {
				int pos  = str.Find(' ');
				if (pos < 0) {
					break;
				}
				CStdString sub = str.Left(pos);
				str = str.Mid(pos + 1);

				if (sub == _T("*")) {
					ipBindList.clear();
					ipBindList.push_back(_T("*"));
					break;
				}
				else if (IsIpAddress(sub, true)) {
					ipBindList.push_back(sub);
				}
			}

			if (ipBindList.empty())
				ipBindList.push_back(_T("*"));

			str.clear();
			for (auto const& ip : ipBindList) {
				str += ip + _T(" ");
			}

			str.TrimRight(_T(" "));
		}
		break;
	case OPTION_CUSTOMPASVIPSERVER:
		if (str.Find(_T("filezilla.sourceforge.net")) != -1)
			str = _T("http://ip.filezilla-project.org/ip.php");
		break;
	}

	{
		simple_lock lock(m_mutex);
		m_sOptionsCache[nOptionID-1].bCached = TRUE;
		m_sOptionsCache[nOptionID-1].nType = 0;
		m_sOptionsCache[nOptionID-1].str = str;
		m_OptionsCache[nOptionID-1]=m_sOptionsCache[nOptionID-1];
	}

	if (!save)
		return;

	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");
	
	TiXmlDocument document;
	if (!XML::Load(document, xmlFileName)) {
		return;
	}

	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot)
		return;

	TiXmlElement* pSettings = pRoot->FirstChildElement("Settings");
	if (!pSettings)
		pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName)
			continue;
		CStdString name(pName);
		if (name != m_Options[nOptionID-1].name)
			continue;

		break;
	}

	if (!pItem)
		pItem = pSettings->LinkEndChild(new TiXmlElement("Item"))->ToElement();
	pItem->Clear();
	pItem->SetAttribute("name", ConvToNetwork(m_Options[nOptionID - 1].name).c_str());
	pItem->SetAttribute("type", "string");
	pItem->LinkEndChild(new TiXmlText(ConvToNetwork(value).c_str()));

	XML::Save(document, xmlFileName);
}

CStdString COptions::GetOption(int nOptionID)
{
	ASSERT(nOptionID>0 && nOptionID<=OPTIONS_NUM);
	ASSERT(!m_Options[nOptionID-1].nType);
	Init();

	if (m_OptionsCache[nOptionID-1].bCached)
		return m_OptionsCache[nOptionID-1].str;

	simple_lock lock(m_mutex);

	if (!m_sOptionsCache[nOptionID-1].bCached) {
		//Default values
		switch (nOptionID)
		{
		case OPTION_SERVERPORT:
			m_sOptionsCache[nOptionID-1].str = _T("21");
			break;
		case OPTION_WELCOMEMESSAGE:
			m_sOptionsCache[nOptionID-1].str = _T("%v");
			m_sOptionsCache[nOptionID-1].str += _T("\r\nwritten by Tim Kosse (tim.kosse@filezilla-project.org)");
			m_sOptionsCache[nOptionID-1].str += _T("\r\nPlease visit https://filezilla-project.org/");
			break;
		case OPTION_CUSTOMPASVIPSERVER:
			m_sOptionsCache[nOptionID-1].str = _T("http://ip.filezilla-project.org/ip.php");
			break;
		case OPTION_IPBINDINGS:
			m_sOptionsCache[nOptionID-1].str = _T("*");
			break;
		case OPTION_TLSPORTS:
			m_sOptionsCache[nOptionID-1].str = _T("990");
			break;
		default:
			m_sOptionsCache[nOptionID-1].str = _T("");
			break;
		}
		m_sOptionsCache[nOptionID-1].bCached = TRUE;
		m_sOptionsCache[nOptionID-1].nType = 0;
	}
	m_OptionsCache[nOptionID-1] = m_sOptionsCache[nOptionID - 1];
	return m_OptionsCache[nOptionID-1].str;
}

_int64 COptions::GetOptionVal(int nOptionID)
{
	ASSERT(nOptionID>0 && nOptionID<=OPTIONS_NUM);
	ASSERT(m_Options[nOptionID-1].nType == 1);
	Init();

	if (m_OptionsCache[nOptionID-1].bCached)
		return m_OptionsCache[nOptionID-1].value;

	simple_lock lock(m_mutex);

	if (!m_sOptionsCache[nOptionID-1].bCached) {
		//Default values
		switch (nOptionID)
		{
			case OPTION_CHECK_DATA_CONNECTION_IP:
				m_sOptionsCache[nOptionID-1].value = 2;
				break;
			case OPTION_MAXUSERS:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_THREADNUM:
				m_sOptionsCache[nOptionID-1].value = 2;
				break;
			case OPTION_TIMEOUT:
			case OPTION_NOTRANSFERTIMEOUT:
				m_sOptionsCache[nOptionID-1].value = 120;
				break;
			case OPTION_LOGINTIMEOUT:
				m_sOptionsCache[nOptionID-1].value = 60;
				break;
			case OPTION_ADMINPORT:
				m_sOptionsCache[nOptionID-1].value = 14147;
				break;
            //ÆôÓÃÈÕÖ¾
            case OPTION_ENABLELOGGING:
                m_sOptionsCache[nOptionID - 1].value = 1;
                break;
			case OPTION_DOWNLOADSPEEDLIMIT:
			case OPTION_UPLOADSPEEDLIMIT:
				m_sOptionsCache[nOptionID-1].value = 10;
				break;
			case OPTION_BUFFERSIZE:
				m_sOptionsCache[nOptionID-1].value = 32768;
				break;
			case OPTION_CUSTOMPASVIPTYPE:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_MODEZ_USE:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_MODEZ_LEVELMIN:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_MODEZ_LEVELMAX:
				m_sOptionsCache[nOptionID-1].value = 9;
				break;
			case OPTION_MODEZ_ALLOWLOCAL:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_ALLOWEXPLICITTLS:
			case OPTION_FORCEPROTP:
			case OPTION_TLS_REQUIRE_SESSION_RESUMPTION:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_BUFFERSIZE2:
				m_sOptionsCache[nOptionID-1].value = 65536*4;
				break;
			case OPTION_NOEXTERNALIPONLOCAL:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_ACTIVE_IGNORELOCAL:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_AUTOBAN_BANTIME:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_AUTOBAN_ATTEMPTS:
				m_sOptionsCache[nOptionID-1].value = 10;
				break;
            //enable SSL
            //case OPTION_ENABLETLS:
            //    m_sOptionsCache[nOptionID - 1].value = 1;
                break;
			default:
				m_sOptionsCache[nOptionID-1].value = 0;
		}
		m_sOptionsCache[nOptionID-1].bCached=TRUE;
		m_sOptionsCache[nOptionID-1].nType=1;
	}
	m_OptionsCache[nOptionID-1]=m_sOptionsCache[nOptionID-1];
	return m_OptionsCache[nOptionID-1].value;
}

void COptions::UpdateInstances()
{
	simple_lock lock(m_mutex);
	for (auto const& pOptions : m_InstanceList) {
		ASSERT(pOptions->m_pOptionsHelperWindow);
		::PostMessage(pOptions->m_pOptionsHelperWindow->GetHwnd(), WM_USER, 0, 0);
	}
}

void COptions::Init()
{
	if (m_bInitialized)
		return;
	simple_lock lock(m_mutex);
	m_bInitialized = TRUE;

	for (int i = 0; i < OPTIONS_NUM; ++i)
		m_sOptionsCache[i].bCached = FALSE;

	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	TiXmlDocument document;

	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (!GetStatus64(xmlFileName, status) ) {
		document.LinkEndChild(new TiXmlElement("FileZillaServer"));
		XML::Save(document, xmlFileName);
	}
	else if (status.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return;
	}

	if (!XML::Load(document, xmlFileName)) {
		return;
	}

	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot) {
		return;
	}

	TiXmlElement* pSettings = pRoot->FirstChildElement("Settings");
	if (!pSettings)
		pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName)
			continue;
		CStdString name(pName);

		const char* pType = pItem->Attribute("type");
		if (!pType)
			continue;
		CStdString type(pType);

		TiXmlNode* textNode = pItem->FirstChild();
		CStdString value;
		if (textNode && textNode->ToText())
			value = ConvFromNetwork(textNode->Value());
		else if (type == _T("numeric"))
			continue;

		for (int i = 0; i < OPTIONS_NUM; ++i) {
			if (!_tcscmp(name, m_Options[i].name)) {
				if (m_sOptionsCache[i].bCached)
					break;

				if (type == _T("numeric")) {
					if (m_Options[i].nType != 1)
						break;
					_int64 value64 = _ttoi64(value);
					if (IsNumeric(value))
						SetOption(i + 1, value64, false);
				}
				else {
					if (m_Options[i].nType != 0)
						break;
					SetOption(i + 1, value, false);
				}
				break;
			}
		}
	}
	ReadSpeedLimits(pSettings);

	UpdateInstances();
}

bool COptions::IsNumeric(LPCTSTR str)
{
	if (!str)
		return false;
	LPCTSTR p = str;
	while (*p) {
		if (*p < '0' || *p > '9') {
			return false;
		}
		++p;
	}
	return true;
}

TiXmlElement *COptions::GetXML()
{
	simple_lock lock(m_mutex);

	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	TiXmlDocument *pDocument = new TiXmlDocument;

	if (!XML::Load(*pDocument, xmlFileName)) {
		delete pDocument;
		return NULL;
	}

	TiXmlElement* pElement = pDocument->FirstChildElement("FileZillaServer");
	if (!pElement) {
		delete pDocument;
		return NULL;
	}

	// Must call FreeXML
	m_mutex.lock();

	return pElement;
}

BOOL COptions::FreeXML(TiXmlElement *pXML, bool save)
{
	ASSERT(pXML);
	if (!pXML)
		return FALSE;

	simple_lock lock(m_mutex);

	// As locked by GetXML
	m_mutex.unlock();

	if (!save) {
		delete pXML->GetDocument();
		return FALSE;
	}

	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");
	bool ret = XML::Save(*pXML, xmlFileName);

	delete pXML->GetDocument();

	return ret;
}

bool COptions::GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength)
{
	int i;
	DWORD len = 2;

	simple_lock lock(m_mutex);
	for (i = 0; i < OPTIONS_NUM; ++i) {
		len += 1;
		if (!m_Options[i].nType) {
			len += 3;
			auto utf8 = ConvToNetwork(GetOption(i + 1));

			if ((i + 1) != OPTION_ADMINPASS) {
				len += utf8.size();
			}
			else {
				if (GetOption(i+1).GetLength() < 6 && utf8.size() > 0)
					len++;
			}
		}
		else
			len += 8;
	}

	len += 4;
	for (i = 0; i < 2; ++i) {
		if (m_sSpeedLimits[i].size() > 0xffff) {
			return false;
		}
		for (auto const& limit : m_sSpeedLimits[i]) {
			len += limit.GetRequiredBufferLen();
		}
	}

	*pBuffer = new unsigned char[len];
	unsigned char *p = *pBuffer;
	*p++ = OPTIONS_NUM / 256;
	*p++ = OPTIONS_NUM % 256;
	for (i = 0; i < OPTIONS_NUM; ++i) {
		*p++ = m_Options[i].nType;
		switch (m_Options[i].nType) {
		case 0:
			{
				CStdString str = GetOption(i+1);
				if ((i+1) == OPTION_ADMINPASS) //Do NOT send admin password,
											 //instead send empty string if admin pass is set
											 //and send a single char if admin pass is invalid (len < 6)
				{
					if (str.GetLength() >= 6 || str == _T("")) {
						str = _T("");
					}
					else {
						str = _T("*");
					}
				}

				auto utf8 = ConvToNetwork(str);

				int len = utf8.size();
				*p++ = (len / 256) / 256;
				*p++ = (len / 256) % 256;
				*p++ = len % 256;
				memcpy(p, utf8.c_str(), len);
				p += len;
			}
			break;
		case 1:
			{
				_int64 value = GetOptionVal(i+1);
				memcpy(p, &value, 8);
				p+=8;
			}
			break;
		default:
			ASSERT(FALSE);
		}
	}

	for (i = 0; i < 2; ++i) {
		*p++ = (m_sSpeedLimits[i].size() >> 8) & 0xffu;
		*p++ = m_sSpeedLimits[i].size() % 256;
		for (auto const& limit : m_sSpeedLimits[i]) {
			p = limit.FillBuffer(p);
		}
	}

	*nBufferLength = len;

	return TRUE;
}

BOOL COptions::ParseOptionsCommand(unsigned char *pData, DWORD dwDataLength, BOOL bFromLocal /*=FALSE*/)
{
	unsigned char *p = pData;
	int num = *p * 256 + p[1];
	p+=2;
	if (num!=OPTIONS_NUM)
		return FALSE;

	int i;
	for (i = 0; i < num; ++i) {
		if ((DWORD)(p-pData)>=dwDataLength)
			return FALSE;
		int nType = *p++;
		if (!nType)
		{
			if ((DWORD)(p-pData+3) >= dwDataLength)
				return 2;
			int len = *p * 256 * 256 + p[1] * 256 + p[2];
			p += 3;
			if ((DWORD)(p - pData + len) > dwDataLength)
				return FALSE;
			char *pBuffer = new char[len + 1];
			memcpy(pBuffer, p, len);
			pBuffer[len]=0;
			if (!m_Options[i].bOnlyLocal || bFromLocal) //Do not change admin interface settings from remote connections
				SetOption(i+1, ConvFromNetwork(pBuffer), false);
			delete [] pBuffer;
			p+=len;
		}
		else if (nType == 1)
		{
			if ((DWORD)(p-pData+8)>dwDataLength)
				return FALSE;
			if (!m_Options[i].bOnlyLocal || bFromLocal) //Do not change admin interface settings from remote connections
				SetOption(i+1, GET64(p), false);
			p+=8;
		}
		else
			return FALSE;
	}

	SPEEDLIMITSLIST dl;
	SPEEDLIMITSLIST ul;

	if ((DWORD)(p-pData+2)>dwDataLength)
		return FALSE;
	num = *p++ << 8;
	num |= *p++;

	simple_lock lock(m_mutex);

	for (i=0; i<num; ++i) {
		CSpeedLimit limit;
		p = limit.ParseBuffer(p, dwDataLength - (p - pData));
		if (!p) {
			return FALSE;
		}
		dl.push_back(limit);
	}

	if ((DWORD)(p-pData+2)>dwDataLength) {
		return FALSE;
	}
	num = *p++ << 8;
	num |= *p++;
	for (i=0; i<num; i++)
	{
		CSpeedLimit limit;
		p = limit.ParseBuffer(p, dwDataLength - (p - pData));
		if (!p) {
			return FALSE;
		}
		ul.push_back(limit);
	}

	m_sSpeedLimits[0] = dl;
	m_sSpeedLimits[1] = ul;

	SaveOptions();

	UpdateInstances();

	return TRUE;
}

static void SetText(TiXmlElement* pElement, const CStdString& text)
{
	pElement->Clear();
	pElement->LinkEndChild(new TiXmlText(ConvToNetwork(text).c_str()));
}

BOOL COptions::SaveSpeedLimits(TiXmlElement* pSettings)
{
	TiXmlElement* pSpeedLimits;
	while ((pSpeedLimits = pSettings->FirstChildElement("SpeedLimits")))
		pSettings->RemoveChild(pSpeedLimits);

	pSpeedLimits = pSettings->LinkEndChild(new TiXmlElement("SpeedLimits"))->ToElement();

	const char* names[] = { "Download", "Upload" };

	for (int i = 0; i < 2; i++)
	{
		TiXmlElement* pSpeedLimit = new TiXmlElement(names[i]);
		pSpeedLimits->LinkEndChild(pSpeedLimit);

		for (unsigned int j = 0; j < m_sSpeedLimits[i].size(); j++)
		{
			CSpeedLimit limit = m_sSpeedLimits[i][j];

			TiXmlElement* pRule = pSpeedLimit->LinkEndChild(new TiXmlElement("Rule"))->ToElement();
			limit.Save(pRule);
		}
	}

	return TRUE;
}

CStdString ReadText(TiXmlElement* pElement)
{
	TiXmlNode* textNode = pElement->FirstChild();
	if (!textNode || !textNode->ToText())
		return _T("");

	return ConvFromNetwork(textNode->Value());
}

BOOL COptions::ReadSpeedLimits(TiXmlElement *pXML)
{
	const char* names[] = { "Download", "Upload" };

	for (int i = 0; i < 2; i++)
	{
		for (TiXmlElement* pSpeedLimits = pXML->FirstChildElement("SpeedLimits"); pSpeedLimits; pSpeedLimits = pSpeedLimits->NextSiblingElement("SpeedLimits"))
		{
			for (TiXmlElement* pLimit = pSpeedLimits->FirstChildElement(names[i]); pLimit; pLimit = pLimit->NextSiblingElement(names[i]))
			{
				for (TiXmlElement* pRule = pLimit->FirstChildElement("Rule"); pRule; pRule = pRule->NextSiblingElement("Rule"))
				{
					CSpeedLimit limit;
					if (!limit.Load(pRule))
						continue;

					if (m_sSpeedLimits[i].size() < 20000)
						m_sSpeedLimits[i].push_back(limit);
				}
			}
		}
	}

	return TRUE;
}

int COptions::GetCurrentSpeedLimit(int nMode)
{
	Init();

	int type[2] = { OPTION_DOWNLOADSPEEDLIMITTYPE, OPTION_UPLOADSPEEDLIMITTYPE };
	int limit[2] = { OPTION_DOWNLOADSPEEDLIMIT, OPTION_UPLOADSPEEDLIMIT };

	int nType = (int)GetOptionVal(type[nMode]);
	switch (nType)
	{
	case 0:
		return -1;
	case 1:
		return (int)GetOptionVal(limit[nMode]);
	default:
		{
			SYSTEMTIME s;
			GetLocalTime(&s);
			for (SPEEDLIMITSLIST::const_iterator iter = m_SpeedLimits[nMode].begin(); iter != m_SpeedLimits[nMode].end(); iter++)
				if (iter->IsItActive(s))
					return iter->m_Speed;
			return -1;
		}
	}
}

void COptions::ReloadConfig()
{
	simple_lock lock(m_mutex);

	m_bInitialized = TRUE;

	for (int i = 0; i < OPTIONS_NUM; i++)
		m_sOptionsCache[i].bCached = FALSE;

	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	TiXmlDocument document;

	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (!GetStatus64(xmlFileName, status) ) {
		document.LinkEndChild(new TiXmlElement("FileZillaServer"));
		XML::Save(document, xmlFileName);
	}
	else if (status.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return;
	}

	if (!XML::Load(document, xmlFileName)) {
		return;
	}

	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot) {
		return;
	}

	TiXmlElement* pSettings = pRoot->FirstChildElement("Settings");
	if (!pSettings)
		pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName)
			continue;
		CStdString name(pName);
		const char* pType = pItem->Attribute("type");
		if (!pType)
			continue;
		CStdString type(pType);
		TiXmlNode* textNode = pItem->FirstChild();
		if (!textNode || !textNode->ToText())
			continue;
		CStdString value = ConvFromNetwork(textNode->Value());


		for (int i = 0;i < OPTIONS_NUM; ++i) {
			if (!_tcscmp(name, m_Options[i].name)) {
				if (m_sOptionsCache[i].bCached)
					break;

				if (type == _T("numeric")) {
					if (m_Options[i].nType != 1)
						break;
					_int64 value64 = _ttoi64(value);
					if (IsNumeric(value))
						SetOption(i + 1, value64, false);
				}
				else {
					if (m_Options[i].nType != 0)
						break;
					SetOption(i  +1, value, false);
				}
				break;
			}
		}
	}
	ReadSpeedLimits(pSettings);

	UpdateInstances();
}

void COptions::SaveOptions()
{
	CStdString const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	TiXmlDocument document;
	if (!XML::Load(document, xmlFileName)) {
		return;
	}

	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot)
		return;

	TiXmlElement* pSettings;
	while ((pSettings = pRoot->FirstChildElement("Settings")))
		pRoot->RemoveChild(pSettings);
	pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	for (unsigned int i = 0; i < OPTIONS_NUM; ++i) {
		if (!m_OptionsCache[i].bCached)
			continue;

		CStdString valuestr;
		if (!m_OptionsCache[i].nType)
			valuestr = m_OptionsCache[i].str;
		else
			valuestr.Format( _T("%I64d"), m_OptionsCache[i].value);

		TiXmlElement* pItem = pSettings->LinkEndChild(new TiXmlElement("Item"))->ToElement();
		pItem->SetAttribute("name", ConvToNetwork(m_Options[i].name).c_str());
		if (!m_OptionsCache[i].nType)
			pItem->SetAttribute("type", "string");
		else
			pItem->SetAttribute("type", "numeric");
		pItem->LinkEndChild(new TiXmlText(ConvToNetwork(valuestr).c_str()));
	}

	SaveSpeedLimits(pSettings);

	XML::Save(document, xmlFileName);
}
