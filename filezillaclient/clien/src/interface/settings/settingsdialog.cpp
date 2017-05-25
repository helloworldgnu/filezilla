#include <filezilla.h>
#include "settingsdialog.h"
#include "../Options.h"
#include "optionspage.h"
#include "optionspage_connection.h"
#include "optionspage_connection_ftp.h"
#include "optionspage_connection_active.h"
#include "optionspage_connection_passive.h"
#include "optionspage_ftpproxy.h"
#include "optionspage_connection_sftp.h"
#include "optionspage_filetype.h"
#include "optionspage_fileexists.h"
#include "optionspage_themes.h"
#include "optionspage_language.h"
#include "optionspage_transfer.h"
#include "optionspage_updatecheck.h"
#include "optionspage_logging.h"
#include "optionspage_debug.h"
#include "optionspage_interface.h"
#include "optionspage_dateformatting.h"
#include "optionspage_sizeformatting.h"
#include "optionspage_edit.h"
#include "optionspage_edit_associations.h"
#include "optionspage_proxy.h"
#include "optionspage_filelists.h"
#include "../filezillaapp.h"
#include "../Mainfrm.h"

BEGIN_EVENT_TABLE(CSettingsDialog, wxDialogEx)
EVT_TREE_SEL_CHANGING(XRCID("ID_TREE"), CSettingsDialog::OnPageChanging)
EVT_TREE_SEL_CHANGED(XRCID("ID_TREE"), CSettingsDialog::OnPageChanged)
EVT_BUTTON(XRCID("wxID_OK"), CSettingsDialog::OnOK)
EVT_BUTTON(XRCID("wxID_CANCEL"), CSettingsDialog::OnCancel)
END_EVENT_TABLE()

CSettingsDialog::CSettingsDialog(CFileZillaEngineContext & engine_context)
	: m_engine_context(engine_context)
{
	m_pOptions = COptions::Get();
}

CSettingsDialog::~CSettingsDialog()
{
}

bool CSettingsDialog::Create(CMainFrame* pMainFrame)
{
	m_pMainFrame = pMainFrame;

	SetExtraStyle(wxWS_EX_BLOCK_EVENTS);
	SetParent(pMainFrame);
	if (!Load(GetParent(), _T("ID_SETTINGS")))
		return false;

	if (!LoadPages())
		return false;

	return true;
}

void CSettingsDialog::AddPage(wxString const& name, COptionsPage* page, int nest)
{
	wxTreeCtrl* treeCtrl = XRCCTRL(*this, "ID_TREE", wxTreeCtrl);
	wxTreeItemId parent = treeCtrl->GetRootItem();
	while( nest-- ) {
		parent = treeCtrl->GetLastChild(parent);
		wxCHECK_RET( parent != wxTreeItemId(), "Nesting level too deep" );
	}

	t_page p;
	p.page = page;
	p.id = treeCtrl->AppendItem(parent, name);
	if( parent != treeCtrl->GetRootItem() ) {
		treeCtrl->Expand(parent);
	}

	m_pages.push_back(p);
}

bool CSettingsDialog::LoadPages()
{
	// Get the tree control.

	wxTreeCtrl* treeCtrl = XRCCTRL(*this, "ID_TREE", wxTreeCtrl);
	wxASSERT(treeCtrl);
	if (!treeCtrl)
		return false;

	treeCtrl->AddRoot(wxString());

	// Create the instances of the page classes and fill the tree.
	AddPage(_("Connection"), new COptionsPageConnection, 0);
	AddPage(_("FTP"), new COptionsPageConnectionFTP, 1);
	AddPage(_("Active mode"), new COptionsPageConnectionActive, 2);
	AddPage(_("Passive mode"), new COptionsPageConnectionPassive, 2);
	AddPage(_("FTP Proxy"), new COptionsPageFtpProxy, 2);
	AddPage(_("SFTP"), new COptionsPageConnectionSFTP, 1);
	AddPage(_("Generic proxy"), new COptionsPageProxy, 1);
	AddPage(_("Transfers"), new COptionsPageTransfer, 0);
	AddPage(_("File Types"), new COptionsPageFiletype, 1);
	AddPage(_("File exists action"), new COptionsPageFileExists, 1);
	AddPage(_("Interface"), new COptionsPageInterface, 0);
	AddPage(_("Themes"), new COptionsPageThemes, 1);
	AddPage(_("Date/time format"), new COptionsPageDateFormatting, 1);
	AddPage(_("Filesize format"), new COptionsPageSizeFormatting, 1);
	AddPage(_("File lists"), new COptionsPageFilelists, 1);
	AddPage(_("Language"), new COptionsPageLanguage, 0);
	AddPage(_("File editing"), new COptionsPageEdit, 0);
	AddPage(_("Filetype associations"), new COptionsPageEditAssociations, 1);
#if FZ_MANUALUPDATECHECK && FZ_AUTOUPDATECHECK
	if (!COptions::Get()->GetOptionVal(OPTION_DEFAULT_DISABLEUPDATECHECK)) {
		AddPage(_("Updates"), new COptionsPageUpdateCheck, 0);
	}
#endif //FZ_MANUALUPDATECHECK && FZ_AUTOUPDATECHECK
	AddPage(_("Logging"), new COptionsPageLogging, 0);
	AddPage(_("Debug"), new COptionsPageDebug, 0);

	treeCtrl->SetQuickBestSize(false);
	treeCtrl->InvalidateBestSize();
	treeCtrl->SetInitialSize();

	// Compensate for scrollbar
	wxSize size = treeCtrl->GetBestSize();
	int scrollWidth = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, treeCtrl);
	size.x += scrollWidth;
	size.y = 0;
	treeCtrl->SetInitialSize(size);
	Layout();

	// Before we can initialize the pages, get the target panel in the settings
	// dialog.
	wxPanel* parentPanel = XRCCTRL(*this, "ID_PAGEPANEL", wxPanel);
	wxASSERT(parentPanel);
	if (!parentPanel)
		return false;

	// Keep track of maximum page size
	size = wxSize();

	for (auto const& page : m_pages) {
		if (!page.page->CreatePage(m_pOptions, this, parentPanel, size))
			return false;
	}

	if (!LoadSettings()) {
		wxMessageBoxEx(_("Failed to load panels, invalid resource files?"));
		return false;
	}

	wxSize canvas;
	canvas.x = GetSize().x - parentPanel->GetSize().x;
	canvas.y = GetSize().y - parentPanel->GetSize().y;

	// Wrap pages nicely
	std::vector<wxWindow*> pages;
	for (auto const& page : m_pages) {
		pages.push_back(page.page);
	}
	wxGetApp().GetWrapEngine()->WrapRecursive(pages, 1.33, "Settings", canvas);

#ifdef __WXGTK__
	// Pre-show dialog under GTK, else panels won't get initialized properly
	Show();
#endif

	// Keep track of maximum page size
	size = wxSize(0, 0);
	for (auto const& page : m_pages) {
		size.IncTo(page.page->GetSizer()->GetMinSize());
	}

	wxSize panelSize = size;
#ifdef __WXGTK__
	panelSize.x += 1;
#endif
	parentPanel->SetInitialSize(panelSize);

	// Adjust pages sizes according to maximum size
	for (auto const& page : m_pages) {
		page.page->GetSizer()->SetMinSize(size);
		page.page->GetSizer()->Fit(page.page);
		page.page->GetSizer()->SetSizeHints(page.page);
		if( GetLayoutDirection() == wxLayout_RightToLeft ) {
			page.page->Move(wxPoint(1, 0));
		}
	}

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);

	for (auto const& page : m_pages) {
		page.page->Hide();
	}

	// Select first page
	treeCtrl->SelectItem(m_pages[0].id);
	if (!m_activePanel)	{
		m_activePanel = m_pages[0].page;
		m_activePanel->Display();
	}

	return true;
}

bool CSettingsDialog::LoadSettings()
{
	for (auto const& page : m_pages) {
		if (!page.page->LoadPage())
			return false;
	}

	return true;
}

void CSettingsDialog::OnPageChanged(wxTreeEvent& event)
{
	if (m_activePanel)
		m_activePanel->Hide();

	wxTreeItemId item = event.GetItem();

	for( auto const& page : m_pages ) {
		if (page.id == item) {
			m_activePanel = page.page;
			m_activePanel->Display();
			break;
		}
	}
}

void CSettingsDialog::OnOK(wxCommandEvent& event)
{
	for( auto const& page : m_pages ) {
		if (!page.page->Validate()) {
			if (m_activePanel != page.page) {
				wxTreeCtrl* treeCtrl = XRCCTRL(*this, "ID_TREE", wxTreeCtrl);
				treeCtrl->SelectItem(page.id);
			}
			return;
		}
	}

	for( auto const& page : m_pages ) {
		page.page->SavePage();
	}

	EndModal(wxID_OK);
}

void CSettingsDialog::OnCancel(wxCommandEvent& event)
{
	EndModal(wxID_CANCEL);
}

void CSettingsDialog::OnPageChanging(wxTreeEvent& event)
{
	if (!m_activePanel)
		return;

	if (!m_activePanel->Validate())
		event.Veto();
}
