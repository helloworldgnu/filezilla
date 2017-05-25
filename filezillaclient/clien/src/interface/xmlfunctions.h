/*
 * xmlfunctions.h declares some useful xml helper functions, especially to
 * improve usability together with wxWidgets.
 */

#ifndef __XMLFUNCTIONS_H__
#define __XMLFUNCTIONS_H__

#ifdef HAVE_LIBTINYXML
#include <tinyxml.h>
#else
#include "../tinyxml/tinyxml.h"
#endif

class CXmlFile
{
public:
	CXmlFile() {};
	explicit CXmlFile(const wxString& fileName, wxString const& root = wxString());

	virtual ~CXmlFile();

	TiXmlElement* CreateEmpty();

	wxString GetFileName() const { return m_fileName; }
	void SetFileName(const wxString& name);

	bool HasFileName() const { return !m_fileName.empty(); }

	// Sets error description on failure
	TiXmlElement* Load();

	wxString GetError() const { return m_error; }
	int GetRawDataLength();
	void GetRawDataHere(char* p); // p has to big enough to hold at least GetRawDataLength() bytes

	bool ParseData(char* data); // data has to be 0-terminated

	void Close();

	TiXmlElement* GetElement() { return m_pElement; }
	TiXmlElement const* GetElement() const { return m_pElement; }

	bool Modified();

	bool Save(bool printError);
protected:
	wxString GetRedirectedName() const;

	// Opens the specified XML file if it exists or creates a new one otherwise.
	// Returns 0 on error.
	bool GetXmlFile(wxString const& file);

	bool LoadXmlDocument(wxString const& file);

	// Save the XML document to the given file
	bool SaveXmlFile();

	CDateTime m_modificationTime;
	wxString m_fileName;
	TiXmlDocument* m_pDocument{};
	TiXmlElement* m_pElement{};
	TiXmlPrinter *m_pPrinter{};

	wxString m_error;

	wxString m_rootName{_T("FileZilla3")};
};

// Convert the given utf-8 string into wxString
wxString ConvLocal(const char *value);

void SetTextAttribute(TiXmlElement* node, const char* name, const wxString& value);
wxString GetTextAttribute(TiXmlElement* node, const char* name);

int GetAttributeInt(TiXmlElement* node, const char* name);
void SetAttributeInt(TiXmlElement* node, const char* name, int value);

TiXmlElement* FindElementWithAttribute(TiXmlElement* node, const char* element, const char* attribute, const char* value);
TiXmlElement* FindElementWithAttribute(TiXmlElement* node, const char* element, const char* attribute, int value);

// Add a new child element with the specified name and value to the xml document
TiXmlElement* AddTextElement(TiXmlElement* node, const char* name, const wxString& value, bool overwrite = false);
void AddTextElement(TiXmlElement* node, const char* name, int value, bool overwrite = false);
TiXmlElement* AddTextElementRaw(TiXmlElement* node, const char* name, const char* value, bool overwrite = false);

// Set the current element's text value
void AddTextElement(TiXmlElement* node, const wxString& value);
void AddTextElement(TiXmlElement* node, int value);
void AddTextElementRaw(TiXmlElement* node, const char* value);

// Get string from named child element
wxString GetTextElement(TiXmlElement* node, const char* name);
wxString GetTextElement_Trimmed(TiXmlElement* node, const char* name);

// Get string from current element
wxString GetTextElement(TiXmlElement* node);
wxString GetTextElement_Trimmed(TiXmlElement* node);

// Get (64-bit) integer from named element
int GetTextElementInt(TiXmlElement* node, const char* name, int defValue = 0);
wxLongLong GetTextElementLongLong(TiXmlElement* node, const char* name, int defValue = 0);

bool GetTextElementBool(TiXmlElement* node, const char* name, bool defValue = false);

// Functions to save and retrieve CServer objects to the XML file
void SetServer(TiXmlElement *node, const CServer& server);
bool GetServer(TiXmlElement *node, CServer& server);

#endif //__XMLFUNCTIONS_H__
