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
#include "../version.h"
#include "filezilla server.h"
#include "../tinyxml/tinyxml.h"
#include "../xml_utils.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BOOL COptions::m_bInitialized = FALSE;

namespace {
TiXmlElement* GetSettings(TiXmlDocument& document)
{
	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot) {
		if (document.FirstChildElement()) {
			return 0;
		}
		pRoot = document.LinkEndChild(new TiXmlElement("FileZillaServer"))->ToElement();
	}

	TiXmlElement* pSettings = pRoot->FirstChildElement("Settings");
	if (!pSettings)
		pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	return pSettings;
}
}

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptions

struct t_Option final
{
	TCHAR name[30];
	int nType;
};

static const t_Option m_Options[IOPTIONS_NUM] = {
	{ _T("Start Minimized"),        1 },
	{ _T("Last Server Address"),    0 },
	{ _T("Last Server Port"),       1 },
	{ _T("Last Server Password"),   0 },
	{ _T("Always use last server"), 1 },
	{ _T("User Sorting"),           1 },
	{ _T("Filename Display"),       1 },
	{ _T("Max reconnect count"),    1 },
};

void COptions::SetOption(int nOptionID, __int64 value)
{
	Init();

	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 1;
	m_OptionsCache[nOptionID - 1].value = value;

	CString valuestr;
	valuestr.Format(_T("%I64d"), value);

	SaveOption(nOptionID, valuestr);
}

void COptions::SetOption(int nOptionID, CString const& value)
{
	Init();

	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 0;
	m_OptionsCache[nOptionID - 1].str = value;

	SaveOption(nOptionID, value);
}

void COptions::SaveOption(int nOptionID, CString const& value)
{
	CString const file = GetFileName(true);

	TiXmlDocument document;
	XML::Load(document, file);

	TiXmlElement* pSettings = GetSettings(document);
	if (!pSettings)
		return;

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName)
			continue;
		CString name(pName);
		if (name != m_Options[nOptionID - 1].name)
			continue;

		break;
	}

	if (!pItem)
		pItem = pSettings->LinkEndChild(new TiXmlElement("Item"))->ToElement();
	pItem->Clear();
	pItem->SetAttribute("name", ConvToNetwork(m_Options[nOptionID - 1].name).c_str());
	pItem->SetAttribute("type", m_OptionsCache[nOptionID - 1].nType ? "numeric" : "string");
	pItem->LinkEndChild(new TiXmlText(ConvToNetwork(value).c_str()));

	XML::Save(document, file);
}

CString COptions::GetOption(int nOptionID)
{
	ASSERT(nOptionID > 0 && nOptionID <= IOPTIONS_NUM);
	ASSERT(!m_Options[nOptionID - 1].nType);
	Init();

	if (m_OptionsCache[nOptionID - 1].bCached)
		return m_OptionsCache[nOptionID - 1].str;

	//Verification
	switch (nOptionID)
	{
	case IOPTION_LASTSERVERADDRESS:
		m_OptionsCache[nOptionID - 1].str = _T("localhost");
		break;
	default:
		m_OptionsCache[nOptionID - 1].str = "";
		break;
	}
	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 0;
	return m_OptionsCache[nOptionID - 1].str;
}

__int64 COptions::GetOptionVal(int nOptionID)
{
	ASSERT(nOptionID>0 && nOptionID<=IOPTIONS_NUM);
	ASSERT(m_Options[nOptionID - 1].nType == 1);
	Init();

	if (m_OptionsCache[nOptionID - 1].bCached)
		return m_OptionsCache[nOptionID - 1].value;

	switch (nOptionID) {
	case IOPTION_LASTSERVERPORT:
		m_OptionsCache[nOptionID - 1].value = 14147;
		break;
	case IOPTION_RECONNECTCOUNT:
		m_OptionsCache[nOptionID - 1].value = 15;
		break;
	default:
		m_OptionsCache[nOptionID - 1].value = 0;
	}
	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 0;
	return m_OptionsCache[nOptionID - 1].value;
}

CString COptions::GetFileName(bool for_saving)
{
	CString ret;

	PWSTR str = 0;
	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &str) == S_OK && str && *str) {
		CString path = str;
		path += _T("\\FileZilla Server\\");

		if (for_saving) {
			CreateDirectory(path, 0);
		}
		path += _T("FileZilla Server Interface.xml");
		
		if (!for_saving) {
			CFileStatus status;
			if (CFile::GetStatus(path, status)) {
				ret = path;
			}
		}
		else {
			ret = path;
		}

		CoTaskMemFree(str);
	}

	if (ret.IsEmpty() && !for_saving) {
		TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
		GetModuleFileName(0, buffer, MAX_PATH);
		LPTSTR pos = _tcsrchr(buffer, '\\');
		if (pos)
			*++pos = 0;
		ret = buffer;
		ret += _T("FileZilla Server Interface.xml");
	}

	return ret;
}

void COptions::Init()
{
	if (m_bInitialized)
		return;
	m_bInitialized = TRUE;
		
	for (int i = 0; i < IOPTIONS_NUM; i++)
		m_OptionsCache[i].bCached = FALSE;

	TiXmlDocument document;
	if (!XML::Load(document, GetFileName(false))) {
		return;
	}

	TiXmlElement* pSettings = GetSettings(document);
	if (!pSettings)
		return;

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName)
			continue;
		CString name(pName);
		const char* pType = pItem->Attribute("type");
		if (!pType)
			continue;
		CString type(pType);
		TiXmlNode* textNode = pItem->FirstChild();
		if (!textNode || !textNode->ToText())
			continue;
		CString value = ConvFromNetwork(textNode->Value());

		for (int i = 0; i < IOPTIONS_NUM; ++i) {
			if (name != m_Options[i].name)
				continue;

			if (m_OptionsCache[i].bCached) {
				::AfxTrace( _T("Item '%s' is already in cache, ignoring item\n"), name);
				break;
			}

			if (type == "numeric") {
				if (m_Options[i].nType != 1) {
					::AfxTrace( _T("Type mismatch for option '%s'. Type is %d, should be %d"), name, m_Options[i].nType, 1);
					break;
				}
				m_OptionsCache[i].bCached = TRUE;
				m_OptionsCache[i].nType = 1;
				_int64 value64=_ttoi64(value);

				switch (i+1)
				{
				case IOPTION_LASTSERVERPORT:
					if (value64 < 1 || value64 > 65535)
						value64 = 14147;
					break;
				default:
					break;
				}

				m_OptionsCache[i].value = value64;
			}
			else {
				if (type != "string")
					::AfxTrace( _T("Unknown option type '%s' for item '%s', assuming string\n"), type, name);
				if (m_Options[i].nType != 0)
				{
					::AfxTrace( _T("Type mismatch for option '%s'. Type is %d, should be %d"), name, m_Options[i].nType, 0);
					break;
				}
				m_OptionsCache[i].bCached = TRUE;
				m_OptionsCache[i].nType = 0;

				m_OptionsCache[i].str = value;
			}
		}
	}
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
		p++;
	}
	return true;
}
