#ifndef __REMOTELISTVIEW_H__
#define __REMOTELISTVIEW_H__

#include "state.h"
#include "listctrlex.h"
#include "filelistctrl.h"

class CQueueView;
class CChmodDialog;
class CInfoText;
class CRemoteListViewDropTarget;

class CRemoteListView : public CFileListCtrl<CGenericFileData>, CStateEventHandler
{
	friend class CRemoteListViewDropTarget;
public:
	CRemoteListView(wxWindow* pParent, CState* pState, CQueueView* pQueue);
	virtual ~CRemoteListView();

	virtual bool CanStartComparison(wxString* pError);
	virtual void StartComparison();
	virtual bool GetNextFile(wxString& name, bool &dir, wxLongLong &size, CDateTime& date);
	virtual void FinishComparison();
	virtual void OnExitComparisonMode();

	void LinkIsNotDir(const CServerPath& path, const wxString& link);
protected:
	virtual wxString GetItemText(int item, unsigned int column);

	// Clears all selections and returns the list of items that were selected
	std::list<wxString> RememberSelectedItems(wxString& focused);

	// Select a list of items based in their names.
	// Sort order may not change between call to RememberSelectedItems and
	// ReselectItems
	void ReselectItems(std::list<wxString>& selectedNames, wxString focused, bool ensureVisible = false);


	// Declared const due to design error in wxWidgets.
	// Won't be fixed since a fix would break backwards compatibility
	// Both functions use a const_cast<CLocalListView *>(this) and modify
	// the instance.
	virtual wxListItemAttr* OnGetItemAttr(long item) const;
	virtual int OnGetItemImage(long item) const;

	virtual bool ItemIsDir(int index) const;
	virtual wxLongLong ItemGetSize(int index) const;

	bool IsItemValid(unsigned int item) const;
	int GetItemIndex(unsigned int item) const;

	virtual CSortComparisonObject GetSortComparisonObject();

	virtual void OnStateChange(CState* pState, enum t_statechange_notifications notification, const wxString& data, const void* data2);
	void ApplyCurrentFilter();
	void SetDirectoryListing(std::shared_ptr<CDirectoryListing> const& pDirectoryListing);
	bool UpdateDirectoryListing(std::shared_ptr<CDirectoryListing> const& pDirectoryListing);
	void UpdateDirectoryListing_Removed(std::shared_ptr<CDirectoryListing> const& pDirectoryListing);
	void UpdateDirectoryListing_Added(std::shared_ptr<CDirectoryListing> const& pDirectoryListing);

#ifdef __WXDEBUG__
	void ValidateIndexMapping();
#endif

	virtual void OnNavigationEvent(bool forward);

	std::shared_ptr<CDirectoryListing> m_pDirectoryListing;

	// Caller is responsible to check selection is valid!
	void TransferSelectedFiles(const CLocalPath& local_parent, bool queueOnly);

	// Cache icon for directories, no need to calculate it multiple times
	int m_dirIcon;

	CInfoText* m_pInfoText;
	void RepositionInfoText();
	void SetInfoText();

	virtual bool OnBeginRename(const wxListEvent& event);
	virtual bool OnAcceptRename(const wxListEvent& event);

#ifdef __WXMSW__
	virtual int GetOverlayIndex(int item);
#endif

	int m_dropTarget;

	// Used to track state for resolving symlinks
	// While being resolved, global state might have changed
	// already.
	struct t_linkResolveState
	{
		CServer server;
		CServerPath remote_path;
		wxString link;
		CLocalPath local_path;
	};
	std::unique_ptr<t_linkResolveState> m_pLinkResolveState;

	CServerPath MenuMkdir();

	DECLARE_EVENT_TABLE()
	void OnItemActivated(wxListEvent &event);
	void OnContextMenu(wxContextMenuEvent& event);
	void OnMenuDownload(wxCommandEvent& event);
	void OnMenuMkdir(wxCommandEvent&);
	void OnMenuMkdirChgDir(wxCommandEvent&);
	void OnMenuDelete(wxCommandEvent&);
	void OnMenuRename(wxCommandEvent&);
	void OnKeyDown(wxKeyEvent& event);
	void OnMenuChmod(wxCommandEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnBeginDrag(wxListEvent&);
	void OnMenuEdit(wxCommandEvent&);
	void OnMenuEnter(wxCommandEvent&);
	void OnMenuGeturl(wxCommandEvent&);
	void OnMenuRefresh(wxCommandEvent&);
	void OnMenuNewfile(wxCommandEvent& event);
};

#endif
