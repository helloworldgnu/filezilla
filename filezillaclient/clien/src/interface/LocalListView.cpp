#include <filezilla.h>

#define FILELISTCTRL_INCLUDE_TEMPLATE_DEFINITION

#include "LocalListView.h"
#include "queue.h"
#include "filezillaapp.h"
#include "filter.h"
#include "file_utils.h"
#include "inputdialog.h"
#include <algorithm>
#include "dndobjects.h"
#include "Options.h"
#ifdef __WXMSW__
#include "lm.h"
#include <wx/msw/registry.h>
#include "volume_enumerator.h"
#endif
#include "edithandler.h"
#include "dragdropmanager.h"
#include "drop_target_ex.h"
#include "local_filesys.h"
#include "filelist_statusbar.h"
#include "sizeformatting.h"
#include "timeformatting.h"

class CLocalListViewDropTarget : public CScrollableDropTarget<wxListCtrlEx>
{
public:
	CLocalListViewDropTarget(CLocalListView* pLocalListView)
		: CScrollableDropTarget<wxListCtrlEx>(pLocalListView)
		, m_pLocalListView(pLocalListView), m_pFileDataObject(new wxFileDataObject()),
		m_pRemoteDataObject(new CRemoteDataObject())
	{
		m_pDataObject = new wxDataObjectComposite;
		m_pDataObject->Add(m_pRemoteDataObject, true);
		m_pDataObject->Add(m_pFileDataObject, false);
		SetDataObject(m_pDataObject);
	}

	void ClearDropHighlight()
	{
		const int dropTarget = m_pLocalListView->m_dropTarget;
		if (dropTarget != -1)
		{
			m_pLocalListView->m_dropTarget = -1;
#ifdef __WXMSW__
			m_pLocalListView->SetItemState(dropTarget, 0, wxLIST_STATE_DROPHILITED);
#else
			m_pLocalListView->RefreshItem(dropTarget);
#endif
		}
	}

	virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def)
	{
		def = CScrollableDropTarget<wxListCtrlEx>::FixupDragResult(def);

		if (def == wxDragError ||
			def == wxDragNone ||
			def == wxDragCancel)
			return def;

		if (m_pLocalListView->m_fileData.empty())
			return wxDragError;

		if (def != wxDragCopy && def != wxDragMove)
			return wxDragError;

		CDragDropManager* pDragDropManager = CDragDropManager::Get();
		if (pDragDropManager)
			pDragDropManager->pDropTarget = m_pLocalListView;

		wxString subdir;
		int flags;
		int hit = m_pLocalListView->HitTest(wxPoint(x, y), flags, 0);
		if (hit != -1 && (flags & wxLIST_HITTEST_ONITEM))
		{
			const CLocalFileData* const data = m_pLocalListView->GetData(hit);
			if (data && data->dir)
				subdir = data->name;
		}

		CLocalPath dir = m_pLocalListView->m_pState->GetLocalDir();
		if (!subdir.empty())
		{
			if (!dir.ChangePath(subdir))
				return wxDragError;
		}

		if (!dir.IsWriteable())
			return wxDragError;

		if (!GetData())
			return wxDragError;

		if (m_pDataObject->GetReceivedFormat() == m_pFileDataObject->GetFormat())
			m_pLocalListView->m_pState->HandleDroppedFiles(m_pFileDataObject, dir, def == wxDragCopy);
		else
		{
			if (m_pRemoteDataObject->GetProcessId() != (int)wxGetProcessId())
			{
				wxMessageBoxEx(_("Drag&drop between different instances of FileZilla has not been implemented yet."));
				return wxDragNone;
			}

			if (!m_pLocalListView->m_pState->GetServer() || !m_pRemoteDataObject->GetServer().EqualsNoPass(*m_pLocalListView->m_pState->GetServer()))
			{
				wxMessageBoxEx(_("Drag&drop between different servers has not been implemented yet."));
				return wxDragNone;
			}

			if (!m_pLocalListView->m_pState->DownloadDroppedFiles(m_pRemoteDataObject, dir))
				return wxDragNone;
		}

		return def;
	}

	virtual bool OnDrop(wxCoord x, wxCoord y)
	{
		CScrollableDropTarget<wxListCtrlEx>::OnDrop(x, y);
		ClearDropHighlight();

		if (m_pLocalListView->m_fileData.empty())
			return false;

		return true;
	}

	virtual int DisplayDropHighlight(wxPoint point)
	{
		DoDisplayDropHighlight(point);
		return -1;
	}

	virtual wxString DoDisplayDropHighlight(wxPoint point)
	{
		wxString subDir;

		int flags;
		int hit = m_pLocalListView->HitTest(point, flags, 0);
		if (!(flags & wxLIST_HITTEST_ONITEM))
			hit = -1;

		if (hit != -1)
		{
			const CLocalFileData* const data = m_pLocalListView->GetData(hit);
			if (!data || !data->dir)
				hit = -1;
			else
			{
				const CDragDropManager* pDragDropManager = CDragDropManager::Get();
				if (pDragDropManager && pDragDropManager->pDragSource == m_pLocalListView)
				{
					if (m_pLocalListView->GetItemState(hit, wxLIST_STATE_SELECTED))
						hit = -1;
					else
						subDir = data->name;
				}
				else
					subDir = data->name;
			}
		}
		if (hit != m_pLocalListView->m_dropTarget)
		{
			ClearDropHighlight();
			if (hit != -1)
			{
				m_pLocalListView->m_dropTarget = hit;
#ifdef __WXMSW__
				m_pLocalListView->SetItemState(hit, wxLIST_STATE_DROPHILITED, wxLIST_STATE_DROPHILITED);
#else
				m_pLocalListView->RefreshItem(hit);
#endif
			}
		}

		return subDir;
	}

	virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def)
	{
		def = CScrollableDropTarget<wxListCtrlEx>::OnDragOver(x, y, def);

		if (def == wxDragError ||
			def == wxDragNone ||
			def == wxDragCancel)
		{
			ClearDropHighlight();
			return def;
		}

		if (m_pLocalListView->m_fileData.empty())
		{
			ClearDropHighlight();
			return wxDragNone;
		}

		const wxString& subdir = DoDisplayDropHighlight(wxPoint(x, y));

		CLocalPath dir = m_pLocalListView->m_pState->GetLocalDir();
		if (subdir.empty()) {
			const CDragDropManager* pDragDropManager = CDragDropManager::Get();
			if (pDragDropManager && pDragDropManager->localParent == m_pLocalListView->m_dir)
				return wxDragNone;
		}
		else
		{
			if (!dir.ChangePath(subdir))
				return wxDragNone;
		}

		if (!dir.IsWriteable())
			return wxDragNone;

		return def;
	}

	virtual void OnLeave()
	{
		CScrollableDropTarget<wxListCtrlEx>::OnLeave();
		ClearDropHighlight();
	}

	virtual wxDragResult OnEnter(wxCoord x, wxCoord y, wxDragResult def)
	{
		def = CScrollableDropTarget<wxListCtrlEx>::OnEnter(x, y, def);
		return OnDragOver(x, y, def);
	}

protected:
	CLocalListView *m_pLocalListView;
	wxFileDataObject* m_pFileDataObject;
	CRemoteDataObject* m_pRemoteDataObject;
	wxDataObjectComposite* m_pDataObject;
};

BEGIN_EVENT_TABLE(CLocalListView, CFileListCtrl<CLocalFileData>)
	EVT_LIST_ITEM_ACTIVATED(wxID_ANY, CLocalListView::OnItemActivated)
	EVT_CONTEXT_MENU(CLocalListView::OnContextMenu)
	// Map both ID_UPLOAD and ID_ADDTOQUEUE to OnMenuUpload, code is identical
	EVT_MENU(XRCID("ID_UPLOAD"), CLocalListView::OnMenuUpload)
	EVT_MENU(XRCID("ID_ADDTOQUEUE"), CLocalListView::OnMenuUpload)
	EVT_MENU(XRCID("ID_MKDIR"), CLocalListView::OnMenuMkdir)
	EVT_MENU(XRCID("ID_MKDIR_CHGDIR"), CLocalListView::OnMenuMkdirChgDir)
	EVT_MENU(XRCID("ID_DELETE"), CLocalListView::OnMenuDelete)
	EVT_MENU(XRCID("ID_RENAME"), CLocalListView::OnMenuRename)
	EVT_KEY_DOWN(CLocalListView::OnKeyDown)
	EVT_LIST_BEGIN_DRAG(wxID_ANY, CLocalListView::OnBeginDrag)
	EVT_MENU(XRCID("ID_OPEN"), CLocalListView::OnMenuOpen)
	EVT_MENU(XRCID("ID_EDIT"), CLocalListView::OnMenuEdit)
	EVT_MENU(XRCID("ID_ENTER"), CLocalListView::OnMenuEnter)
#ifdef __WXMSW__
	EVT_COMMAND(-1, fzEVT_VOLUMESENUMERATED, CLocalListView::OnVolumesEnumerated)
	EVT_COMMAND(-1, fzEVT_VOLUMEENUMERATED, CLocalListView::OnVolumesEnumerated)
#endif
	EVT_MENU(XRCID("ID_CONTEXT_REFRESH"), CLocalListView::OnMenuRefresh)
END_EVENT_TABLE()

CLocalListView::CLocalListView(wxWindow* pParent, CState *pState, CQueueView *pQueue)
	: CFileListCtrl<CLocalFileData>(pParent, pState, pQueue),
	CStateEventHandler(pState)
{
	wxGetApp().AddStartupProfileRecord(_T("CLocalListView::CLocalListView"));
	m_pState->RegisterHandler(this, STATECHANGE_LOCAL_DIR);
	m_pState->RegisterHandler(this, STATECHANGE_APPLYFILTER);
	m_pState->RegisterHandler(this, STATECHANGE_LOCAL_REFRESH_FILE);

	m_dropTarget = -1;

	const unsigned long widths[4] = { 120, 80, 100, 120 };

	AddColumn(_("Filename"), wxLIST_FORMAT_LEFT, widths[0], true);
	AddColumn(_("Filesize"), wxLIST_FORMAT_RIGHT, widths[1]);
	AddColumn(_("Filetype"), wxLIST_FORMAT_LEFT, widths[2]);
	AddColumn(_("Last modified"), wxLIST_FORMAT_LEFT, widths[3]);
	LoadColumnSettings(OPTION_LOCALFILELIST_COLUMN_WIDTHS, OPTION_LOCALFILELIST_COLUMN_SHOWN, OPTION_LOCALFILELIST_COLUMN_ORDER);

	InitSort(OPTION_LOCALFILELIST_SORTORDER);

	SetImageList(GetSystemImageList(), wxIMAGE_LIST_SMALL);

#ifdef __WXMSW__
	m_pVolumeEnumeratorThread = 0;
#endif

	InitHeaderSortImageList();

	SetDropTarget(new CLocalListViewDropTarget(this));

	EnablePrefixSearch(true);
}

CLocalListView::~CLocalListView()
{
	wxString str = wxString::Format(_T("%d %d"), m_sortDirection, m_sortColumn);
	COptions::Get()->SetOption(OPTION_LOCALFILELIST_SORTORDER, str);

#ifdef __WXMSW__
	delete m_pVolumeEnumeratorThread;
#endif
}

bool CLocalListView::DisplayDir(CLocalPath const& dirname)
{
	CancelLabelEdit();

	wxString focused;
	std::list<wxString> selectedNames;
	bool ensureVisible = false;
	if (m_dir != dirname)
	{
		ResetSearchPrefix();

		if (IsComparing())
			ExitComparisonMode();

		ClearSelection();
		focused = m_pState->GetPreviouslyVisitedLocalSubdir();
		ensureVisible = !focused.empty();
		if (focused.empty())
			focused = _T("..");

		if (GetItemCount())
			EnsureVisible(0);
		m_dir = dirname;
	}
	else
	{
		// Remember which items were selected
		selectedNames = RememberSelectedItems(focused);
	}

	if (m_pFilelistStatusBar)
		m_pFilelistStatusBar->UnselectAll();

	const int oldItemCount = m_indexMapping.size();

	m_fileData.clear();
	m_indexMapping.clear();

	m_hasParent = m_dir.HasLogicalParent();

	if (m_hasParent) {
		CLocalFileData data;
		data.dir = true;
		data.name = _T("..");
		data.size = -1;
		m_fileData.push_back(data);
		m_indexMapping.push_back(0);
	}

#ifdef __WXMSW__
	if (m_dir.GetPath() == _T("\\")) {
		DisplayDrives();
	}
	else if (m_dir.GetPath().Left(2) == _T("\\\\"))
	{
		int pos = m_dir.GetPath().Mid(2).Find('\\');
		if (pos != -1 && pos + 3 != (int)m_dir.GetPath().Len())
			goto regular_dir;

		// UNC path without shares
		DisplayShares(m_dir.GetPath());
	}
	else
#endif
	{
#ifdef __WXMSW__
regular_dir:
#endif
		CFilterManager filter;
		CLocalFileSystem local_filesystem;

		if (!local_filesystem.BeginFindFiles(m_dir.GetPath(), false)) {
			SetItemCount(1);
			return false;
		}

		int64_t totalSize{};
		int unknown_sizes = 0;
		int totalFileCount = 0;
		int totalDirCount = 0;
		int hidden = 0;

		int num = m_fileData.size();
		CLocalFileData data;
		bool wasLink;
		while (local_filesystem.GetNextFile(data.name, wasLink, data.dir, &data.size, &data.time, &data.attributes)) {
			if (data.name.empty()) {
				wxGetApp().DisplayEncodingWarning();
				continue;
			}

			m_fileData.push_back(data);
			if (!filter.FilenameFiltered(data.name, m_dir.GetPath(), data.dir, data.size, true, data.attributes, data.time)) {
				if (data.dir)
					totalDirCount++;
				else {
					if (data.size != -1)
						totalSize += data.size;
					else
						unknown_sizes++;
					totalFileCount++;
				}
				m_indexMapping.push_back(num);
			}
			else
				hidden++;
			num++;
		}

		if (m_pFilelistStatusBar)
			m_pFilelistStatusBar->SetDirectoryContents(totalFileCount, totalDirCount, totalSize, unknown_sizes, hidden);
	}

	if (m_dropTarget != -1) {
		CLocalFileData* data = GetData(m_dropTarget);
		if (!data || !data->dir) {
			SetItemState(m_dropTarget, 0, wxLIST_STATE_DROPHILITED);
			m_dropTarget = -1;
		}
	}

	const int count = m_indexMapping.size();
	if (oldItemCount != count)
		SetItemCount(count);

	SortList(-1, -1, false);

	if (IsComparing()) {
		m_originalIndexMapping.clear();
		RefreshComparison();
	}

	ReselectItems(selectedNames, focused, ensureVisible);

	RefreshListOnly();

	return true;
}

// See comment to OnGetItemText
int CLocalListView::OnGetItemImage(long item) const
{
	CLocalListView *pThis = const_cast<CLocalListView *>(this);
	CLocalFileData *data = pThis->GetData(item);
	if (!data)
		return -1;
	int &icon = data->icon;

	if (icon == -2)
	{
		wxString path = _T("");
		if (data->name != _T(".."))
		{
#ifdef __WXMSW__
			if (m_dir.GetPath() == _T("\\"))
				path = data->name + _T("\\");
			else
#endif
				path = m_dir.GetPath() + data->name;
		}

		icon = pThis->GetIconIndex(data->dir ? iconType::dir : iconType::file, path);
	}
	return icon;
}

void CLocalListView::OnItemActivated(wxListEvent &event)
{
	int count = 0;
	bool back = false;

	int item = -1;
	for (;;)
	{
		item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1)
			break;

		count++;

		if (!item && m_hasParent)
			back = true;
	}
	if (count > 1)
	{
		if (back)
		{
			wxBell();
			return;
		}

		wxCommandEvent cmdEvent;
		OnMenuUpload(cmdEvent);
		return;
	}

	item = event.GetIndex();

	CLocalFileData *data = GetData(item);
	if (!data)
		return;

	if (data->dir)
	{
		const int action = COptions::Get()->GetOptionVal(OPTION_DOUBLECLICK_ACTION_DIRECTORY);
		if (action == 3)
		{
			// No action
			wxBell();
			return;
		}

		if (!action || data->name == _T(".."))
		{
			// Enter action

			wxString error;
			if (!m_pState->SetLocalDir(data->name, &error))
			{
				if (!error.empty())
					wxMessageBoxEx(error, _("Failed to change directory"), wxICON_INFORMATION);
				else
					wxBell();
			}
			return;
		}

		wxCommandEvent evt(0, action == 1 ? XRCID("ID_UPLOAD") : XRCID("ID_ADDTOQUEUE"));
		OnMenuUpload(evt);
		return;
	}

	if (data->comparison_flags == fill)
	{
		wxBell();
		return;
	}

	const int action = COptions::Get()->GetOptionVal(OPTION_DOUBLECLICK_ACTION_FILE);
	if (action == 3)
	{
		// No action
		wxBell();
		return;
	}

	if (action == 2)
	{
		// View / Edit action
		wxCommandEvent evt;
		OnMenuEdit(evt);
		return;
	}

	const CServer* pServer = m_pState->GetServer();
	if (!pServer)
	{
		wxBell();
		return;
	}

	CServerPath path = m_pState->GetRemotePath();
	if (path.empty())
	{
		wxBell();
		return;
	}

	const bool queue_only = action == 1;

	m_pQueue->QueueFile(queue_only, false, data->name, wxEmptyString, m_dir, path, *pServer, data->size);
	m_pQueue->QueueFile_Finish(true);
}

void CLocalListView::OnMenuEnter(wxCommandEvent &)
{
	int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item == -1) {
		wxBell();
		return;
	}

	if (GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1) {
		wxBell();
		return;
	}

	CLocalFileData *data = GetData(item);
	if (!data || !data->dir) {
		wxBell();
		return;
	}

	wxString error;
	if (!m_pState->SetLocalDir(data->name, &error)) {
		if (!error.empty())
			wxMessageBoxEx(error, _("Failed to change directory"), wxICON_INFORMATION);
		else
			wxBell();
	}
}

#ifdef __WXMSW__
void CLocalListView::DisplayDrives()
{
	int count = m_fileData.size();

	std::list<wxString> drives = CVolumeDescriptionEnumeratorThread::GetDrives();
	for( std::list<wxString>::const_iterator it = drives.begin(); it != drives.end(); ++it ) {
		wxString drive = *it;
		if (drive.Right(1) == _T("\\"))
			drive.RemoveLast();

		CLocalFileData data;
		data.name = drive;
		data.label = CSparseOptional<wxString>(data.name);
		data.dir = true;
		data.size = -1;

		m_fileData.push_back(data);
		m_indexMapping.push_back(count);
		count++;
	}

	if (m_pFilelistStatusBar)
		m_pFilelistStatusBar->SetDirectoryContents(0, drives.size(), 0, false, 0);

	if (!m_pVolumeEnumeratorThread) {
		m_pVolumeEnumeratorThread = new CVolumeDescriptionEnumeratorThread(this);
		if (m_pVolumeEnumeratorThread->Failed()) {
			delete m_pVolumeEnumeratorThread;
			m_pVolumeEnumeratorThread = 0;
		}
	}
}

void CLocalListView::DisplayShares(wxString computer)
{
	// Cast through a union to avoid warning about breaking strict aliasing rule
	union
	{
		SHARE_INFO_1* pShareInfo;
		LPBYTE pShareInfoBlob;
	} si;

	DWORD read, total;
	DWORD resume_handle = 0;

	if (!computer.empty() && computer.Last() == '\\')
		computer.RemoveLast();

	int j = m_fileData.size();
	int share_count = 0;
	int res = 0;
	do
	{
		const wxWX2WCbuf buf = computer.wc_str(wxConvLocal);
		res = NetShareEnum((wchar_t*)(const wchar_t*)buf, 1, &si.pShareInfoBlob, MAX_PREFERRED_LENGTH, &read, &total, &resume_handle);

		if (res != ERROR_SUCCESS && res != ERROR_MORE_DATA)
			break;

		SHARE_INFO_1* p = si.pShareInfo;
		for (unsigned int i = 0; i < read; i++, p++)
		{
			if (p->shi1_type != STYPE_DISKTREE)
				continue;

			CLocalFileData data;
			data.name = p->shi1_netname;
#ifdef __WXMSW__
			data.label = CSparseOptional<wxString>(data.name);
#endif
			data.dir = true;
			data.size = -1;

			m_fileData.push_back(data);
			m_indexMapping.push_back(j++);

			share_count++;
		}

		NetApiBufferFree(si.pShareInfo);
	}
	while (res == ERROR_MORE_DATA);

	if (m_pFilelistStatusBar)
		m_pFilelistStatusBar->SetDirectoryContents(0, share_count, 0, false, 0);
}

#endif //__WXMSW__

CLocalFileData* CLocalListView::GetData(unsigned int item)
{
	if (!IsItemValid(item))
		return 0;

	return &m_fileData[m_indexMapping[item]];
}

bool CLocalListView::IsItemValid(unsigned int item) const
{
	if (item >= m_indexMapping.size())
		return false;

	unsigned int index = m_indexMapping[item];
	if (index >= m_fileData.size())
		return false;

	return true;
}

CFileListCtrl<CLocalFileData>::CSortComparisonObject CLocalListView::GetSortComparisonObject()
{
	CFileListCtrlSortBase::DirSortMode dirSortMode = GetDirSortMode();
	CFileListCtrlSortBase::NameSortMode nameSortMode = GetNameSortMode();

	if (!m_sortDirection)
	{
		if (m_sortColumn == 1)
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CFileListCtrlSortSize<std::vector<CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
		else if (m_sortColumn == 2)
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CFileListCtrlSortType<std::vector<CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
		else if (m_sortColumn == 3)
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CFileListCtrlSortTime<std::vector<CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
		else
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CFileListCtrlSortName<std::vector<CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
	}
	else
	{
		if (m_sortColumn == 1)
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CReverseSort<CFileListCtrlSortSize<std::vector<CLocalFileData>, CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
		else if (m_sortColumn == 2)
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CReverseSort<CFileListCtrlSortType<std::vector<CLocalFileData>, CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
		else if (m_sortColumn == 3)
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CReverseSort<CFileListCtrlSortTime<std::vector<CLocalFileData>, CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
		else
			return CFileListCtrl<CLocalFileData>::CSortComparisonObject(new CReverseSort<CFileListCtrlSortName<std::vector<CLocalFileData>, CLocalFileData>, CLocalFileData>(m_fileData, m_fileData, dirSortMode, nameSortMode, this));
	}
}

void CLocalListView::OnContextMenu(wxContextMenuEvent& event)
{
	if (GetEditControl())
	{
		event.Skip();
		return;
	}

	wxMenu* pMenu = wxXmlResource::Get()->LoadMenu(_T("ID_MENU_LOCALFILELIST"));
	if (!pMenu)
		return;

	const bool connected = m_pState->IsRemoteConnected();
	if (!connected)
	{
		pMenu->Enable(XRCID("ID_EDIT"), COptions::Get()->GetOptionVal(OPTION_EDIT_TRACK_LOCAL) == 0);
		pMenu->Enable(XRCID("ID_UPLOAD"), false);
		pMenu->Enable(XRCID("ID_ADDTOQUEUE"), false);
	}

	int index = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	int count = 0;
	int fillCount = 0;
	bool selectedDir = false;
	while (index != -1)
	{
		count++;
		const CLocalFileData* const data = GetData(index);
		if (!data || (!index && m_hasParent))
		{
			pMenu->Enable(XRCID("ID_OPEN"), false);
			pMenu->Enable(XRCID("ID_RENAME"), false);
			pMenu->Enable(XRCID("ID_EDIT"), false);
		}
		if ((data && data->comparison_flags == fill) || (!index && m_hasParent))
			fillCount++;
		if (data && data->dir)
			selectedDir = true;
		index = GetNextItem(index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}
	if (!count || fillCount == count)
	{
		pMenu->Delete(XRCID("ID_ENTER"));
		pMenu->Enable(XRCID("ID_UPLOAD"), false);
		pMenu->Enable(XRCID("ID_ADDTOQUEUE"), false);
		pMenu->Enable(XRCID("ID_DELETE"), false);
		pMenu->Enable(XRCID("ID_RENAME"), false);
		pMenu->Enable(XRCID("ID_EDIT"), false);
	}
	else if (count > 1)
	{
		pMenu->Delete(XRCID("ID_ENTER"));
		pMenu->Enable(XRCID("ID_RENAME"), false);
	}
	else
	{
		// Exactly one item selected
		if (!selectedDir)
			pMenu->Delete(XRCID("ID_ENTER"));
	}
	if (selectedDir)
		pMenu->Enable(XRCID("ID_EDIT"), false);

	PopupMenu(pMenu);
	delete pMenu;
}

void CLocalListView::OnMenuUpload(wxCommandEvent& event)
{
	const CServer* pServer = m_pState->GetServer();
	if (!pServer) {
		wxBell();
		return;
	}

	bool added = false;

	bool queue_only = event.GetId() == XRCID("ID_ADDTOQUEUE");

	long item = -1;
	for (;;) {
		item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (!item && m_hasParent)
			continue;
		if (item == -1)
			break;

		const CLocalFileData *data = GetData(item);
		if (!data)
			break;

		if (data->comparison_flags == fill)
			continue;

		CServerPath path = m_pState->GetRemotePath();
		if (path.empty()) {
			wxBell();
			break;
		}

		if (data->dir) {
			if (!path.ChangePath(data->name)) {
				continue;
			}

			CLocalPath localPath(m_dir);
			localPath.AddSegment(data->name);
			m_pQueue->QueueFolder(event.GetId() == XRCID("ID_ADDTOQUEUE"), false, localPath, path, *pServer);
		}
		else {
			m_pQueue->QueueFile(queue_only, false, data->name, wxEmptyString, m_dir, path, *pServer, data->size);
			added = true;
		}
	}
	if (added)
		m_pQueue->QueueFile_Finish(!queue_only);
}

// Create a new Directory
void CLocalListView::OnMenuMkdir(wxCommandEvent&)
{
	wxString newdir = MenuMkdir();
	if (!newdir.empty()) {
		m_pState->RefreshLocal();
	}
}

// Create a new Directory and enter the new Directory
void CLocalListView::OnMenuMkdirChgDir(wxCommandEvent&)
{
	wxString newdir = MenuMkdir();
	if (newdir.empty()) {
		return;
	}

	// OnMenuEnter
	wxString error;
	if (!m_pState->SetLocalDir(newdir, &error))
	{
		if (!error.empty())
			wxMessageBoxEx(error, _("Failed to change directory"), wxICON_INFORMATION);
		else
			wxBell();
	}
}

// Helper-Function to create a new Directory
// Returns the name of the new directory
wxString CLocalListView::MenuMkdir()
{
	CInputDialog dlg;
	if (!dlg.Create(this, _("Create directory"), _("Please enter the name of the directory which should be created:")))
		return wxString();

	if (dlg.ShowModal() != wxID_OK)
		return wxString();

	if (dlg.GetValue().empty())
	{
		wxBell();
		return wxString();
	}

	wxFileName fn(dlg.GetValue(), _T(""));
	fn.Normalize(wxPATH_NORM_ALL, m_dir.GetPath());

	bool res;

	{
		wxLogNull log;
		res = fn.Mkdir(fn.GetPath(), 0777, wxPATH_MKDIR_FULL);
	}

	if (!res) {
		wxBell();
		return wxString();
	}

	// Return name of the New Directory
	return fn.GetPath();
}

void CLocalListView::OnMenuDelete(wxCommandEvent&)
{
	std::list<wxString> pathsToDelete;
	long item = -1;
	while ((item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
		if (!item && m_hasParent)
			continue;

		CLocalFileData *data = GetData(item);
		if (!data)
			continue;

		if (data->comparison_flags == fill)
			continue;

		pathsToDelete.push_back(m_dir.GetPath() + data->name);
	}
	if (!CLocalFileSystem::RecursiveDelete(pathsToDelete, this))
		wxGetApp().DisplayEncodingWarning();

	m_pState->SetLocalDir(m_dir);
}

void CLocalListView::OnMenuRename(wxCommandEvent&)
{
	if (GetEditControl()) {
		GetEditControl()->SetFocus();
		return;
	}

	int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item < 0 || (!item && m_hasParent)) {
		wxBell();
		return;
	}

	if (GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) != -1) {
		wxBell();
		return;
	}

	CLocalFileData *data = GetData(item);
	if (!data || data->comparison_flags == fill) {
		wxBell();
		return;
	}

	EditLabel(item);
}

void CLocalListView::OnKeyDown(wxKeyEvent& event)
{
#ifdef __WXMAC__
#define CursorModifierKey wxMOD_CMD
#else
#define CursorModifierKey wxMOD_ALT
#endif

	const int code = event.GetKeyCode();
	if (code == WXK_DELETE || code == WXK_NUMPAD_DELETE) {
		if (GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) == -1) {
			wxBell();
			return;
		}

		wxCommandEvent tmp;
		OnMenuDelete(tmp);
	}
	else if (code == WXK_F2) {
		wxCommandEvent tmp;
		OnMenuRename(tmp);
	}
	else if (code == WXK_RIGHT && event.GetModifiers() == CursorModifierKey) {
		wxListEvent evt;
		evt.m_itemIndex = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
		OnItemActivated(evt);
	}
	else if (code == WXK_DOWN && event.GetModifiers() == CursorModifierKey) {
		wxCommandEvent cmdEvent;
		OnMenuUpload(cmdEvent);
	}
	else if (code == 'N' && event.GetModifiers() == (wxMOD_CONTROL | wxMOD_SHIFT)) {
		wxCommandEvent cmdEvent;
		OnMenuMkdir(cmdEvent);
	}
	else
		event.Skip();
}

bool CLocalListView::OnBeginRename(const wxListEvent& event)
{
	if (!m_pState->GetLocalDir().IsWriteable())
		return false;

	if (event.GetIndex() == 0 && m_hasParent)
		return false;

	const CLocalFileData * const data = GetData(event.GetIndex());
	if (!data || data->comparison_flags == fill)
		return false;

	return true;
}

bool CLocalListView::OnAcceptRename(const wxListEvent& event)
{
	const int index = event.GetIndex();
	if (!index && m_hasParent)
		return false;

	if (event.GetLabel().empty())
		return false;

	if (!m_pState->GetLocalDir().IsWriteable())
		return false;

	CLocalFileData *const data = GetData(event.GetIndex());
	if (!data || data->comparison_flags == fill)
		return false;

	wxString newname = event.GetLabel();
#ifdef __WXMSW__
	newname = newname.Left(255);
#endif

	if (newname == data->name)
		return false;

	if (!RenameFile(this, m_dir.GetPath(), data->name, newname))
		return false;

	data->name = newname;
#ifdef __WXMSW__
	data->label.clear();
#endif
	m_pState->RefreshLocal();

	return true;
}

void CLocalListView::ApplyCurrentFilter()
{
	CFilterManager filter;

	if (!filter.HasSameLocalAndRemoteFilters() && IsComparing())
		ExitComparisonMode();

	unsigned int min = m_hasParent ? 1 : 0;
	if (m_fileData.size() <= min)
		return;

	wxString focused;
	const std::list<wxString>& selectedNames = RememberSelectedItems(focused);

	if (m_pFilelistStatusBar)
		m_pFilelistStatusBar->UnselectAll();

	wxLongLong totalSize;
	int unknown_sizes = 0;
	int totalFileCount = 0;
	int totalDirCount = 0;
	int hidden = 0;

	m_indexMapping.clear();
	if (m_hasParent)
		m_indexMapping.push_back(0);
	for (unsigned int i = min; i < m_fileData.size(); i++)
	{
		const CLocalFileData& data = m_fileData[i];
		if (data.comparison_flags == fill)
			continue;
		if (filter.FilenameFiltered(data.name, m_dir.GetPath(), data.dir, data.size, true, data.attributes, data.time)) {
			hidden++;
			continue;
		}

		if (data.dir)
			totalDirCount++;
		else
		{
			if (data.size != -1)
				totalSize += data.size;
			else
				unknown_sizes++;
			totalFileCount++;
		}

		m_indexMapping.push_back(i);
	}
	SetItemCount(m_indexMapping.size());

	if (m_pFilelistStatusBar)
		m_pFilelistStatusBar->SetDirectoryContents(totalFileCount, totalDirCount, totalSize, unknown_sizes, hidden);

	SortList(-1, -1, false);

	if (IsComparing())
	{
		m_originalIndexMapping.clear();
		RefreshComparison();
	}

	ReselectItems(selectedNames, focused);

	if (!IsComparing())
		RefreshListOnly();
}

std::list<wxString> CLocalListView::RememberSelectedItems(wxString& focused)
{
	std::list<wxString> selectedNames;
	// Remember which items were selected
#ifndef __WXMSW__
	// GetNextItem is O(n) if nothing is selected, GetSelectedItemCount() is O(1)
	if (GetSelectedItemCount())
#endif
	{
		int item = -1;
		for (;;)
		{
			item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if (item == -1)
				break;
			const CLocalFileData &data = m_fileData[m_indexMapping[item]];
			if (data.comparison_flags != fill)
			{
				if (data.dir)
					selectedNames.push_back(_T("d") + data.name);
				else
					selectedNames.push_back(_T("-") + data.name);
			}
			SetSelection(item, false);
		}
	}

	int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
	if (item != -1)
	{
		const CLocalFileData &data = m_fileData[m_indexMapping[item]];
		if (data.comparison_flags != fill)
			focused = data.name;

		SetItemState(item, 0, wxLIST_STATE_FOCUSED);
	}

	return selectedNames;
}

void CLocalListView::ReselectItems(const std::list<wxString>& selectedNames, wxString focused, bool ensureVisible)
{
	// Reselect previous items if neccessary.
	// Sorting direction did not change. We just have to scan through items once

	if (selectedNames.empty())
	{
		if (focused.empty())
			return;
		for (unsigned int i = 0; i < m_indexMapping.size(); i++)
		{
			const CLocalFileData &data = m_fileData[m_indexMapping[i]];
			if (data.name == focused)
			{
				SetItemState(i, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
				if (ensureVisible)
					EnsureVisible(i);
				return;
			}
		}
		return;
	}

	int firstSelected = -1;

	int i = -1;
	for (std::list<wxString>::const_iterator iter = selectedNames.begin(); iter != selectedNames.end(); ++iter)
	{
		while (++i < (int)m_indexMapping.size())
		{
			const CLocalFileData &data = m_fileData[m_indexMapping[i]];
			if (data.name == focused)
			{
				SetItemState(i, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
				if (ensureVisible)
					EnsureVisible(i);
				focused = _T("");
			}
			if (data.dir && *iter == (_T("d") + data.name))
			{
				if (firstSelected == -1)
					firstSelected = i;
				if (m_pFilelistStatusBar)
					m_pFilelistStatusBar->SelectDirectory();
				SetSelection(i, true);
				break;
			}
			else if (*iter == (_T("-") + data.name))
			{
				if (firstSelected == -1)
					firstSelected = i;
				if (m_pFilelistStatusBar)
					m_pFilelistStatusBar->SelectFile(data.size);
				SetSelection(i, true);
				break;
			}
		}
		if (i == (int)m_indexMapping.size())
			break;
	}
	if (!focused.empty())
	{
		if (firstSelected != -1)
			SetItemState(firstSelected, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
		else
			SetItemState(0, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
	}
}

void CLocalListView::OnStateChange(CState*, enum t_statechange_notifications notification, const wxString& data, const void*)
{
	if (notification == STATECHANGE_LOCAL_DIR)
		DisplayDir(m_pState->GetLocalDir());
	else if (notification == STATECHANGE_APPLYFILTER)
		ApplyCurrentFilter();
	else
	{
		wxASSERT(notification == STATECHANGE_LOCAL_REFRESH_FILE);
		RefreshFile(data);
	}
}

void CLocalListView::OnBeginDrag(wxListEvent&)
{
	long item = -1;
	for (;;) {
		item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1)
			break;

		if (!item && m_hasParent)
			return;
	}

	wxFileDataObject obj;

	CDragDropManager* pDragDropManager = CDragDropManager::Init();
	pDragDropManager->pDragSource = this;
	pDragDropManager->localParent = m_dir;

	item = -1;
	for (;;) {
		item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1)
			break;

		CLocalFileData *data = GetData(item);
		if (!data)
			continue;

		if (data->comparison_flags == fill)
			continue;

		wxString const name = m_dir.GetPath() + data->name;
		pDragDropManager->m_localFiles.push_back(name);
		obj.AddFile(name);
	}

	if (obj.GetFilenames().empty()) {
		pDragDropManager->Release();
		return;
	}

	CLabelEditBlocker b(*this);

	wxDropSource source(this);
	source.SetData(obj);
	int res = source.DoDragDrop(wxDrag_AllowMove);

	bool handled_internally = pDragDropManager->pDropTarget != 0;

	pDragDropManager->Release();

	if (!handled_internally && (res == wxDragCopy || res == wxDragMove)) {
		// We only need to refresh local side if the operation got handled
		// externally, the internal handlers do this for us already
		m_pState->RefreshLocal();
	}
}

void CLocalListView::RefreshFile(const wxString& file)
{
	CLocalFileData data;

	bool wasLink;
	enum CLocalFileSystem::local_fileType type = CLocalFileSystem::GetFileInfo(m_dir.GetPath() + file, wasLink, &data.size, &data.time, &data.attributes);
	if (type == CLocalFileSystem::unknown)
		return;

	data.name = file;
	data.dir = type == CLocalFileSystem::dir;

	CFilterManager filter;
	if (filter.FilenameFiltered(data.name, m_dir.GetPath(), data.dir, data.size, true, data.attributes, data.time))
		return;

	CancelLabelEdit();

	// Look if file data already exists
	unsigned int i = 0;
	for (auto iter = m_fileData.begin(); iter != m_fileData.end(); ++iter, ++i)
	{
		const CLocalFileData& oldData = *iter;
		if (oldData.name != file)
			continue;

		// Update file list status bar
		if (m_pFilelistStatusBar)
		{
#ifndef __WXMSW__
			// GetNextItem is O(n) if nothing is selected, GetSelectedItemCount() is O(1)
			if (GetSelectedItemCount())
#endif
			{
				int item = -1;
				for (;;)
				{
					item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
					if (item == -1)
						break;
					if (m_indexMapping[item] != i)
						continue;

					if (oldData.dir)
						m_pFilelistStatusBar->UnselectDirectory();
					else
						m_pFilelistStatusBar->UnselectFile(oldData.size);
					if (data.dir)
						m_pFilelistStatusBar->SelectDirectory();
					else
						m_pFilelistStatusBar->SelectFile(data.size);
					break;
				}
			}

			if (oldData.dir)
				m_pFilelistStatusBar->RemoveDirectory();
			else
				m_pFilelistStatusBar->RemoveFile(oldData.size);
			if (data.dir)
				m_pFilelistStatusBar->AddDirectory();
			else
				m_pFilelistStatusBar->AddFile(data.size);
		}

		// Update the data
		data.fileType = oldData.fileType;

		*iter = data;
		if (IsComparing())
		{
			// Sort order doesn't change
			RefreshComparison();
		}
		else
		{
			if (m_sortColumn)
				SortList();
			RefreshListOnly(false);
		}
		return;
	}

	if (data.dir)
		m_pFilelistStatusBar->AddDirectory();
	else
		m_pFilelistStatusBar->AddFile(data.size);

	wxString focused;
	std::list<wxString> selectedNames;
	if (IsComparing())
	{
		wxASSERT(!m_originalIndexMapping.empty());
		selectedNames = RememberSelectedItems(focused);
		m_indexMapping.clear();
		m_originalIndexMapping.swap(m_indexMapping);
	}

	// Insert new entry
	int index = m_fileData.size();
	m_fileData.push_back(data);

	// Find correct position in index mapping
	std::vector<unsigned int>::iterator start = m_indexMapping.begin();
	if (m_hasParent)
		++start;
	CFileListCtrl<CLocalFileData>::CSortComparisonObject compare = GetSortComparisonObject();
	std::vector<unsigned int>::iterator insertPos = std::lower_bound(start, m_indexMapping.end(), index, compare);
	compare.Destroy();

	const int item = insertPos - m_indexMapping.begin();
	m_indexMapping.insert(insertPos, index);

	if (!IsComparing())
	{
		SetItemCount(m_indexMapping.size());

		// Move selections
		int prevState = 0;
		for (unsigned int i = item; i < m_indexMapping.size(); i++)
		{
			int state = GetItemState(i, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
			if (state != prevState)
			{
				SetItemState(i, prevState, wxLIST_STATE_FOCUSED);
				SetSelection(i, (prevState & wxLIST_STATE_SELECTED) != 0);
				prevState = state;
			}
		}
		RefreshListOnly();
	}
	else
	{
		RefreshComparison();
		if (m_pFilelistStatusBar)
			m_pFilelistStatusBar->UnselectAll();
		ReselectItems(selectedNames, focused);
	}
}

wxListItemAttr* CLocalListView::OnGetItemAttr(long item) const
{
	CLocalListView *pThis = const_cast<CLocalListView *>(this);
	const CLocalFileData* const data = pThis->GetData((unsigned int)item);

	if (!data)
		return 0;

#ifndef __WXMSW__
	if (item == m_dropTarget)
		return &pThis->m_dropHighlightAttribute;
#endif

	switch (data->comparison_flags)
	{
	case different:
		return &pThis->m_comparisonBackgrounds[0];
	case lonely:
		return &pThis->m_comparisonBackgrounds[1];
	case newer:
		return &pThis->m_comparisonBackgrounds[2];
	default:
		return 0;
	}
}

void CLocalListView::StartComparison()
{
	if (m_sortDirection || m_sortColumn)
	{
		wxASSERT(m_originalIndexMapping.empty());
		SortList(0, 0);
	}

	ComparisonRememberSelections();

	if (m_originalIndexMapping.empty())
		m_originalIndexMapping.swap(m_indexMapping);
	else
		m_indexMapping.clear();

	m_comparisonIndex = -1;

	const CLocalFileData& last = m_fileData[m_fileData.size() - 1];
	if (last.comparison_flags != fill)
	{
		CLocalFileData data;
		data.dir = false;
		data.icon = -1;
		data.size = -1;
		data.comparison_flags = fill;
		m_fileData.push_back(data);
	}
}

bool CLocalListView::GetNextFile(wxString& name, bool& dir, wxLongLong& size, CDateTime& date)
{
	if (++m_comparisonIndex >= (int)m_originalIndexMapping.size())
		return false;

	const unsigned int index = m_originalIndexMapping[m_comparisonIndex];
	if (index >= m_fileData.size())
		return false;

	const CLocalFileData& data = m_fileData[index];

	name = data.name;
	dir = data.dir;
	size = data.size;
	date = data.time;

	return true;
}

void CLocalListView::FinishComparison()
{
	SetItemCount(m_indexMapping.size());

	ComparisonRestoreSelections();

	RefreshListOnly();

	CComparableListing* pOther = GetOther();
	if (!pOther)
		return;

	pOther->ScrollTopItem(GetTopItem());
}

bool CLocalListView::CanStartComparison(wxString*)
{
	return true;
}

wxString CLocalListView::GetItemText(int item, unsigned int column)
{
	CLocalFileData *data = GetData(item);
	if (!data)
		return wxString();

	if (!column) {
#ifdef __WXMSW__
		return data->label ? *data->label : data->name;
#else
		return data->name;
#endif
	}
	else if (column == 1) {
		if (data->size < 0)
			return wxString();
		else
			return CSizeFormat::Format(data->size);
	}
	else if (column == 2) {
		if (!item && m_hasParent)
			return wxString();

		if (data->comparison_flags == fill)
			return wxString();

		if (data->fileType.empty())
			data->fileType = GetType(data->name, data->dir, m_dir.GetPath());

		return data->fileType;
	}
	else if (column == 3) {
		return CTimeFormat::Format(data->time);
	}
	return wxString();
}

void CLocalListView::OnMenuEdit(wxCommandEvent&)
{
	CServer server;
	CServerPath path;

	if (!m_pState->GetServer()) {
		if (COptions::Get()->GetOptionVal(OPTION_EDIT_TRACK_LOCAL)) {
			wxMessageBoxEx(_("Cannot edit file, not connected to any server."), _("Editing failed"), wxICON_EXCLAMATION);
			return;
		}
	}
	else {
		server = *m_pState->GetServer();

		path = m_pState->GetRemotePath();
		if (path.empty()) {
			wxMessageBoxEx(_("Cannot edit file, remote path unknown."), _("Editing failed"), wxICON_EXCLAMATION);
			return;
		}
	}

	std::vector<CEditHandler::FileData> selected_item;

	long item = -1;
	while ((item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
		if (!item && m_hasParent) {
			wxBell();
			return;
		}

		const CLocalFileData *data = GetData(item);
		if (!data)
			continue;

		if (data->dir) {
			wxBell();
			return;
		}

		if (data->comparison_flags == fill)
			continue;

		selected_item.push_back({m_dir.GetPath() + data->name, data->size});
	}

	CEditHandler* pEditHandler = CEditHandler::Get();
	pEditHandler->Edit(CEditHandler::local, selected_item, path, server, this);
}

void CLocalListView::OnMenuOpen(wxCommandEvent&)
{
	long item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item == -1) {
		OpenInFileManager(m_dir.GetPath());
		return;
	}

	std::list<CLocalFileData> selected_item_list;

	item = -1;
	while ((item = GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
		if (!item && m_hasParent) {
			wxBell();
			return;
		}

		const CLocalFileData *data = GetData(item);
		if (!data)
			continue;

		if (data->comparison_flags == fill)
			continue;

		selected_item_list.push_back(*data);
	}

	CEditHandler* pEditHandler = CEditHandler::Get();
	if (!pEditHandler) {
		wxBell();
		return;
	}

	if (selected_item_list.empty()) {
		wxBell();
		return;
	}

	if (selected_item_list.size() > 10) {
		CConditionalDialog dlg(this, CConditionalDialog::many_selected_for_edit, CConditionalDialog::yesno);
		dlg.SetTitle(_("Confirmation needed"));
		dlg.AddText(_("You have selected more than 10 files or directories to open, do you really want to continue?"));

		if (!dlg.Run())
			return;
	}

	for (auto const& data : selected_item_list) {
		if (data.dir) {
			CLocalPath path(m_dir);
			if (!path.ChangePath(data.name)) {
				wxBell();
				continue;
			}

			OpenInFileManager(path.GetPath());
			continue;
		}

		wxFileName fn(m_dir.GetPath(), data.name);
		if (wxLaunchDefaultApplication(fn.GetFullPath(), 0)) {
			continue;
		}
		bool program_exists = false;
		wxString cmd = GetSystemOpenCommand(fn.GetFullPath(), program_exists);
		if (cmd.empty()) {
			int pos = data.name.Find('.') == -1;
			if (pos == -1 || (pos == 0 && data.name.Mid(1).Find('.') == -1))
				cmd = pEditHandler->GetOpenCommand(fn.GetFullPath(), program_exists);
		}
		if (cmd.empty()) {
			wxMessageBoxEx(wxString::Format(_("The file '%s' could not be opened:\nNo program has been associated on your system with this file type."), fn.GetFullPath()), _("Opening failed"), wxICON_EXCLAMATION);
			continue;
		}
		if (!program_exists) {
			wxString msg = wxString::Format(_("The file '%s' cannot be opened:\nThe associated program (%s) could not be found.\nPlease check your filetype associations."), fn.GetFullPath(), cmd);
			wxMessageBoxEx(msg, _("Cannot edit file"), wxICON_EXCLAMATION);
			continue;
		}

		if (wxExecute(cmd))
			continue;

		wxMessageBoxEx(wxString::Format(_("The file '%s' could not be opened:\nThe associated command failed"), fn.GetFullPath()), _("Opening failed"), wxICON_EXCLAMATION);
	}
}

bool CLocalListView::ItemIsDir(int index) const
{
	return m_fileData[index].dir;
}

wxLongLong CLocalListView::ItemGetSize(int index) const
{
	return m_fileData[index].size;
}

#ifdef __WXMSW__

void CLocalListView::OnVolumesEnumerated(wxCommandEvent& event)
{
	if (!m_pVolumeEnumeratorThread)
		return;

	std::list<CVolumeDescriptionEnumeratorThread::t_VolumeInfo> volumeInfo;
	volumeInfo = m_pVolumeEnumeratorThread->GetVolumes();

	if (event.GetEventType() == fzEVT_VOLUMESENUMERATED) {
		delete m_pVolumeEnumeratorThread;
		m_pVolumeEnumeratorThread = 0;
	}

	if (m_dir.GetPath() != _T("\\"))
		return;

	for (std::list<CVolumeDescriptionEnumeratorThread::t_VolumeInfo>::const_iterator iter = volumeInfo.begin(); iter != volumeInfo.end(); ++iter) {
		wxString drive = iter->volume;

		unsigned int item, index;
		for (item = 1; item < m_indexMapping.size(); ++item) {
			index = m_indexMapping[item];
			if (m_fileData[index].name == drive || m_fileData[index].name.Left(drive.Len() + 1) == drive + _T(" "))
				break;
		}
		if (item >= m_indexMapping.size())
			continue;

		m_fileData[index].label = CSparseOptional<wxString>(drive + _T(" (") + iter->volumeName + _T(")"));

		RefreshItem(item);
	}
}

#endif

void CLocalListView::OnMenuRefresh(wxCommandEvent&)
{
	m_pState->RefreshLocal();
}

void CLocalListView::OnNavigationEvent(bool forward)
{
	if (!forward)
	{
		if (!m_hasParent)
		{
			wxBell();
			return;
		}

		wxString error;
		if (!m_pState->SetLocalDir(_T(".."), &error))
		{
			if (!error.empty())
				wxMessageBoxEx(error, _("Failed to change directory"), wxICON_INFORMATION);
			else
				wxBell();
		}
	}
}
