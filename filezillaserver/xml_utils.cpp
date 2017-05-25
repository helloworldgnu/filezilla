#include "StdAfx.h"
#include "xml_utils.h"
#include "conversion.h"
#include "tinyxml/tinyxml.h"

namespace XML
{

CStdString ReadText(TiXmlElement* pElement)
{
	TiXmlNode* textNode = pElement->FirstChild();
	if (!textNode || !textNode->ToText())
		return _T("");

	return ConvFromNetwork(textNode->Value());
}

void SetText(TiXmlElement* pElement, const CStdString& text)
{
	pElement->Clear();
	pElement->LinkEndChild(new TiXmlText(ConvToNetwork(text).c_str()));
}


bool Load(TiXmlDocument & document, CStdString const& file)
{
	bool ret = false;

	FILE* f = _wfopen(file, L"rb");
	if (f) {
		ret = document.LoadFile(f);

		fclose(f);
	}

	return ret;
}

bool Save(TiXmlNode & node, CStdString const& file)
{
	bool ret = false;

	auto * document = node.GetDocument();
	if (document) {
		FILE* f = _wfopen(file, L"wb");
		if (f) {
			ret = document->SaveFile(f);
		}

		fclose(f);
	}

	return ret;
}

}