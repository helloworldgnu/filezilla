#include <filezilla.h>
#include "xmlfunctions.h"
#include "Options.h"
#include <wx/ffile.h>
#include <wx/log.h>
#include <wx/base64.h>

#include <local_filesys.h>

CXmlFile::CXmlFile(wxString const& fileName, wxString const& root)
{
	if (!root.empty()) {
		m_rootName = root;
	}
	SetFileName(fileName);
}

void CXmlFile::SetFileName(const wxString& name)
{
	wxASSERT(!name.empty());
	m_fileName = name;
	m_modificationTime = CDateTime();
}

CXmlFile::~CXmlFile()
{
	Close();
}

TiXmlElement* CXmlFile::Load()
{
	Close();
	m_error.clear();

	wxCHECK(!m_fileName.empty(), 0);

	wxString redirectedName = GetRedirectedName();

	GetXmlFile(redirectedName);
	if (!m_pElement) {
		wxString err = wxString::Format(_("The file '%s' could not be loaded."), m_fileName);
		if (m_error.empty()) {
			err += wxString(_T("\n")) + _("Make sure the file can be accessed and is a well-formed XML document.");
		}
		else {
			err += _T("\n") + m_error;
		}

		// Try the backup file
		GetXmlFile(redirectedName + _T("~"));
		if (!m_pElement) {
			// Loading backup failed. If both original and backup file are empty, create new file.
			if (CLocalFileSystem::GetSize(redirectedName) <= 0 && CLocalFileSystem::GetSize(redirectedName + _T("~")) <= 0) {
				m_error.clear();
				CreateEmpty();
				m_modificationTime = CLocalFileSystem::GetModificationTime(redirectedName);
				return m_pElement;
			}

			// File corrupt and no functional backup, give up.
			m_error = err;
			m_modificationTime.clear();
			return 0;
		}


		// Loading the backup file succeeded, restore file
		bool res;
		{
			wxLogNull null;
			res = wxCopyFile(redirectedName + _T("~"), redirectedName);
		}
		if (!res) {
			// Could not restore backup, give up.
			Close();
			m_error = err;
			m_error += _T("\n") + wxString::Format(_("The valid backup file %s could not be restored"), redirectedName + _T("~"));
			m_modificationTime.clear();
			return 0;
		}

		// We no longer need the backup
		wxRemoveFile(redirectedName + _T("~"));
		m_error.clear();
	}

	m_modificationTime = CLocalFileSystem::GetModificationTime(redirectedName);
	return m_pElement;
}

bool CXmlFile::Modified()
{
	wxCHECK(!m_fileName.empty(), false);

	if (!m_modificationTime.IsValid())
		return true;

	CDateTime const modificationTime = CLocalFileSystem::GetModificationTime(m_fileName);
	if (modificationTime.IsValid() && modificationTime == m_modificationTime)
		return false;

	return true;
}

void CXmlFile::Close()
{
	delete m_pDocument;
	m_pDocument = 0;
	m_pElement = 0;
}

bool CXmlFile::Save(bool printError)
{
	m_error.clear();

	wxCHECK(!m_fileName.empty(), false);
	wxCHECK(m_pDocument, false);

	bool res = SaveXmlFile();
	m_modificationTime = CLocalFileSystem::GetModificationTime(m_fileName);

	if (!res && printError) {
		wxASSERT(!m_error.empty());

		wxString msg = wxString::Format(_("Could not write \"%s\":"), m_fileName);
		wxMessageBoxEx(msg + _T("\n") + m_error, _("Error writing xml file"), wxICON_ERROR);
	}
	return res;
}

TiXmlElement* CXmlFile::CreateEmpty()
{
	Close();

	m_pDocument = new TiXmlDocument();
	m_pDocument->SetCondenseWhiteSpace(false);
	m_pDocument->LinkEndChild(new TiXmlDeclaration("1.0", "UTF-8", "yes"));

	m_pElement = new TiXmlElement(m_rootName);
	m_pDocument->LinkEndChild(m_pElement);

	return m_pElement;
}

wxString ConvLocal(const char *value)
{
	return wxString(wxConvUTF8.cMB2WC(value), *wxConvCurrent);
}

void RemoveTextElement(TiXmlElement* node)
{
	for (TiXmlNode* pChild = node->FirstChild(); pChild; pChild = pChild->NextSibling()) {
		if (pChild->ToText()) {
			node->RemoveChild(pChild);
			break;
		}
	}
}

TiXmlElement* AddTextElement(TiXmlElement* node, const char* name, const wxString& value, bool overwrite)
{
	wxASSERT(node);

	wxScopedCharBuffer utf8 = value.utf8_str();
	if (!utf8)
		return 0;

	return AddTextElementRaw(node, name, utf8, overwrite);
}

void AddTextElement(TiXmlElement* node, const char* name, int value, bool overwrite)
{
	char buffer[sizeof(int) * 8]; // Always big enough
	sprintf(buffer, "%d", value);
	AddTextElementRaw(node, name, buffer, overwrite);
}

TiXmlElement* AddTextElementRaw(TiXmlElement* node, const char* name, const char* value, bool overwrite)
{
	wxASSERT(node);
	wxASSERT(value);

	TiXmlElement *element = 0;
	if (overwrite) {
		element = node->FirstChildElement(name);
		if (element)
			RemoveTextElement(element);
	}

	if (!element) {
		element = new TiXmlElement(name);
		node->LinkEndChild(element);
	}

	if (*value)
		element->LinkEndChild(new TiXmlText(value));

	return element;
}

void AddTextElement(TiXmlElement* node, const wxString& value)
{
	wxASSERT(node);
	wxASSERT(value);

	wxScopedCharBuffer utf8 = value.utf8_str();
	if (!utf8)
		return;

	AddTextElementRaw(node, utf8);
}

void AddTextElement(TiXmlElement* node, int value)
{
	char buffer[sizeof(int)]; // Always big enough
	sprintf(buffer, "%d", value);
	AddTextElementRaw(node, buffer);
}

void AddTextElementRaw(TiXmlElement* node, const char* value)
{
	wxASSERT(node);
	wxASSERT(value);

	RemoveTextElement(node);

	if (*value)
		node->LinkEndChild(new TiXmlText(value));
}

wxString GetTextElement_Trimmed(TiXmlElement* node, const char* name)
{
	wxString t = GetTextElement(node, name);
	t.Trim(true);
	t.Trim(false);

	return t;
}

wxString GetTextElement(TiXmlElement* node, const char* name)
{
	wxASSERT(node);

	TiXmlElement* element = node->FirstChildElement(name);
	if (!element)
		return wxString();

	TiXmlNode* textNode = element->FirstChild();
	if (!textNode || !textNode->ToText())
		return wxString();

	return ConvLocal(textNode->Value());
}

wxString GetTextElement_Trimmed(TiXmlElement* node)
{
	wxString t = GetTextElement(node);
	t.Trim(true);
	t.Trim(false);

	return t;
}

wxString GetTextElement(TiXmlElement* node)
{
	wxASSERT(node);

	for (TiXmlNode* pChild = node->FirstChild(); pChild; pChild = pChild->NextSibling()) {
		if (pChild->ToText()) {
			return ConvLocal(pChild->Value());
		}
	}

	return wxString();
}

int GetTextElementInt(TiXmlElement* node, const char* name, int defValue /*=0*/)
{
	wxASSERT(node);

	TiXmlElement* element = node->FirstChildElement(name);
	if (!element)
		return defValue;

	TiXmlNode* textNode = element->FirstChild();
	if (!textNode || !textNode->ToText())
		return defValue;

	const char* str = textNode->Value();
	const char* p = str;

	int value = 0;
	bool negative = false;
	if (*p == '-') {
		negative = true;
		p++;
	}
	while (*p) {
		if (*p < '0' || *p > '9')
			return defValue;

		value *= 10;
		value += *p - '0';

		p++;
	}

	return negative ? -value : value;
}

wxLongLong GetTextElementLongLong(TiXmlElement* node, const char* name, int defValue /*=0*/)
{
	wxASSERT(node);

	TiXmlElement* element = node->FirstChildElement(name);
	if (!element)
		return defValue;

	TiXmlNode* textNode = element->FirstChild();
	if (!textNode || !textNode->ToText())
		return defValue;

	const char* str = textNode->Value();
	const char* p = str;

	wxLongLong value = 0;
	bool negative = false;
	if (*p == '-') {
		negative = true;
		p++;
	}
	while (*p) {
		if (*p < '0' || *p > '9')
			return defValue;

		value *= 10;
		value += *p - '0';

		p++;
	}

	return negative ? -value : value;
}

bool GetTextElementBool(TiXmlElement* node, const char* name, bool defValue /*=false*/)
{
	wxASSERT(node);

	TiXmlElement* element = node->FirstChildElement(name);
	if (!element)
		return defValue;

	TiXmlNode* textNode = element->FirstChild();
	if (!textNode || !textNode->ToText())
		return defValue;

	const char* str = textNode->Value();
	if (!str)
		return defValue;

	switch (str[0])
	{
	case '0':
		return false;
	case '1':
		return true;
	default:
		return defValue;
	}
}

bool CXmlFile::LoadXmlDocument(wxString const& file)
{
	wxFFile f(file, _T("rb"));
	if (!f.IsOpened()) {
		const wxChar* s = wxSysErrorMsg();
		if (s && *s)
			m_error = s;
		else
			m_error = _("Unknown error opening the file. Make sure the file can be accessed and is a well-formed XML document.");
		return false;
	}

	m_pDocument = new TiXmlDocument;
	m_pDocument->SetCondenseWhiteSpace(false);
	if (!m_pDocument->LoadFile(f.fp()) && m_pDocument->ErrorId() != TiXmlBase::TIXML_ERROR_DOCUMENT_EMPTY) {
		const char* s = m_pDocument->ErrorDesc();
		if (s && *s) {
			m_error.Printf(_("The XML document is not well-formed: %s"), wxString(s, wxConvLibc));
		}
		else
			m_error = _("Unknown error opening the file. Make sure the file can be accessed and is a well-formed XML document.");
		delete m_pDocument;
		m_pDocument = 0;
		return false;
	}
	return true;
}

// Opens the specified XML file if it exists or creates a new one otherwise.
// Returns 0 on error.
bool CXmlFile::GetXmlFile(wxString const& file)
{
	Close();

	if (CLocalFileSystem::GetSize(file) <= 0) {
		return false;
	}

	// File exists, open it
	if (!LoadXmlDocument(file)) {
		return false;
	}

	m_pElement = m_pDocument->FirstChildElement(m_rootName);
	if (!m_pElement) {
		if (m_pDocument->FirstChildElement()) {
			// Not created by FileZilla3
			Close();
			m_error = _("Unknown root element, the file does not appear to be generated by FileZilla.");
			return false;
		}
		m_pElement = m_pDocument->LinkEndChild(new TiXmlElement(m_rootName))->ToElement();
	}

	return true;
}

wxString CXmlFile::GetRedirectedName() const
{
	wxString redirectedName = m_fileName;
	bool isLink = false;
	if (CLocalFileSystem::GetFileInfo(m_fileName, isLink, 0, 0, 0) == CLocalFileSystem::file) {
		if (isLink) {
			CLocalPath target(CLocalFileSystem::GetSymbolicLinkTarget(m_fileName));
			if (!target.empty()) {
				redirectedName = target.GetPath();
				redirectedName.RemoveLast();
			}
		}
	}
	return redirectedName;
}

bool CXmlFile::SaveXmlFile()
{
	bool exists = false;

	bool isLink = false;
	int flags = 0;

	wxString redirectedName = GetRedirectedName();
	if (CLocalFileSystem::GetFileInfo(redirectedName, isLink, 0, 0, &flags) == CLocalFileSystem::file) {
#ifdef __WXMSW__
		if (flags & FILE_ATTRIBUTE_HIDDEN)
			SetFileAttributes(redirectedName, flags & ~FILE_ATTRIBUTE_HIDDEN);
#endif

		exists = true;
		bool res;
		{
			wxLogNull null;
			res = wxCopyFile(redirectedName, redirectedName + _T("~"));
		}
		if (!res) {
			m_error = _("Failed to create backup copy of xml file");
			return false;
		}
	}

	bool success = false;
	{
		wxFFile f(redirectedName, _T("w"));
		success = f.IsOpened() && m_pDocument->SaveFile(f.fp());
	}

	if (!success) {
		wxRemoveFile(redirectedName);
		if (exists) {
			wxLogNull null;
			wxRenameFile(redirectedName + _T("~"), redirectedName);
		}
		m_error = _("Failed to write xml file");
		return false;
	}

	if (exists)
		wxRemoveFile(redirectedName + _T("~"));

	return true;
}

bool GetServer(TiXmlElement *node, CServer& server)
{
	wxASSERT(node);

	wxString host = GetTextElement(node, "Host");
	if (host.empty())
		return false;

	int port = GetTextElementInt(node, "Port");
	if (port < 1 || port > 65535)
		return false;

	if (!server.SetHost(host, port))
		return false;

	int const protocol = GetTextElementInt(node, "Protocol");
	if (protocol < 0 || protocol > ServerProtocol::MAX_VALUE) {
		return false;
	}
	server.SetProtocol(static_cast<ServerProtocol>(protocol));

	int type = GetTextElementInt(node, "Type");
	if (type < 0 || type >= SERVERTYPE_MAX)
		return false;

	server.SetType((enum ServerType)type);

	int logonType = GetTextElementInt(node, "Logontype");
	if (logonType < 0)
		return false;

	server.SetLogonType((enum LogonType)logonType);

	if (server.GetLogonType() != ANONYMOUS) {
		wxString user = GetTextElement(node, "User");

		wxString pass;
		if ((long)NORMAL == logonType || (long)ACCOUNT == logonType) {
			TiXmlElement* passElement = node->FirstChildElement("Pass");
			if (passElement) {
				pass = GetTextElement(passElement);

				wxString encoding = GetTextAttribute(passElement, "encoding");

				if (encoding == _T("base64")) {
					wxMemoryBuffer buf = wxBase64Decode(pass);
					if (!buf.IsEmpty()) {
						pass = wxString::FromUTF8(static_cast<const char*>(buf.GetData()), buf.GetDataLen());
					}
					else {
						pass.clear();
					}
				}
				else if (!encoding.empty()) {
					pass.clear();
					server.SetLogonType(ASK);
				}
			}
		}

		if (!server.SetUser(user, pass))
			return false;

		if ((long)ACCOUNT == logonType) {
			wxString account = GetTextElement(node, "Account");
			if (account.empty())
				return false;
			if (!server.SetAccount(account))
				return false;
		}
	}

	int timezoneOffset = GetTextElementInt(node, "TimezoneOffset");
	if (!server.SetTimezoneOffset(timezoneOffset))
		return false;

	wxString pasvMode = GetTextElement(node, "PasvMode");
	if (pasvMode == _T("MODE_PASSIVE"))
		server.SetPasvMode(MODE_PASSIVE);
	else if (pasvMode == _T("MODE_ACTIVE"))
		server.SetPasvMode(MODE_ACTIVE);
	else
		server.SetPasvMode(MODE_DEFAULT);

	int maximumMultipleConnections = GetTextElementInt(node, "MaximumMultipleConnections");
	server.MaximumMultipleConnections(maximumMultipleConnections);

	wxString encodingType = GetTextElement(node, "EncodingType");
	if (encodingType == _T("Auto"))
		server.SetEncodingType(ENCODING_AUTO);
	else if (encodingType == _T("UTF-8"))
		server.SetEncodingType(ENCODING_UTF8);
	else if (encodingType == _T("Custom")) {
		wxString customEncoding = GetTextElement(node, "CustomEncoding");
		if (customEncoding.empty())
			return false;
		if (!server.SetEncodingType(ENCODING_CUSTOM, customEncoding))
			return false;
	}
	else
		server.SetEncodingType(ENCODING_AUTO);

	if (CServer::SupportsPostLoginCommands(server.GetProtocol())) {
		std::vector<wxString> postLoginCommands;
		TiXmlElement* pElement = node->FirstChildElement("PostLoginCommands");
		if (pElement) {
			TiXmlElement* pCommandElement = pElement->FirstChildElement("Command");
			while (pCommandElement) {
				TiXmlNode* textNode = pCommandElement->FirstChild();
				if (textNode && textNode->ToText()) {
					wxString command = ConvLocal(textNode->Value());
					if (!command.empty())
						postLoginCommands.push_back(command);
				}

				pCommandElement = pCommandElement->NextSiblingElement("Command");
			}
		}
		if (!server.SetPostLoginCommands(postLoginCommands))
			return false;
	}

	server.SetBypassProxy(GetTextElementInt(node, "BypassProxy", false) == 1);
	server.SetName(GetTextElement_Trimmed(node, "Name"));

	if (server.GetName().empty())
		server.SetName(GetTextElement_Trimmed(node));

	return true;
}

void SetServer(TiXmlElement *node, const CServer& server)
{
	if (!node)
		return;

	bool kiosk_mode = COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) != 0;

	node->Clear();

	AddTextElement(node, "Host", server.GetHost());
	AddTextElement(node, "Port", server.GetPort());
	AddTextElement(node, "Protocol", server.GetProtocol());
	AddTextElement(node, "Type", server.GetType());

	enum LogonType logonType = server.GetLogonType();

	if (server.GetLogonType() != ANONYMOUS) {
		AddTextElement(node, "User", server.GetUser());

		if (server.GetLogonType() == NORMAL || server.GetLogonType() == ACCOUNT) {
			if (kiosk_mode)
				logonType = ASK;
			else {
				wxString pass = server.GetPass();
				auto const& buf = pass.utf8_str(); // wxWidgets has such an ugly string API....
				std::string utf8(buf.data(), buf.length());

				wxString base64 = wxBase64Encode(utf8.c_str(), utf8.size());
				TiXmlElement* passElement = AddTextElement(node, "Pass", base64);
				if (passElement) {
					SetTextAttribute(passElement, "encoding", _T("base64"));
				}

				if (server.GetLogonType() == ACCOUNT)
					AddTextElement(node, "Account", server.GetAccount());
			}
		}
	}
	AddTextElement(node, "Logontype", logonType);

	AddTextElement(node, "TimezoneOffset", server.GetTimezoneOffset());
	switch (server.GetPasvMode())
	{
	case MODE_PASSIVE:
		AddTextElementRaw(node, "PasvMode", "MODE_PASSIVE");
		break;
	case MODE_ACTIVE:
		AddTextElementRaw(node, "PasvMode", "MODE_ACTIVE");
		break;
	default:
		AddTextElementRaw(node, "PasvMode", "MODE_DEFAULT");
		break;
	}
	AddTextElement(node, "MaximumMultipleConnections", server.MaximumMultipleConnections());

	switch (server.GetEncodingType())
	{
	case ENCODING_AUTO:
		AddTextElementRaw(node, "EncodingType", "Auto");
		break;
	case ENCODING_UTF8:
		AddTextElementRaw(node, "EncodingType", "UTF-8");
		break;
	case ENCODING_CUSTOM:
		AddTextElementRaw(node, "EncodingType", "Custom");
		AddTextElement(node, "CustomEncoding", server.GetCustomEncoding());
		break;
	}

	if (CServer::SupportsPostLoginCommands(server.GetProtocol())) {
		std::vector<wxString> const& postLoginCommands = server.GetPostLoginCommands();
		if (!postLoginCommands.empty()) {
			TiXmlElement* pElement = node->LinkEndChild(new TiXmlElement("PostLoginCommands"))->ToElement();
			for (std::vector<wxString>::const_iterator iter = postLoginCommands.begin(); iter != postLoginCommands.end(); ++iter)
				AddTextElement(pElement, "Command", *iter);
		}
	}

	AddTextElementRaw(node, "BypassProxy", server.GetBypassProxy() ? "1" : "0");
	const wxString& name = server.GetName();
	if (!name.empty())
		AddTextElement(node, "Name", name);
}

void SetTextAttribute(TiXmlElement* node, const char* name, const wxString& value)
{
	wxASSERT(node);

	wxScopedCharBuffer utf8 = value.utf8_str();
	if (!utf8)
		return;

	node->SetAttribute(name, utf8);
}

wxString GetTextAttribute(TiXmlElement* node, const char* name)
{
	wxASSERT(node);

	const char* value = node->Attribute(name);
	if (!value)
		return wxString();

	return ConvLocal(value);
}

TiXmlElement* FindElementWithAttribute(TiXmlElement* node, const char* element, const char* attribute, const char* value)
{
	TiXmlElement* child;
	if (element)
		child = node->FirstChildElement(element);
	else
		child = node->FirstChildElement();

	while (child)
	{
		const char* nodeVal = child->Attribute(attribute);
		if (nodeVal && !strcmp(value, nodeVal))
			return child;

		if (element)
			child = child->NextSiblingElement(element);
		else
			child = child->NextSiblingElement();
	}

	return 0;
}

TiXmlElement* FindElementWithAttribute(TiXmlElement* node, const char* element, const char* attribute, int value)
{
	TiXmlElement* child;
	if (element)
		child = node->FirstChildElement(element);
	else
		child = node->FirstChildElement();

	while (child)
	{
		int nodeValue;
		const char* nodeVal = child->Attribute(attribute, &nodeValue);
		if (nodeVal && nodeValue == value)
			return child;

		if (element)
			child = child->NextSiblingElement(element);
		else
			child = child->NextSiblingElement();
	}

	return 0;
}

int GetAttributeInt(TiXmlElement* node, const char* name)
{
	int value;
	if (!node->Attribute(name, &value))
		return 0;

	return value;
}

void SetAttributeInt(TiXmlElement* node, const char* name, int value)
{
	node->SetAttribute(name, value);
}

int CXmlFile::GetRawDataLength()
{
	if (!m_pDocument)
		return 0;

	delete m_pPrinter;
	m_pPrinter = new TiXmlPrinter;
	m_pPrinter->SetStreamPrinting();

	m_pDocument->Accept(m_pPrinter);
	return m_pPrinter->Size();
}

void CXmlFile::GetRawDataHere(char* p) // p has to big enough to hold at least GetRawDataLength() bytes
{
	if (!m_pPrinter)
	{
		wxFAIL;
		return;
	}

	memcpy(p, m_pPrinter->CStr(), m_pPrinter->Size());
}

bool CXmlFile::ParseData(char* data)
{
	Close();
	m_pDocument = new TiXmlDocument;
	m_pDocument->SetCondenseWhiteSpace(false);
	m_pDocument->Parse(data);

	m_pElement = m_pDocument->FirstChildElement(m_rootName);
	if (!m_pElement) {
		Close();
		return false;
	}

	return true;
}
