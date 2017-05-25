#ifndef __REMOTETREEVIEW_H__
#define __REMOTETREEVIEW_H__

#include <option_change_event_handler.h>
#include "systemimagelist.h"
#include "state.h"
#include "filter.h"
#include "treectrlex.h"

class CQueueView;
class CRemoteTreeView : public wxTreeCtrlEx, CSystemImageList, CStateEventHandler, COptionChangeEventHandler
{
	DECLARE_CLASS(CRemoteTreeView)

	friend class CRemoteTreeViewDropTarget;

public:
	CRemoteTreeView(wxWindow* parent, wxWindowID id, CState* pState, CQueueView* pQueue);
	virtual ~CRemoteTreeView();

protected:
	wxTreeItemId MakeParent(CServerPath path, bool select);
	void SetDirectoryListing(std::shared_ptr<CDirectoryListing> const& pListing, bool modified);
	virtual void OnStateChange(CState* pState, enum t_statechange_notifications notification, const wxString&, const void*);

	void DisplayItem(wxTreeItemId parent, const CDirectoryListing& listing);
	void RefreshItem(wxTreeItemId parent, const CDirectoryListing& listing, bool will_select_parent);

	void SetItemImages(wxTreeItemId item, bool unknown);

	bool HasSubdirs(const CDirectoryListing& listing, const CFilterManager& filter);

	CServerPath GetPathFromItem(const wxTreeItemId& item) const;

	bool ListExpand(wxTreeItemId item);

	void ApplyFilters(bool resort);

	CQueueView* m_pQueue;

	void CreateImageList();
	wxBitmap CreateIcon(int index, const wxString& overlay = _T(""));
	wxImageList* m_pImageList;

	// Set to true in SetDirectoryListing.
	// Used to suspends event processing in OnItemExpanding for example
	bool m_busy;

	wxTreeItemId m_ExpandAfterList;

	wxTreeItemId m_dropHighlight;

	CServerPath MenuMkdir();

	void UpdateSortMode();

	virtual void OnOptionsChanged(changed_options_t const& options);

	DECLARE_EVENT_TABLE()
	void OnItemExpanding(wxTreeEvent& event);
	void OnSelectionChanged(wxTreeEvent& event);
	void OnItemActivated(wxTreeEvent& event);
	void OnBeginDrag(wxTreeEvent& event);
	void OnContextMenu(wxTreeEvent& event);
	void OnMenuChmod(wxCommandEvent&);
	void OnMenuDownload(wxCommandEvent& event);
	void OnMenuDelete(wxCommandEvent&);
	void OnMenuRename(wxCommandEvent&);
	void OnBeginLabelEdit(wxTreeEvent& event);
	void OnEndLabelEdit(wxTreeEvent& event);
	void OnMkdir(wxCommandEvent&);
	void OnMenuMkdirChgDir(wxCommandEvent&);
	void OnChar(wxKeyEvent& event);
	void OnMenuGeturl(wxCommandEvent&);

	wxTreeItemId m_contextMenuItem;
};

#endif
