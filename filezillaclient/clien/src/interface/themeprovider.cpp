#include <filezilla.h>
#include "themeprovider.h"
#include "filezillaapp.h"
#include "Options.h"
#include "xmlfunctions.h"

#include <wx/animate.h>

#include <utility>

static CThemeProvider* instance = 0;

CThemeProvider::CThemeProvider()
{
	wxArtProvider::Push(this);

	m_themePath = GetThemePath();

	RegisterOption(OPTION_THEME);

	if( !instance )
		instance = this;
}

CThemeProvider::~CThemeProvider()
{
	if( instance == this ) {
		instance = 0;
	}
}

CThemeProvider* CThemeProvider::Get()
{
	return instance;
}

static wxString SubdirFromSize(const int size)
{
	return wxString::Format(_T("%dx%d/"), size, size);
}

std::list<wxString> CThemeProvider::GetSearchDirs(const wxSize& bestSize)
{
	// Sort order:
	// - Current theme before general resource dir
	// - Try current size first
	// - Then try scale down next larger icon
	// - Then try scaling up next smaller icon


	int sizes[] = { 48,32,24,20,16 };

	std::list<wxString> sizeStrings;

	for (auto const& size : sizes) {
		if (size < bestSize.GetWidth())
			sizeStrings.push_back(SubdirFromSize(size));
		else if (size > bestSize.GetWidth())
			sizeStrings.push_front(SubdirFromSize(size));
	}
	sizeStrings.push_front(SubdirFromSize(bestSize.GetWidth()));

	std::list<wxString> dirs;
	for (auto const& size : sizeStrings) {
		dirs.push_back(m_themePath + size);
	}

	CLocalPath const resourceDir(wxGetApp().GetResourceDir());
	for (auto const& size : sizeStrings) {
		dirs.push_back(resourceDir.GetPath() + size);
	}

	return dirs;
}

wxBitmap CThemeProvider::CreateBitmap(const wxArtID& id, const wxArtClient& /*client*/, const wxSize& size)
{
	if (id.Left(4) != _T("ART_"))
		return wxNullBitmap;
	wxASSERT(size.GetWidth() == size.GetHeight());

	std::list<wxString> dirs = GetSearchDirs(size);

	wxString name = id.Mid(4);

	// The ART_* IDs are always given in uppercase ASCII,
	// all filenames used by FileZilla for the resources
	// are lowercase ASCII. Locale-independent transformation
	// needed e.g. if using Turkish locale.
	MakeLowerAscii(name);

	wxLogNull logNull;

	for (auto const& dir : dirs) {
		wxString fileName = dir + name + _T(".png");

		// MSW toolbar only greys out disabled buttons in a visually
		// pleasing way if the bitmap has an alpha channel.
		wxImage img(fileName, wxBITMAP_TYPE_PNG);
		if (!img.Ok())
			continue;

		if (img.HasMask() && !img.HasAlpha())
			img.InitAlpha();
		if (size.IsFullySpecified())
			img.Rescale(size.x, size.y, wxIMAGE_QUALITY_HIGH);
		return wxBitmap(img);
	}

	return wxNullBitmap;
}

wxAnimation CThemeProvider::CreateAnimation(const wxArtID& id, const wxSize& size)
{
	if (id.Left(4) != _T("ART_"))
		return wxAnimation();
	wxASSERT(size.GetWidth() == size.GetHeight());

	std::list<wxString> dirs = GetSearchDirs(size);

	wxString name = id.Mid(4);

	// The ART_* IDs are always given in uppercase ASCII,
	// all filenames used by FileZilla for the resources
	// are lowercase ASCII. Locale-independent transformation
	// needed e.g. if using Turkish locale.
	MakeLowerAscii(name);

	wxLogNull logNull;

	for (auto const& dir : dirs) {
		wxString fileName = dir + name + _T(".gif");

		wxAnimation a(fileName);
		if( a.IsOk() ) {
			return a;
		}
	}

	return wxAnimation();
}

std::vector<wxString> CThemeProvider::GetThemes()
{
	std::vector<wxString> themes;

	CLocalPath const resourceDir = wxGetApp().GetResourceDir();
	if (wxFileName::FileExists(resourceDir.GetPath() + _T("theme.xml")))
		themes.push_back(wxString());

	wxDir dir(resourceDir.GetPath());
	bool found;
	wxString subdir;
	for (found = dir.GetFirst(&subdir, _T("*"), wxDIR_DIRS); found; found = dir.GetNext(&subdir)) {
		if (wxFileName::FileExists(resourceDir.GetPath() + subdir + _T("/") + _T("theme.xml")))
			themes.push_back(subdir + _T("/"));
	}

	return themes;
}

std::vector<std::unique_ptr<wxBitmap>> CThemeProvider::GetAllImages(const wxString& theme, const wxSize& size)
{
	wxString path = wxGetApp().GetResourceDir().GetPath() + theme + _T("/");

	wxLogNull log;

	wxString strSize = wxString::Format(_T("%dx%d/"), size.GetWidth(), size.GetHeight());
	if (wxDir::Exists(path + strSize)) {
		path += strSize;
	}
	else {
		if (size.GetWidth() > 32)
			path += _T("48x48/");
		else if (size.GetWidth() > 16)
			path += _T("32x32/");
		else
			path += _T("16x16/");
	}

	std::vector<std::unique_ptr<wxBitmap>> bitmaps;

	if (!wxDir::Exists(path))
		return bitmaps;

	wxDir dir(path);
	if (!dir.IsOpened())
		return bitmaps;

	wxString file;
	for (bool found = dir.GetFirst(&file, _T("*.png")); found; found = dir.GetNext(&file)) {
		if (file.Right(13) == _T("_disabled.png"))
			continue;

		wxFileName fn(path, file);
		std::unique_ptr<wxBitmap> bmp(new wxBitmap);
		if (bmp->LoadFile(fn.GetFullPath(), wxBITMAP_TYPE_PNG)) {
			bitmaps.push_back(std::move(bmp));
		}
	}
	return bitmaps;
}

bool CThemeProvider::GetThemeData(const wxString& themePath, wxString& name, wxString& author, wxString& email)
{
	wxString const file(wxGetApp().GetResourceDir().GetPath() + themePath + _T("theme.xml"));
	CXmlFile xml(file);
	TiXmlElement* root = xml.Load();
	TiXmlElement* theme = root ? root->FirstChildElement("Theme") : 0;
	if (!theme)
		return false;

	name = GetTextElement(theme, "Name");
	author = GetTextElement(theme, "Author");
	email = GetTextElement(theme, "Mail");
	return true;
}

std::vector<wxString> CThemeProvider::GetThemeSizes(const wxString& themePath)
{
	std::vector<wxString> sizes;

	wxString const file(wxGetApp().GetResourceDir().GetPath() + themePath + _T("theme.xml"));
	CXmlFile xml(file);
	TiXmlElement* root = xml.Load();
	TiXmlElement* theme = root ? root->FirstChildElement("Theme") : 0;
	if (!theme)
		return sizes;

	for (TiXmlElement* pSize = theme->FirstChildElement("size"); pSize; pSize = pSize->NextSiblingElement("size")) {
		const char* txt = pSize->GetText();
		if (!txt)
			continue;

		wxString size = ConvLocal(txt);
		if (size.empty())
			continue;

		sizes.push_back(size);
	}

	return sizes;
}

wxIconBundle CThemeProvider::GetIconBundle(const wxArtID& id, const wxArtClient& client /*=wxART_OTHER*/)
{
	wxIconBundle iconBundle;

	if (id.Left(4) != _T("ART_"))
		return iconBundle;

	wxString name = id.Mid(4);
	MakeLowerAscii(name);

	const wxChar* dirs[] = { _T("16x16/"), _T("32x32/"), _T("48x48/") };

	CLocalPath const resourcePath = wxGetApp().GetResourceDir();

	for (auto const& dir : dirs ) {
		wxString file = resourcePath.GetPath() + dir + name + _T(".png");
		if (!wxFileName::FileExists(file))
			continue;

		iconBundle.AddIcon(wxIcon(file, wxBITMAP_TYPE_PNG));
	}

	return iconBundle;
}

bool CThemeProvider::ThemeHasSize(const wxString& themePath, const wxString& size)
{
	wxString const file(wxGetApp().GetResourceDir().GetPath() + themePath + _T("theme.xml"));
	CXmlFile xml(file);
	TiXmlElement* root = xml.Load();
	TiXmlElement* theme = root ? root->FirstChildElement("Theme") : 0;
	if (!theme) {
		return false;
	}

	for (TiXmlElement* pSize = theme->FirstChildElement("size"); pSize; pSize = pSize->NextSiblingElement("size")) {
		const char* txt = pSize->GetText();
		if (!txt)
			continue;

		if (size == ConvLocal(txt)) {
			return true;
		}
	}

	return false;
}

wxString CThemeProvider::GetThemePath()
{
	CLocalPath const resourceDir = wxGetApp().GetResourceDir();
	wxString themePath = resourceDir.GetPath() + COptions::Get()->GetOption(OPTION_THEME);
	if (wxFile::Exists(themePath + _T("theme.xml")))
		return themePath;

	themePath = resourceDir.GetPath() + _T("opencrystal/");
	if (wxFile::Exists(themePath + _T("theme.xml")))
		return themePath;

	wxASSERT(wxFile::Exists(resourceDir.GetPath() + _T("theme.xml")));
	return resourceDir.GetPath();
}

void CThemeProvider::OnOptionsChanged(changed_options_t const& options)
{
	m_themePath = GetThemePath();

	wxArtProvider::Remove(this);
	wxArtProvider::Push(this);
}

wxSize CThemeProvider::GetIconSize(enum iconSize size)
{
	int s;
	if (size == iconSizeSmall) {
		s = wxSystemSettings::GetMetric(wxSYS_SMALLICON_X);
		if (s <= 0)
			s = 16;
	}
	else if (size == iconSize24) {
		s = wxSystemSettings::GetMetric(wxSYS_SMALLICON_X);
		if (s <= 0) {
			s = 24;
		}
		else {
			s += s/2;
		}
	}
	else if (size == iconSizeLarge) {
		s = wxSystemSettings::GetMetric(wxSYS_ICON_X);
		if (s <= 0)
			s = 48;
		else
			s += s/2;
	}
	else {
		s = wxSystemSettings::GetMetric(wxSYS_ICON_X);
		if (s <= 0)
			s = 32;
	}

	return wxSize(s, s);
}

wxSize CThemeProvider::GetIconSize(wxString const& str)
{
	wxSize iconSize;
	if (str == _T("24x24"))
		return CThemeProvider::GetIconSize(iconSize24);
	else if (str == _T("32x32"))
		iconSize = CThemeProvider::GetIconSize(iconSizeNormal);
	else if (str == _T("48x48"))
		iconSize = CThemeProvider::GetIconSize(iconSizeLarge);
	else
		iconSize = CThemeProvider::GetIconSize(iconSizeSmall);

	return iconSize;
}
