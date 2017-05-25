#ifndef __XML_UTILS_H__
#define __XML_UTILS_H__

class TiXmlDocument;
class TiXmlElement;
class TiXmlNode;

namespace XML
{

CStdString ReadText(TiXmlElement* pElement);
void SetText(TiXmlElement* pElement, const CStdString& text);

bool Load(TiXmlDocument & document, CStdString const& file);
bool Save(TiXmlNode & element, CStdString const& file);

}

#endif //__XML_UTIL	S_H__