#include <filezilla.h>
#include "treectrlex.h"

IMPLEMENT_DYNAMIC_CLASS(wxTreeCtrlEx, wxNavigationEnabled<wxTreeCtrl>)

#ifdef __WXMAC__
BEGIN_EVENT_TABLE(wxTreeCtrlEx, wxNavigationEnabled<wxTreeCtrl>)
EVT_CHAR(wxTreeCtrlEx::OnChar)
END_EVENT_TABLE()
#endif

wxTreeCtrlEx::wxTreeCtrlEx()
	: m_nameSortMode(
#ifdef __WXMSW__
		CFileListCtrlSortBase::namesort_caseinsensitive
#else
		CFileListCtrlSortBase::namesort_casesensitive
#endif
	)
{
}

wxTreeCtrlEx::wxTreeCtrlEx(wxWindow *parent, wxWindowID id /*=wxID_ANY*/,
			   const wxPoint& pos /*=wxDefaultPosition*/,
			   const wxSize& size /*=wxDefaultSize*/,
			   long style /*=wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT*/)
	: m_nameSortMode(
#ifdef __WXMSW__
		CFileListCtrlSortBase::namesort_caseinsensitive
#else
		CFileListCtrlSortBase::namesort_casesensitive
#endif
	)
{
	Create(parent, id, pos, size, style);
}

void wxTreeCtrlEx::SafeSelectItem(const wxTreeItemId& item)
{
	if( !item ) {
		m_setSelection = true;
		UnselectAll();
		m_setSelection = false;
	}
	else {
		const wxTreeItemId old_selection = GetSelection();

		m_setSelection = true;
		SelectItem(item);
		m_setSelection = false;
		if (item != old_selection)
			EnsureVisible(item);
	}
}

#ifdef __WXMAC__
void wxTreeCtrlEx::OnChar(wxKeyEvent& event)
{
	if (event.GetKeyCode() != WXK_TAB) {
		event.Skip();
		return;
	}

	HandleAsNavigationKey(event);
}
#endif

wxTreeItemId wxTreeCtrlEx::GetFirstItem() const
{
	wxTreeItemId root = GetRootItem();
	if (root.IsOk() && GetWindowStyle() & wxTR_HIDE_ROOT) {
		wxTreeItemIdValue cookie;
		root = GetFirstChild(root, cookie);
	}

	return root;
}

wxTreeItemId wxTreeCtrlEx::GetLastItem() const
{
	wxTreeItemId cur = GetRootItem();
	if (cur.IsOk() && GetWindowStyle() & wxTR_HIDE_ROOT) {
		cur = GetLastChild(cur);
	}

	while (cur.IsOk() && HasChildren(cur) && IsExpanded(cur)) {
		cur = GetLastChild(cur);
	}

	return cur;
}

wxTreeItemId wxTreeCtrlEx::GetBottomItem() const
{
	wxTreeItemId cur = GetFirstVisibleItem();
	if (cur) {
		wxTreeItemId next;
		while ((next = GetNextVisible(cur)).IsOk()) {
			cur = next;
		}
	}
	return cur;
}

wxTreeItemId wxTreeCtrlEx::GetNextItemSimple(wxTreeItemId const& item) const
{
	if (item.IsOk() && ItemHasChildren(item) && IsExpanded(item)) {
		wxTreeItemIdValue cookie;
		return GetFirstChild(item, cookie);
	}
	else {
		wxTreeItemId cur = item;
		wxTreeItemId next = GetNextSibling(cur);
		while (!next.IsOk() && cur.IsOk()) {
			cur = GetItemParent(cur);
			if (cur.IsOk()) {
				next = GetNextSibling(cur);
			}
		}
		return next;
	}
}

wxTreeItemId wxTreeCtrlEx::GetPrevItemSimple(wxTreeItemId const& item) const
{
	wxTreeItemId cur = GetPrevSibling(item);
	if (cur.IsOk()) {
		while (cur.IsOk() && HasChildren(cur) && IsExpanded(cur)) {
			cur = GetLastChild(cur);
		}
	}
	else {
		cur = GetItemParent(item);
		if (cur.IsOk() && cur == GetRootItem() && (GetWindowStyle() & wxTR_HIDE_ROOT)) {
			cur = wxTreeItemId();
		}
	}
	return cur;
}

int wxTreeCtrlEx::OnCompareItems(wxTreeItemId const& item1, wxTreeItemId const& item2)
{
	wxString const& label1 = GetItemText(item1);
	wxString const& label2 = GetItemText(item2);

	switch (m_nameSortMode)
	{
	case CFileListCtrlSortBase::namesort_casesensitive:
		return CFileListCtrlSortBase::CmpCase(label1, label2);

	default:
	case CFileListCtrlSortBase::namesort_caseinsensitive:
		return CFileListCtrlSortBase::CmpNoCase(label1, label2);

	case CFileListCtrlSortBase::namesort_natural:
		return CFileListCtrlSortBase::CmpNatural(label1, label2);
	}
}
