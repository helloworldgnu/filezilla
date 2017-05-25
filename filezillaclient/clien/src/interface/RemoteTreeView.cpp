#include <filezilla.h>
#include "RemoteTreeView.h"
#include "commandqueue.h"
#include <wx/dnd.h>
#include "dndobjects.h"
#include "chmoddialog.h"
#include "recursive_operation.h"
#include "inputdialog.h"
#include "dragdropmanager.h"
#include "drop_target_ex.h"
#include <wx/clipbrd.h>
#include "queue.h"
#include "QueueView.h"
#include "themeprovider.h"
#include "Options.h"

class CItemData : public wxTreeItemData
{
public:
	CItemData(CServerPath path) : m_path(path) {}
	CServerPath m_path;
};

class CRemoteTreeViewDropTarget : public CScrollableDropTarget<wxTreeCtrlEx>
{
public:
	CRemoteTreeViewDropTarget(CRemoteTreeView* pRemoteTreeView)
		: CScrollableDropTarget<wxTreeCtrlEx>(pRemoteTreeView)
		, m_pRemoteTreeView(pRemoteTreeView)
		, m_pFileDataObject(new wxFileDataObject())
		, m_pRemoteDataObject(new CRemoteDataObject())
	{
		m_pDataObject = new wxDataObjectComposite;
		m_pDataObject->Add(m_pRemoteDataObject, true);
		m_pDataObject->Add(m_pFileDataObject, false);
		SetDataObject(m_pDataObject);
	}

	void ClearDropHighlight()
	{
		const wxTreeItemId dropHighlight = m_pRemoteTreeView->m_dropHighlight;
		if (dropHighlight != wxTreeItemId()) {
			m_pRemoteTreeView->SetItemDropHighlight(dropHighlight, false);
			m_pRemoteTreeView->m_dropHighlight = wxTreeItemId();
		}
	}

	wxTreeItemId GetHit(const wxPoint& point)
	{
		int flags = 0;
		wxTreeItemId hit = m_pRemoteTreeView->HitTest(point, flags);

		if (flags & (wxTREE_HITTEST_ABOVE | wxTREE_HITTEST_BELOW | wxTREE_HITTEST_NOWHERE | wxTREE_HITTEST_TOLEFT | wxTREE_HITTEST_TORIGHT))
			return wxTreeItemId();

		return hit;
	}

	virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def)
	{
		if (def == wxDragError ||
			def == wxDragNone ||
			def == wxDragCancel)
			return def;

		wxTreeItemId hit = GetHit(wxPoint(x, y));
		if (!hit)
			return wxDragNone;

		CServerPath path = m_pRemoteTreeView->GetPathFromItem(hit);
		if (path.empty())
			return wxDragNone;

		if (!GetData())
			return wxDragError;

		CDragDropManager* pDragDropManager = CDragDropManager::Get();
		if (pDragDropManager)
			pDragDropManager->pDropTarget = m_pRemoteTreeView;

		if (m_pDataObject->GetReceivedFormat() == m_pFileDataObject->GetFormat())
			m_pRemoteTreeView->m_pState->UploadDroppedFiles(m_pFileDataObject, path, false);
		else
		{
			if (m_pRemoteDataObject->GetProcessId() != (int)wxGetProcessId())
			{
				wxMessageBoxEx(_("Drag&drop between different instances of FileZilla has not been implemented yet."));
				return wxDragNone;
			}

			if (!m_pRemoteTreeView->m_pState->GetServer() || !m_pRemoteDataObject->GetServer().EqualsNoPass(*m_pRemoteTreeView->m_pState->GetServer()))
			{
				wxMessageBoxEx(_("Drag&drop between different servers has not been implemented yet."));
				return wxDragNone;
			}

			// Make sure path path is valid
			if (path == m_pRemoteDataObject->GetServerPath())
			{
				wxMessageBoxEx(_("Source and path of the drop operation are identical"));
				return wxDragNone;
			}

			const std::list<CRemoteDataObject::t_fileInfo>& files = m_pRemoteDataObject->GetFiles();
			for (std::list<CRemoteDataObject::t_fileInfo>::const_iterator iter = files.begin(); iter != files.end(); ++iter)
			{
				const CRemoteDataObject::t_fileInfo& info = *iter;
				if (info.dir)
				{
					CServerPath dir = m_pRemoteDataObject->GetServerPath();
					dir.AddSegment(info.name);
					if (dir == path)
						return wxDragNone;
					else if (dir.IsParentOf(path, false))
					{
						wxMessageBoxEx(_("A directory cannot be dragged into one of its subdirectories."));
						return wxDragNone;
					}
				}
			}

			for (std::list<CRemoteDataObject::t_fileInfo>::const_iterator iter = files.begin(); iter != files.end(); ++iter)
			{
				const CRemoteDataObject::t_fileInfo& info = *iter;
				m_pRemoteTreeView->m_pState->m_pCommandQueue->ProcessCommand(
					new CRenameCommand(m_pRemoteDataObject->GetServerPath(), info.name, path, info.name)
					);
			}

			return wxDragNone;
		}

		return def;
	}

	virtual bool OnDrop(wxCoord x, wxCoord y)
	{
		if (!CScrollableDropTarget<wxTreeCtrlEx>::OnDrop(x, y)) {
			return false;
		}
		ClearDropHighlight();

		wxTreeItemId hit = GetHit(wxPoint(x, y));
		if (!hit)
			return false;

		const CServerPath& path = m_pRemoteTreeView->GetPathFromItem(hit);
		if (path.empty())
			return false;

		return true;
	}

	wxTreeItemId DisplayDropHighlight(wxPoint point)
	{
		wxTreeItemId hit = GetHit(point);
		if (!hit) {
			ClearDropHighlight();
			return wxTreeItemId();
		}

		const CServerPath& path = m_pRemoteTreeView->GetPathFromItem(hit);

		if (path.empty()) {
			ClearDropHighlight();
			return wxTreeItemId();
		}

		const wxTreeItemId dropHighlight = m_pRemoteTreeView->m_dropHighlight;
		if (dropHighlight != wxTreeItemId())
			m_pRemoteTreeView->SetItemDropHighlight(dropHighlight, false);

		m_pRemoteTreeView->SetItemDropHighlight(hit, true);
		m_pRemoteTreeView->m_dropHighlight = hit;

		return hit;
	}

	virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def)
	{
		def = CScrollableDropTarget<wxTreeCtrlEx>::OnDragOver(x, y, def);

		if (def == wxDragError ||
			def == wxDragNone ||
			def == wxDragCancel)
		{
			ClearDropHighlight();
			return def;
		}

		wxTreeItemId hit = DisplayDropHighlight(wxPoint(x, y));
		if (!hit.IsOk())
			return wxDragNone;

		if (def == wxDragLink)
			def = wxDragCopy;

		return def;
	}

	virtual void OnLeave()
	{
		CScrollableDropTarget<wxTreeCtrlEx>::OnLeave();
		ClearDropHighlight();
	}

	virtual wxDragResult OnEnter(wxCoord x, wxCoord y, wxDragResult def)
	{
		def = CScrollableDropTarget<wxTreeCtrlEx>::OnEnter(x, y, def);
		return OnDragOver(x, y, def);
	}

protected:
	CRemoteTreeView *m_pRemoteTreeView;
	wxFileDataObject* m_pFileDataObject;
	CRemoteDataObject* m_pRemoteDataObject;
	wxDataObjectComposite* m_pDataObject;
};

IMPLEMENT_CLASS(CRemoteTreeView, wxTreeCtrlEx)

BEGIN_EVENT_TABLE(CRemoteTreeView, wxTreeCtrlEx)
EVT_TREE_ITEM_EXPANDING(wxID_ANY, CRemoteTreeView::OnItemExpanding)
EVT_TREE_SEL_CHANGED(wxID_ANY, CRemoteTreeView::OnSelectionChanged)
EVT_TREE_ITEM_ACTIVATED(wxID_ANY, CRemoteTreeView::OnItemActivated)
EVT_TREE_BEGIN_DRAG(wxID_ANY, CRemoteTreeView::OnBeginDrag)
EVT_TREE_ITEM_MENU(wxID_ANY, CRemoteTreeView::OnContextMenu)
EVT_MENU(XRCID("ID_CHMOD"), CRemoteTreeView::OnMenuChmod)
EVT_MENU(XRCID("ID_DOWNLOAD"), CRemoteTreeView::OnMenuDownload)
EVT_MENU(XRCID("ID_ADDTOQUEUE"), CRemoteTreeView::OnMenuDownload)
EVT_MENU(XRCID("ID_DELETE"), CRemoteTreeView::OnMenuDelete)
EVT_MENU(XRCID("ID_RENAME"), CRemoteTreeView::OnMenuRename)
EVT_TREE_BEGIN_LABEL_EDIT(wxID_ANY, CRemoteTreeView::OnBeginLabelEdit)
EVT_TREE_END_LABEL_EDIT(wxID_ANY, CRemoteTreeView::OnEndLabelEdit)
EVT_MENU(XRCID("ID_MKDIR"), CRemoteTreeView::OnMkdir)
EVT_MENU(XRCID("ID_MKDIR_CHGDIR"), CRemoteTreeView::OnMenuMkdirChgDir)
EVT_CHAR(CRemoteTreeView::OnChar)
EVT_MENU(XRCID("ID_GETURL"), CRemoteTreeView::OnMenuGeturl)
END_EVENT_TABLE()

CRemoteTreeView::CRemoteTreeView(wxWindow* parent, wxWindowID id, CState* pState, CQueueView* pQueue)
	: wxTreeCtrlEx(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxTR_EDIT_LABELS | wxTR_LINES_AT_ROOT | wxTR_HAS_BUTTONS | wxNO_BORDER | wxTR_HIDE_ROOT),
	CSystemImageList(16),
	CStateEventHandler(pState)
{
#ifdef __WXMAC__
	SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
#endif

	pState->RegisterHandler(this, STATECHANGE_REMOTE_DIR);
	pState->RegisterHandler(this, STATECHANGE_REMOTE_DIR_MODIFIED);
	pState->RegisterHandler(this, STATECHANGE_APPLYFILTER);

	CreateImageList();

	UpdateSortMode();
	RegisterOption(OPTION_FILELIST_NAMESORT);

	m_busy = false;
	m_pQueue = pQueue;
	AddRoot(_T(""));
	m_ExpandAfterList = wxTreeItemId();

	SetDropTarget(new CRemoteTreeViewDropTarget(this));

	Enable(false);
}

CRemoteTreeView::~CRemoteTreeView()
{
	SetImageList(0);
	delete m_pImageList;
}

void CRemoteTreeView::OnStateChange(CState* pState, enum t_statechange_notifications notification, const wxString&, const void*)
{
	if (notification == STATECHANGE_REMOTE_DIR)
		SetDirectoryListing(pState->GetRemoteDir(), false);
	else if (notification == STATECHANGE_REMOTE_DIR_MODIFIED)
		SetDirectoryListing(pState->GetRemoteDir(), true);
	else if (notification == STATECHANGE_APPLYFILTER)
		ApplyFilters(false);
}

void CRemoteTreeView::SetDirectoryListing(std::shared_ptr<CDirectoryListing> const& pListing, bool modified)
{
	m_busy = true;

	if (!pListing) {
		m_ExpandAfterList = wxTreeItemId();
		DeleteAllItems();
		AddRoot(_T(""));
		m_busy = false;
		if (FindFocus() == this) {
			wxNavigationKeyEvent *evt = new wxNavigationKeyEvent();
			evt->SetFromTab(true);
			evt->SetEventObject(this);
			evt->SetDirection(true);
			QueueEvent(evt);
		}
		Enable(false);
		m_contextMenuItem = wxTreeItemId();
		return;
	}
	Enable(true);
#ifdef __WXGTK__
	GetParent()->m_dirtyTabOrder = true;
#endif

	if (pListing->get_unsure_flags() && !(pListing->get_unsure_flags() & ~(CDirectoryListing::unsure_unknown | CDirectoryListing::unsure_file_mask))) {
		// Just files changed, does not affect directory tree
		m_busy = false;
		return;
	}

#ifndef __WXMSW__
	Freeze();
#endif
	wxTreeItemId parent = MakeParent(pListing->path, !modified);
	if (!parent)
	{
		m_busy = false;
#ifndef __WXMSW__
		Thaw();
#endif
		return;
	}

	if (!IsExpanded(parent) && parent != m_ExpandAfterList)
	{
		DeleteChildren(parent);
		CFilterManager filter;
		if (HasSubdirs(*pListing, filter))
			AppendItem(parent, _T(""), -1, -1);
	}
	else
	{
		RefreshItem(parent, *pListing, !modified);

		if (m_ExpandAfterList == parent)
		{
#ifndef __WXMSW__
			// Prevent CalculatePositions from being called
			wxGenericTreeItem *anchor = m_anchor;
			m_anchor = 0;
#endif
			Expand(parent);
#ifndef __WXMSW__
			m_anchor = anchor;
#endif
		}
	}
	m_ExpandAfterList = wxTreeItemId();

	SetItemImages(parent, false);

#ifndef __WXMSW__
	Thaw();
#endif
	if (!modified)
		SafeSelectItem(parent);
#ifndef __WXMSW__
	else
		Refresh();
#endif

	m_busy = false;
}

wxTreeItemId CRemoteTreeView::MakeParent(CServerPath path, bool select)
{
	std::list<wxString> pieces;
	while (path.HasParent())
	{
		pieces.push_front(path.GetLastSegment());
		path = path.GetParent();
	}
	wxASSERT(!path.GetPath().empty());
	pieces.push_front(path.GetPath());

	const wxTreeItemId root = GetRootItem();
	wxTreeItemId parent = root;

	for (std::list<wxString>::const_iterator iter = pieces.begin(); iter != pieces.end(); ++iter)
	{
		if (iter != pieces.begin())
			path.AddSegment(*iter);

		wxTreeItemIdValue cookie;
		wxTreeItemId child = GetFirstChild(parent, cookie);
		if (child && GetItemText(child).empty())
		{
			Delete(child);
			child = wxTreeItemId();
			if (parent != root)
				ListExpand(parent);
		}
		for (child = GetFirstChild(parent, cookie); child; child = GetNextSibling(child))
		{
			const wxString& text = GetItemText(child);
			if (text == *iter)
				break;
		}
		if (!child)
		{
			CDirectoryListing listing;

			if (m_pState->m_pEngine->CacheLookup(path, listing) == FZ_REPLY_OK)
			{
				child = AppendItem(parent, *iter, 0, 2, path.HasParent() ? 0 : new CItemData(path));
				SetItemImages(child, false);
			}
			else
			{
				child = AppendItem(parent, *iter, 1, 3, path.HasParent() ? 0 : new CItemData(path));
				SetItemImages(child, true);
			}
			SortChildren(parent);

			std::list<wxString>::const_iterator nextIter = iter;
			++nextIter;
			if (nextIter != pieces.end())
				DisplayItem(child, listing);
		}
		if (select && iter != pieces.begin())
		{
#ifndef __WXMSW__
			// Prevent CalculatePositions from being called
			wxGenericTreeItem *anchor = m_anchor;
			m_anchor = 0;
#endif
			Expand(parent);
#ifndef __WXMSW__
			m_anchor = anchor;
#endif
		}

		parent = child;
	}

	return parent;
}

wxBitmap CRemoteTreeView::CreateIcon(int index, const wxString& overlay /*=_T("")*/)
{
	// Create memory DC
	wxSize s = CThemeProvider::GetIconSize(iconSizeSmall);
#ifdef __WXMSW__
	wxBitmap bmp(s.x, s.y, 32);
#else
	wxBitmap bmp(s.x, s.y, 24);
#endif
	wxMemoryDC dc;
	dc.SelectObject(bmp);

	// Make sure the background is set correctly
	dc.SetBrush(wxBrush(GetBackgroundColour()));
	dc.SetPen(wxPen(GetBackgroundColour()));
	dc.DrawRectangle(0, 0, s.x, s.y);

	// Draw item from system image list
	GetSystemImageList()->Draw(index, dc, 0, 0, wxIMAGELIST_DRAW_TRANSPARENT);

	// Load overlay
	if (!overlay.empty())
	{
		wxImage unknownIcon = wxArtProvider::GetBitmap(overlay, wxART_OTHER, CThemeProvider::GetIconSize(iconSizeSmall)).ConvertToImage();

		// Convert mask into alpha channel
		if (unknownIcon.IsOk() && !unknownIcon.HasAlpha())
		{
			wxASSERT(unknownIcon.HasMask());
			unknownIcon.InitAlpha();
		}

		// Draw overlay
		dc.DrawBitmap(unknownIcon, 0, 0, true);
	}

	dc.SelectObject(wxNullBitmap);
	return bmp;
}

void CRemoteTreeView::CreateImageList()
{
	wxSize s = CThemeProvider::GetIconSize(iconSizeSmall);
	m_pImageList = new wxImageList(s.x, s.y, true, 4);

	// Normal directory
	int index = GetIconIndex(iconType::dir, _T("{78013B9C-3532-4fe1-A418-5CD1955127CC}"), false);
	m_pImageList->Add(CreateIcon(index));
	m_pImageList->Add(CreateIcon(index, _T("ART_UNKNOWN")));

	// Opened directory
	index = GetIconIndex(iconType::opened_dir, _T("{78013B9C-3532-4fe1-A418-5CD1955127CC}"), false);
	m_pImageList->Add(CreateIcon(index));
	m_pImageList->Add(CreateIcon(index, _T("ART_UNKNOWN")));

	SetImageList(m_pImageList);
}

bool CRemoteTreeView::HasSubdirs(const CDirectoryListing& listing, const CFilterManager& filter)
{
	if (!listing.has_dirs())
		return false;

	if (!filter.HasActiveFilters())
		return true;

	const wxString path = listing.path.GetPath();
	for (unsigned int i = 0; i < listing.GetCount(); i++)
	{
		if (!listing[i].is_dir())
			continue;

		if (filter.FilenameFiltered(listing[i].name, path, true, -1, false, 0, listing[i].time))
			continue;

		return true;
	}

	return false;
}

void CRemoteTreeView::DisplayItem(wxTreeItemId parent, const CDirectoryListing& listing)
{
	DeleteChildren(parent);

	const wxString path = listing.path.GetPath();

	CFilterDialog filter;
	for (unsigned int i = 0; i < listing.GetCount(); i++)
	{
		if (!listing[i].is_dir())
			continue;

		if (filter.FilenameFiltered(listing[i].name, path, true, -1, false, 0, listing[i].time))
			continue;

		const wxString& name = listing[i].name;
		CServerPath subdir = listing.path;
		subdir.AddSegment(name);

		CDirectoryListing subListing;

		if (m_pState->m_pEngine->CacheLookup(subdir, subListing) == FZ_REPLY_OK)
		{
			wxTreeItemId child = AppendItem(parent, name, 0, 2, 0);
			SetItemImages(child, false);

			if (HasSubdirs(subListing, filter))
				AppendItem(child, _T(""), -1, -1);
		}
		else
		{
			wxTreeItemId child = AppendItem(parent, name, 1, 3, 0);
			SetItemImages(child, true);
		}
	}
	SortChildren(parent);
}

void CRemoteTreeView::RefreshItem(wxTreeItemId parent, const CDirectoryListing& listing, bool will_select_parent)
{
	SetItemImages(parent, false);

	wxTreeItemIdValue cookie;
	wxTreeItemId child = GetFirstChild(parent, cookie);
	if (!child || GetItemText(child).empty())
	{
		DisplayItem(parent, listing);
		return;
	}

	CFilterManager filter;

	const wxString path = listing.path.GetPath();

	wxArrayString dirs;
	for (unsigned int i = 0; i < listing.GetCount(); i++)
	{
		if (!listing[i].is_dir())
			continue;

		if (!filter.FilenameFiltered(listing[i].name, path, true, -1, false, 0, listing[i].time))
			dirs.push_back(listing[i].name);
	}

	auto const& sortFunc = CFileListCtrlSortBase::GetCmpFunction(m_nameSortMode);
	dirs.Sort(sortFunc);

	bool inserted = false;
	child = GetLastChild(parent);
	wxArrayString::reverse_iterator iter = dirs.rbegin();
	while (child && iter != dirs.rend()) {
		int cmp = sortFunc(GetItemText(child), *iter);

		if (!cmp) {
			CServerPath path = listing.path;
			path.AddSegment(*iter);

			CDirectoryListing subListing;
			if (m_pState->m_pEngine->CacheLookup(path, subListing) == FZ_REPLY_OK)
			{
				if (!GetLastChild(child) && HasSubdirs(subListing, filter))
					AppendItem(child, _T(""), -1, -1);
				SetItemImages(child, false);
			}
			else
				SetItemImages(child, true);

			child = GetPrevSibling(child);
			++iter;
		}
		else if (cmp > 0) {
			// Child no longer exists
			wxTreeItemId sel = GetSelection();
			while (sel && sel != child)
				sel = GetItemParent(sel);
			wxTreeItemId prev = GetPrevSibling(child);
			if (!sel || will_select_parent)
				Delete(child);
			child = prev;
		}
		else if (cmp < 0) {
			// New directory
			CServerPath path = listing.path;
			path.AddSegment(*iter);

			CDirectoryListing subListing;
			if (m_pState->m_pEngine->CacheLookup(path, subListing) == FZ_REPLY_OK) {
				wxTreeItemId child = AppendItem(parent, *iter, 0, 2, 0);
				SetItemImages(child, false);

				if (HasSubdirs(subListing, filter))
					AppendItem(child, _T(""), -1, -1);
			}
			else {
				wxTreeItemId child = AppendItem(parent, *iter, 1, 3, 0);
				if (child)
					SetItemImages(child, true);
			}

			++iter;
			inserted = true;
		}
	}
	while (child) {
		// Child no longer exists
		wxTreeItemId sel = GetSelection();
		while (sel && sel != child)
			sel = GetItemParent(sel);
		wxTreeItemId prev = GetPrevSibling(child);
		if (!sel || will_select_parent)
			Delete(child);
		child = prev;
	}
	while (iter != dirs.rend()) {
		CServerPath path = listing.path;
		path.AddSegment(*iter);

		CDirectoryListing subListing;
		if (m_pState->m_pEngine->CacheLookup(path, subListing) == FZ_REPLY_OK) {
			wxTreeItemId child = AppendItem(parent, *iter, 0, 2, 0);
			SetItemImages(child, false);

			if (HasSubdirs(subListing, filter))
				AppendItem(child, _T(""), -1, -1);
		}
		else {
			wxTreeItemId child = AppendItem(parent, *iter, 1, 3, 0);
			SetItemImages(child, true);
		}

		++iter;
		inserted = true;
	}

	if (inserted)
		SortChildren(parent);
}

void CRemoteTreeView::OnItemExpanding(wxTreeEvent& event)
{
	if (m_busy)
		return;

	wxTreeItemId item = event.GetItem();
	if (!item)
		return;

	if (!ListExpand(item))
	{
		event.Veto();
		return;
	}

	Refresh(false);
}

void CRemoteTreeView::SetItemImages(wxTreeItemId item, bool unknown)
{
	const int old_image = GetItemImage(item, wxTreeItemIcon_Normal);
	if (!unknown)
	{
		if (old_image == 0)
			return;
		SetItemImage(item, 0, wxTreeItemIcon_Normal);
		SetItemImage(item, 2, wxTreeItemIcon_Selected);
		SetItemImage(item, 0, wxTreeItemIcon_Expanded);
		SetItemImage(item, 2, wxTreeItemIcon_SelectedExpanded);
	}
	else
	{
		if (old_image == 1)
			return;
		SetItemImage(item, 1, wxTreeItemIcon_Normal);
		SetItemImage(item, 3, wxTreeItemIcon_Selected);
		SetItemImage(item, 1, wxTreeItemIcon_Expanded);
		SetItemImage(item, 3, wxTreeItemIcon_SelectedExpanded);
	}
}

void CRemoteTreeView::OnSelectionChanged(wxTreeEvent& event)
{
	if (event.GetItem() != m_ExpandAfterList)
		m_ExpandAfterList = wxTreeItemId();
	if (m_busy)
		return;

	if (!m_pState->IsRemoteIdle())
	{
		wxBell();
		return;
	}

	wxTreeItemId item = event.GetItem();
	if (!item)
		return;

	const CServerPath path = GetPathFromItem(item);
	wxASSERT(!path.empty());
	if (path.empty())
		return;

	m_pState->ChangeRemoteDir(path);
}

void CRemoteTreeView::OnItemActivated(wxTreeEvent& event)
{
	m_ExpandAfterList = GetSelection();
	event.Skip();
}

CServerPath CRemoteTreeView::GetPathFromItem(const wxTreeItemId& item) const
{
	std::list<wxString> segments;

	wxTreeItemId i = item;
	while (i != GetRootItem())
	{
		const CItemData* const pData = (const CItemData*)GetItemData(i);
		if (pData)
		{
			CServerPath path = pData->m_path;
			for (std::list<wxString>::const_iterator iter = segments.begin(); iter != segments.end(); ++iter)
			{
				if (!path.AddSegment(*iter))
					return CServerPath();
			}
			return path;
		}

		segments.push_front(GetItemText(i));
		i = GetItemParent(i);
	}

	return CServerPath();
}

void CRemoteTreeView::OnBeginDrag(wxTreeEvent& event)
{
	// Drag could result in recursive operation, don't allow at this point
	if (!m_pState->IsRemoteIdle())
	{
		wxBell();
		return;
	}

	const wxTreeItemId& item = event.GetItem();
	if (!item)
		return;

	const CServerPath& path = GetPathFromItem(item);
	if (path.empty() || !path.HasParent())
		return;

	const CServerPath& parent = path.GetParent();
	const wxString& lastSegment = path.GetLastSegment();
	if (lastSegment.empty())
		return;

	wxDataObjectComposite object;

	const CServer* const pServer = m_pState->GetServer();
	if (!pServer)
		return;

	CRemoteDataObject *pRemoteDataObject = new CRemoteDataObject(*pServer, parent);
	pRemoteDataObject->AddFile(lastSegment, true, -1, false);

	pRemoteDataObject->Finalize();

	object.Add(pRemoteDataObject, true);

#if FZ3_USESHELLEXT
	CShellExtensionInterface* ext = CShellExtensionInterface::CreateInitialized();
	if (ext)
	{
		const wxString& file = ext->GetDragDirectory();

		wxASSERT(!file.empty());

		wxFileDataObject *pFileDataObject = new wxFileDataObject;
		pFileDataObject->AddFile(file);

		object.Add(pFileDataObject);
	}
#endif

	CDragDropManager* pDragDropManager = CDragDropManager::Init();
	pDragDropManager->pDragSource = this;
	pDragDropManager->server = *pServer;
	pDragDropManager->remoteParent = parent;

	wxDropSource source(this);
	source.SetData(object);

	int res = source.DoDragDrop();

	pDragDropManager->Release();

	if (res != wxDragCopy)
	{
#if FZ3_USESHELLEXT
		delete ext;
		ext = 0;
#endif
		return;
	}

#if FZ3_USESHELLEXT
	if (ext)
	{
		if (!pRemoteDataObject->DidSendData())
		{
			const CServer* const pServer = m_pState->GetServer();
			if (!pServer || !m_pState->IsRemoteIdle())
			{
				wxBell();
				delete ext;
				ext = 0;
				return;
			}

			CLocalPath target(ext->GetTarget());
			if (target.empty())
			{
				delete ext;
				ext = 0;
				wxMessageBoxEx(_("Could not determine the target of the Drag&Drop operation.\nEither the shell extension is not installed properly or you didn't drop the files into an Explorer window."));
				return;
			}

			m_pState->DownloadDroppedFiles(pRemoteDataObject, target);

			delete ext;
			ext = 0;
			return;
		}
		delete ext;
		ext = 0;
	}
#endif
}

void CRemoteTreeView::OnContextMenu(wxTreeEvent& event)
{
	m_contextMenuItem = event.GetItem();
	wxMenu* pMenu = wxXmlResource::Get()->LoadMenu(_T("ID_MENU_REMOTETREE"));
	if (!pMenu)
		return;

	const CServerPath& path = m_contextMenuItem ? GetPathFromItem(m_contextMenuItem) : CServerPath();
	if (!m_pState->IsRemoteIdle() || path.empty())
	{
		pMenu->Enable(XRCID("ID_DOWNLOAD"), false);
		pMenu->Enable(XRCID("ID_ADDTOQUEUE"), false);
		pMenu->Enable(XRCID("ID_MKDIR"), false);
		pMenu->Enable(XRCID("ID_MKDIR_CHGDIR"), false);
		pMenu->Enable(XRCID("ID_DELETE"), false);
		pMenu->Enable(XRCID("ID_CHMOD"), false);
		pMenu->Enable(XRCID("ID_RENAME"), false);
		pMenu->Enable(XRCID("ID_GETURL"), false);
	}
	else if (!path.HasParent())
		pMenu->Enable(XRCID("ID_RENAME"), false);

	if (!m_pState->GetLocalDir().IsWriteable())
	{
		pMenu->Enable(XRCID("ID_DOWNLOAD"), false);
		pMenu->Enable(XRCID("ID_ADDTOQUEUE"), false);
	}

	PopupMenu(pMenu);
	delete pMenu;
}

void CRemoteTreeView::OnMenuChmod(wxCommandEvent&)
{
	if (!m_pState->IsRemoteIdle())
		return;

	if (!m_contextMenuItem)
		return;

	const CServerPath& path = GetPathFromItem(m_contextMenuItem);
	if (path.empty())
		return;

	const bool hasParent = path.HasParent();

	CChmodDialog* pChmodDlg = new CChmodDialog;

	// Get current permissions of directory
	const wxString& name = GetItemText(m_contextMenuItem);
	char permissions[9] = {0};
	bool cached = false;

	// Obviously item needs to have a parent directory...
	if (hasParent)
	{
		const CServerPath& parentPath = path.GetParent();
		CDirectoryListing listing;

		// ... and it needs to be cached
		cached = m_pState->m_pEngine->CacheLookup(parentPath, listing) == FZ_REPLY_OK;
		if (cached)
		{
			for (unsigned int i = 0; i < listing.GetCount(); i++)
			{
				if (listing[i].name != name)
					continue;

				pChmodDlg->ConvertPermissions(*listing[i].permissions, permissions);
			}
		}
	}

	if (!pChmodDlg->Create(this, 0, 1, name, permissions))
	{
		pChmodDlg->Destroy();
		pChmodDlg = 0;
		return;
	}

	if (pChmodDlg->ShowModal() != wxID_OK)
	{
		pChmodDlg->Destroy();
		pChmodDlg = 0;
		return;
	}

	// State may have changed while chmod dialog was shown
	if (!m_contextMenuItem || !m_pState->IsRemoteConnected() || !m_pState->IsRemoteIdle())
	{
		pChmodDlg->Destroy();
		pChmodDlg = 0;
		return;
	}

	const int applyType = pChmodDlg->GetApplyType();

	CRecursiveOperation* pRecursiveOperation = m_pState->GetRecursiveOperationHandler();

	if (cached) // Implies hasParent
	{
		// Change directory permissions
		if (!applyType || applyType == 2)
		{
			wxString newPerms = pChmodDlg->GetPermissions(permissions, true);

			m_pState->m_pCommandQueue->ProcessCommand(new CChmodCommand(path.GetParent(), name, newPerms));
		}

		if (pChmodDlg->Recursive())
			// Start recursion
			pRecursiveOperation->AddDirectoryToVisit(path, _T(""), CLocalPath());
	}
	else
	{
		if (hasParent)
			pRecursiveOperation->AddDirectoryToVisitRestricted(path.GetParent(), name, pChmodDlg->Recursive());
		else
			pRecursiveOperation->AddDirectoryToVisitRestricted(path, _T(""), pChmodDlg->Recursive());
	}

	if (!cached || pChmodDlg->Recursive())
	{
		pRecursiveOperation->SetChmodDialog(pChmodDlg);

		CServerPath currentPath;
		const wxTreeItemId selected = GetSelection();
		if (selected)
			currentPath = GetPathFromItem(selected);
		CFilterManager filter;
		pRecursiveOperation->StartRecursiveOperation(CRecursiveOperation::recursive_chmod, hasParent ? path.GetParent() : path, filter.GetActiveFilters(false), !cached, currentPath);
	}
	else
	{
		pChmodDlg->Destroy();
		const wxTreeItemId selected = GetSelection();
		if (selected)
		{
			CServerPath currentPath = GetPathFromItem(selected);
			m_pState->ChangeRemoteDir(currentPath);
		}
	}
}

void CRemoteTreeView::OnMenuDownload(wxCommandEvent& event)
{
	CLocalPath localDir = m_pState->GetLocalDir();
	if (!localDir.IsWriteable())
	{
		wxBell();
		return;
	}

	if (!m_pState->IsRemoteIdle())
		return;

	if (!m_contextMenuItem)
		return;

	const CServerPath& path = GetPathFromItem(m_contextMenuItem);
	if (path.empty())
		return;

	const wxString& name = GetItemText(m_contextMenuItem);

	localDir.AddSegment(CQueueView::ReplaceInvalidCharacters(name));

	CRecursiveOperation* pRecursiveOperation = m_pState->GetRecursiveOperationHandler();
	pRecursiveOperation->AddDirectoryToVisit(path, _T(""), localDir);

	CServerPath currentPath;
	const wxTreeItemId selected = GetSelection();
	if (selected)
		currentPath = GetPathFromItem(selected);

	const bool addOnly = event.GetId() == XRCID("ID_ADDTOQUEUE");
	CFilterManager filter;
	pRecursiveOperation->StartRecursiveOperation(addOnly ? CRecursiveOperation::recursive_addtoqueue : CRecursiveOperation::recursive_download, path, filter.GetActiveFilters(false), true, currentPath);
}

void CRemoteTreeView::OnMenuDelete(wxCommandEvent&)
{
	if (!m_pState->IsRemoteIdle())
		return;

	if (!m_contextMenuItem)
		return;

	const CServerPath& path = GetPathFromItem(m_contextMenuItem);
	if (path.empty())
		return;

	if (wxMessageBoxEx(_("Really delete all selected files and/or directories from the server?"), _("Confirmation needed"), wxICON_QUESTION | wxYES_NO, this) != wxYES)
		return;

	const bool hasParent = path.HasParent();

	CRecursiveOperation* pRecursiveOperation = m_pState->GetRecursiveOperationHandler();

	CServerPath startDir;
	if (hasParent)
	{
		const wxString& name = GetItemText(m_contextMenuItem);
		startDir = path.GetParent();
		pRecursiveOperation->AddDirectoryToVisit(startDir, name);
	}
	else
	{
		startDir = path;
		pRecursiveOperation->AddDirectoryToVisit(startDir, _T(""));
	}

	CServerPath currentPath;
	const wxTreeItemId selected = GetSelection();
	if (selected)
		currentPath = GetPathFromItem(selected);
	if (!currentPath.empty() && (path == currentPath || path.IsParentOf(currentPath, false)))
		currentPath = startDir;

	CFilterManager filter;
	pRecursiveOperation->StartRecursiveOperation(CRecursiveOperation::recursive_delete, startDir, filter.GetActiveFilters(false), !hasParent, currentPath);
}

void CRemoteTreeView::OnMenuRename(wxCommandEvent&)
{
	if (!m_pState->IsRemoteIdle())
		return;

	if (!m_contextMenuItem)
		return;

	const CServerPath& path = GetPathFromItem(m_contextMenuItem);
	if (path.empty())
		return;

	if (!path.HasParent())
		return;

	EditLabel(m_contextMenuItem);
}

void CRemoteTreeView::OnBeginLabelEdit(wxTreeEvent& event)
{
	if (!m_pState->IsRemoteIdle())
	{
		event.Veto();
		return;
	}

	const CServerPath& path = GetPathFromItem(event.GetItem());
	if (path.empty())
	{
		event.Veto();
		return;
	}

	if (!path.HasParent())
	{
		event.Veto();
		return;
	}
}

void CRemoteTreeView::OnEndLabelEdit(wxTreeEvent& event)
{
	if (event.IsEditCancelled())
	{
		event.Veto();
		return;
	}

	if (!m_pState->IsRemoteIdle())
	{
		event.Veto();
		return;
	}

	CItemData* const pData = (CItemData*)GetItemData(event.GetItem());
	if (pData)
	{
		event.Veto();
		return;
	}

	CServerPath old_path = GetPathFromItem(event.GetItem());
	CServerPath parent = old_path.GetParent();

	const wxString& oldName = GetItemText(event.GetItem());
	const wxString& newName = event.GetLabel();
	if (oldName == newName)
	{
		event.Veto();
		return;
	}

	m_pState->m_pCommandQueue->ProcessCommand(new CRenameCommand(parent, oldName, parent, newName));
	m_pState->ChangeRemoteDir(parent);

	CServerPath currentPath;
	const wxTreeItemId selected = GetSelection();
	if (selected)
		currentPath = GetPathFromItem(selected);
	if (currentPath.empty())
		return;

	if (currentPath == old_path || currentPath.IsSubdirOf(old_path, false))
	{
		// Previously selected path was below renamed one, list the new one

		std::list<wxString> subdirs;
		while (currentPath != old_path)
		{
			if (!currentPath.HasParent())
			{
				// Abort just in case
				return;
			}
			subdirs.push_front(currentPath.GetLastSegment());
			currentPath = currentPath.GetParent();
		}
		currentPath = parent;
		currentPath.AddSegment(newName);
		for (std::list<wxString>::const_iterator iter = subdirs.begin(); iter != subdirs.end(); ++iter)
			currentPath.AddSegment(*iter);
		m_pState->ChangeRemoteDir(currentPath);
	}
	else if (currentPath != parent)
		m_pState->ChangeRemoteDir(currentPath);
}


// Create a new Directory
void CRemoteTreeView::OnMkdir(wxCommandEvent&)
{
	CServerPath newpath = MenuMkdir();

	CServerPath listed;
	if (newpath.HasParent())
	{
		listed = newpath.GetParent();
		m_pState->ChangeRemoteDir(listed);
	}

	CServerPath currentPath;
	const wxTreeItemId selected = GetSelection();
	if (selected)
		currentPath = GetPathFromItem(selected);
	if (!currentPath.empty() && currentPath != listed)
		m_pState->ChangeRemoteDir(currentPath);

}

// Create a new Directory and enter the new Directory
void CRemoteTreeView::OnMenuMkdirChgDir(wxCommandEvent&)
{
	CServerPath newpath = MenuMkdir();
	if (!newpath.empty()) {
		m_pState->ChangeRemoteDir(newpath);
	}
}

// Help-Function to create a new Directory
// Returns the name of the new directory
CServerPath CRemoteTreeView::MenuMkdir()
{
	if (!m_pState->IsRemoteIdle())
		return CServerPath();

	if (!m_contextMenuItem)
		return CServerPath();

	const CServerPath& path = GetPathFromItem(m_contextMenuItem);
	if (path.empty())
		return CServerPath();

	CInputDialog dlg;
	if (!dlg.Create(this, _("Create directory"), _("Please enter the name of the directory which should be created:")))
		return CServerPath();

	CServerPath newPath = path;

	// Append a long segment which does (most likely) not exist in the path and
	// replace it with "New directory" later. This way we get the exact position of
	// "New directory" and can preselect it in the dialog.
	wxString tmpName = _T("25CF809E56B343b5A12D1F0466E3B37A49A9087FDCF8412AA9AF8D1E849D01CF");
	if (newPath.AddSegment(tmpName))
	{
		wxString pathName = newPath.GetPath();
		int pos = pathName.Find(tmpName);
		wxASSERT(pos != -1);
		wxString newName = _("New directory");
		pathName.Replace(tmpName, newName);
		dlg.SetValue(pathName);
		dlg.SelectText(pos, pos + newName.Length());
	}

	if (dlg.ShowModal() != wxID_OK)
		return CServerPath();

	newPath = path;
	if (!newPath.ChangePath(dlg.GetValue()))
	{
		wxBell();
		return CServerPath();
	}

	m_pState->m_pCommandQueue->ProcessCommand(new CMkdirCommand(newPath));

	return newPath;
}

bool CRemoteTreeView::ListExpand(wxTreeItemId item)
{
	const CServerPath path = GetPathFromItem(item);
	wxASSERT(!path.empty());
	if (path.empty())
		return false;

	CDirectoryListing listing;
	if (m_pState->m_pEngine->CacheLookup(path, listing) == FZ_REPLY_OK)
		RefreshItem(item, listing, false);
	else
	{
		SetItemImages(item, true);

		wxTreeItemId child = GetLastChild(item);
		if (!child || GetItemText(child).empty())
			return false;
	}

	return true;
}

void CRemoteTreeView::OnChar(wxKeyEvent& event)
{
	m_contextMenuItem = GetSelection();

	wxCommandEvent cmdEvt;
	if (event.GetKeyCode() == WXK_F2)
		OnMenuRename(cmdEvt);
	else if (event.GetKeyCode() == WXK_DELETE || event.GetKeyCode() == WXK_NUMPAD_DELETE)
		OnMenuDelete(cmdEvt);
	else
		event.Skip();
}

struct _parents
{
	wxTreeItemId item;
	CServerPath path;
};

void CRemoteTreeView::ApplyFilters(bool resort)
{
	std::list<struct _parents> parents;

	const wxTreeItemId root = GetRootItem();
	wxTreeItemIdValue cookie;
	for (wxTreeItemId child = GetFirstChild(root, cookie); child; child = GetNextSibling(child)) {
		CServerPath path = GetPathFromItem(child);
		if (path.empty())
			continue;

		struct _parents dir;
		dir.item = child;
		dir.path = path;
		parents.push_back(dir);
	}

	CFilterManager filter;
	while (!parents.empty()) {
		struct _parents parent = parents.back();
		parents.pop_back();

		if (resort) {
			SortChildren(parent.item);
		}

		CDirectoryListing listing;
		if (m_pState->m_pEngine->CacheLookup(parent.path, listing) == FZ_REPLY_OK)
			RefreshItem(parent.item, listing, false);
		else if (filter.HasActiveFilters()) {
			for (wxTreeItemId child = GetFirstChild(parent.item, cookie); child; child = GetNextSibling(child)) {
				CServerPath path = GetPathFromItem(child);
				if (path.empty())
					continue;

				if (filter.FilenameFiltered(GetItemText(child), path.GetPath(), true, -1, false, 0, CDateTime())) {
					wxTreeItemId sel = GetSelection();
					while (sel && sel != child)
						sel = GetItemParent(sel);
					if (!sel) {
						Delete(child);
						continue;
					}
				}

				struct _parents dir;
				dir.item = child;
				dir.path = path;
				parents.push_back(dir);
			}

			// The stuff below has already been done above in this one case
			continue;
		}
		for (wxTreeItemId child = GetFirstChild(parent.item, cookie); child; child = GetNextSibling(child)) {
			CServerPath path = GetPathFromItem(child);
			if (path.empty())
				continue;

			struct _parents dir;
			dir.item = child;
			dir.path = path;
			parents.push_back(dir);
		}
	}
}

void CRemoteTreeView::OnMenuGeturl(wxCommandEvent&)
{
	if (!m_contextMenuItem)
		return;

	const CServerPath& path = GetPathFromItem(m_contextMenuItem);
	if (path.empty())
	{
		wxBell();
		return;
	}

	const CServer *pServer = m_pState->GetServer();
	if (!pServer)
	{
		wxBell();
		return;
	}

	if (!wxTheClipboard->Open())
	{
		wxMessageBoxEx(_("Could not open clipboard"), _("Could not copy URLs"), wxICON_EXCLAMATION);
		return;
	}

	wxString url = pServer->FormatServer(true);
	url += path.GetPath();

	// Poor mans URLencode
	url.Replace(_T(" "), _T("%20"));

	wxTheClipboard->SetData(new wxURLDataObject(url));

	wxTheClipboard->Flush();
	wxTheClipboard->Close();
}

void CRemoteTreeView::UpdateSortMode()
{
	switch (COptions::Get()->GetOptionVal(OPTION_FILELIST_NAMESORT))
	{
	case 0:
	default:
		m_nameSortMode = CFileListCtrlSortBase::namesort_caseinsensitive;
		break;
	case 1:
		m_nameSortMode = CFileListCtrlSortBase::namesort_casesensitive;
		break;
	case 2:
		m_nameSortMode = CFileListCtrlSortBase::namesort_natural;
		break;
	}
}

void CRemoteTreeView::OnOptionsChanged(changed_options_t const& options)
{
	if (options.test(OPTION_FILELIST_NAMESORT)) {
		UpdateSortMode();
		ApplyFilters(true);
	}
}
