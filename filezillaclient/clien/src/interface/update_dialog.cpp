#include <filezilla.h>

#if FZ_MANUALUPDATECHECK

#include "buildinfo.h"
#include "filezillaapp.h"
#include "file_utils.h"
#include "update_dialog.h"
#include "themeprovider.h"

#include <wx/animate.h>
#include <wx/hyperlink.h>

BEGIN_EVENT_TABLE(CUpdateDialog, wxDialogEx)
EVT_BUTTON(XRCID("ID_INSTALL"), CUpdateDialog::OnInstall)
EVT_TIMER(wxID_ANY, CUpdateDialog::OnTimer)
EVT_HYPERLINK(XRCID("ID_SHOW_DETAILS"), CUpdateDialog::ShowDetails)
EVT_HYPERLINK(XRCID("ID_RETRY"), CUpdateDialog::Retry)
EVT_HYPERLINK(XRCID("ID_DOWNLOAD_RETRY"), CUpdateDialog::Retry)
EVT_BUTTON(XRCID("ID_DEBUGLOG"), CUpdateDialog::OnDebugLog)
END_EVENT_TABLE()

namespace pagenames {
enum type {
	checking,
	failed,
	newversion,
	latest
};
}

static int refcount = 0;

CUpdateDialog::CUpdateDialog(wxWindow* parent, CUpdater& updater)
	: parent_(parent)
	, updater_(updater)
{
	timer_.SetOwner(this);
	++refcount;
}

CUpdateDialog::~CUpdateDialog()
{
	--refcount;
}

bool CUpdateDialog::IsRunning()
{
	return refcount != 0;
}

int CUpdateDialog::ShowModal()
{
	wxString version(PACKAGE_VERSION, wxConvLocal);
	if (version.empty() || version[0] < '0' || version[0] > '9')
	{
		wxMessageBoxEx(_("Executable contains no version info, cannot check for updates."), _("Check for updates failed"), wxICON_ERROR, parent_);
		return wxID_CANCEL;
	}

	if (!Load(parent_, _T("ID_UPDATE_DIALOG")))
		return wxID_CANCEL;

	LoadPanel(_T("ID_CHECKING_PANEL"));
	LoadPanel(_T("ID_FAILURE_PANEL"));
	LoadPanel(_T("ID_NEWVERSION_PANEL"));
	LoadPanel(_T("ID_LATEST_PANEL"));
	if( panels_.size() != 4 ) {
		return wxID_CANCEL;
	}

	wxAnimation a = CThemeProvider::Get()->CreateAnimation(_T("ART_THROBBER"), wxSize(16,16));
	XRCCTRL(*this, "ID_WAIT_CHECK", wxAnimationCtrl)->SetMinSize(a.GetSize());
	XRCCTRL(*this, "ID_WAIT_CHECK", wxAnimationCtrl)->SetAnimation(a);
	XRCCTRL(*this, "ID_WAIT_CHECK", wxAnimationCtrl)->Play();
	XRCCTRL(*this, "ID_WAIT_DOWNLOAD", wxAnimationCtrl)->SetMinSize(a.GetSize());
	XRCCTRL(*this, "ID_WAIT_DOWNLOAD", wxAnimationCtrl)->SetAnimation(a);
	XRCCTRL(*this, "ID_WAIT_DOWNLOAD", wxAnimationCtrl)->Play();

	Wrap();

	XRCCTRL(*this, "ID_DETAILS", wxTextCtrl)->Hide();

	UpdaterState s = updater_.GetState();
	UpdaterStateChanged( s, updater_.AvailableBuild() );

	updater_.AddHandler(*this);

	updater_.RunIfNeeded();

	int ret = wxDialogEx::ShowModal();
	updater_.RemoveHandler(*this);

	return ret;
}

void CUpdateDialog::Wrap()
{
	wxPanel* parentPanel = XRCCTRL(*this, "ID_CONTENT", wxPanel);
	wxSize canvas;
	canvas.x = GetSize().x - parentPanel->GetSize().x;
	canvas.y = GetSize().y - parentPanel->GetSize().y;

	// Wrap pages nicely
	std::vector<wxWindow*> pages;
	for (auto const& panel : panels_) {
		pages.push_back(panel);
	}
	wxGetApp().GetWrapEngine()->WrapRecursive(pages, 1.33, "Update", canvas);

	// Keep track of maximum page size
	wxSize size = GetSizer()->GetMinSize();
	for (auto const& panel : panels_) {
		size.IncTo(panel->GetSizer()->GetMinSize());
	}

	wxSize panelSize = size;
#ifdef __WXGTK__
	panelSize.x += 1;
#endif
	parentPanel->SetInitialSize(panelSize);

	// Adjust pages sizes according to maximum size
	for (auto const& panel : panels_) {
		panel->GetSizer()->SetMinSize(size);
		panel->GetSizer()->Fit(panel);
		panel->GetSizer()->SetSizeHints(panel);
		if (GetLayoutDirection() == wxLayout_RightToLeft) {
			panel->Move(wxPoint(0, 0));
		}
	}

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);

#ifdef __WXGTK__
	// Pre-show dialog under GTK, else panels won't get initialized properly
	Show();
#endif

	for (auto const& panel : panels_) {
		panel->Hide();
	}
	panels_[0]->Show();
}

void CUpdateDialog::LoadPanel(wxString const& name)
{
	wxPanel* p = new wxPanel();
	if (!wxXmlResource::Get()->LoadPanel(p, XRCCTRL(*this, "ID_CONTENT", wxPanel), name)) {
		delete p;
		return;
	}

	panels_.push_back(p);
}


void CUpdateDialog::UpdaterStateChanged( UpdaterState s, build const& v )
{
	timer_.Stop();
	for (auto const& panel : panels_) {
		panel->Hide();
	}
	if( s == UpdaterState::idle ) {
		panels_[pagenames::latest]->Show();
	}
	else if( s == UpdaterState::failed ) {
		XRCCTRL(*this, "ID_DETAILS", wxTextCtrl)->ChangeValue(updater_.GetLog());
		panels_[pagenames::failed]->Show();
	}
	else if( s == UpdaterState::checking ) {
		panels_[pagenames::checking]->Show();
	}
	else if( s == UpdaterState::newversion || s == UpdaterState::newversion_ready || s == UpdaterState::newversion_downloading ) {
		XRCCTRL(*this, "ID_VERSION", wxStaticText)->SetLabel(v.version_);

		wxString news = updater_.GetChangelog();

		auto pos = news.find(v.version_ + _T(" (2"));
		if (pos != wxString::npos) {
			news = news.Mid(pos);
		}

		XRCCTRL(*this, "ID_NEWS_LABEL", wxStaticText)->Show(!news.empty());
		XRCCTRL(*this, "ID_NEWS", wxTextCtrl)->Show(!news.empty());
		if( news != XRCCTRL(*this, "ID_NEWS", wxTextCtrl)->GetValue() ) {
			XRCCTRL(*this, "ID_NEWS", wxTextCtrl)->ChangeValue(news);
		}
		bool downloading = s == UpdaterState::newversion_downloading;
		XRCCTRL(*this, "ID_DOWNLOAD_LABEL", wxStaticText)->Show(downloading);
		XRCCTRL(*this, "ID_WAIT_DOWNLOAD", wxAnimationCtrl)->Show(downloading);
		XRCCTRL(*this, "ID_DOWNLOAD_PROGRESS", wxStaticText)->Show(downloading);
		if( downloading ) {
			timer_.Start(500);
			wxTimerEvent ev;
			OnTimer(ev);
		}

		bool ready = s == UpdaterState::newversion_ready;
		XRCCTRL(*this, "ID_DOWNLOADED", wxStaticText)->Show(ready);
		XRCCTRL(*this, "ID_INSTALL", wxButton)->Show(ready);

		bool manual = s == UpdaterState::newversion;
		bool dlfail = s == UpdaterState::newversion && !v.url_.empty();
		XRCCTRL(*this, "ID_DOWNLOAD_FAIL", wxStaticText)->Show(dlfail);
		XRCCTRL(*this, "ID_DOWNLOAD_RETRY", wxHyperlinkCtrl)->Show(dlfail);

		XRCCTRL(*this, "ID_NEWVERSION_WEBSITE_TEXT", wxStaticText)->Show(manual && !dlfail);
		XRCCTRL(*this, "ID_NEWVERSION_WEBSITE_TEXT_DLFAIL", wxStaticText)->Show(manual && dlfail);
		XRCCTRL(*this, "ID_NEWVERSION_WEBSITE_LINK", wxHyperlinkCtrl)->Show(manual);

		panels_[pagenames::newversion]->Show();
		panels_[pagenames::newversion]->Layout();
	}
}

void CUpdateDialog::OnInstall(wxCommandEvent&)
{
	wxString f = updater_.DownloadedFile();
	if( f.empty() ) {
		return;
	}
#ifdef __WXMSW__
	wxExecute(_T("\"") + f +  _T("\" /update"));
	wxWindow* p = parent_;
	while( p->GetParent() ) {
		p = p->GetParent();
	}
	p->Close();
#else
	bool program_exists = false;
	wxString cmd = GetSystemOpenCommand(f, program_exists);
	if( program_exists && !cmd.empty() ) {
		if (wxExecute(cmd))
			return;
	}

	wxFileName fn(f);
	OpenInFileManager(fn.GetPath());
#endif
}

void CUpdateDialog::OnTimer(wxTimerEvent&)
{
	wxULongLong size = updater_.AvailableBuild().size_;
	wxULongLong downloaded = updater_.BytesDownloaded();

	unsigned int percent = 0;
	if( size > 0 ) {
		percent = ((downloaded * 100) / size).GetLo();
	}

	XRCCTRL(*this, "ID_DOWNLOAD_PROGRESS", wxStaticText)->SetLabel(wxString::Format(_("(%u%% downloaded)"), percent));
}

void CUpdateDialog::ShowDetails(wxHyperlinkEvent&)
{
	XRCCTRL(*this, "ID_SHOW_DETAILS", wxHyperlinkCtrl)->Hide();
	XRCCTRL(*this, "ID_DETAILS", wxTextCtrl)->Show();

	panels_[pagenames::failed]->Layout();
}

void CUpdateDialog::Retry(wxHyperlinkEvent&)
{
	updater_.RunIfNeeded();
}

void CUpdateDialog::OnDebugLog(wxCommandEvent&)
{
	wxMessageBox(updater_.GetLog());
}

#endif
