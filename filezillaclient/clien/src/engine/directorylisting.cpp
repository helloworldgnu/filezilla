#include <filezilla.h>

CDirectoryListing::CDirectoryListing()
	: m_flags()
	, m_entryCount()
{
}

CDirectoryListing::CDirectoryListing(const CDirectoryListing& listing)
	: path(listing.path)
	, m_firstListTime(listing.m_firstListTime)
	, m_flags(listing.m_flags)
	, m_entries(listing.m_entries), m_searchmap_case(listing.m_searchmap_case), m_searchmap_nocase(listing.m_searchmap_nocase)
	, m_entryCount(listing.m_entryCount)
{
}

CDirectoryListing& CDirectoryListing::operator=(const CDirectoryListing &a)
{
	if (&a == this)
		return *this;

	m_entries = a.m_entries;

	path = a.path;

	m_flags = a.m_flags;

	m_entryCount = a.m_entryCount;

	m_firstListTime = a.m_firstListTime;

	m_searchmap_case = a.m_searchmap_case;
	m_searchmap_nocase = a.m_searchmap_nocase;

	return *this;
}

wxString CDirentry::dump() const
{
	wxString str = wxString::Format(_T("name=%s\nsize=%s\npermissions=%s\nownerGroup=%s\ndir=%d\nlink=%d\ntarget=%s\nunsure=%d\n"),
				name, size.ToString(), *permissions, *ownerGroup, flags & flag_dir, flags & flag_link,
				target ? *target : wxString(), flags & flag_unsure);

	if( has_date() ) {
		str += _T("date=") + time.Degenerate().FormatISODate() + _T("\n");
	}
	if( has_time() ) {
		str += _T("time=") + time.Degenerate().FormatISOTime() + _T("\n");
	}
	return str;
}

bool CDirentry::operator==(const CDirentry &op) const
{
	if (name != op.name)
		return false;

	if (size != op.size)
		return false;

	if (permissions != op.permissions)
		return false;

	if (ownerGroup != op.ownerGroup)
		return false;

	if (flags != op.flags)
		return false;

	if( has_date() ) {
		if (time != op.time)
			return false;
	}

	return true;
}

void CDirectoryListing::SetCount(unsigned int count)
{
	if (count == m_entryCount)
		return;

	const unsigned int old_count = m_entryCount;

	if (!count)
	{
		m_entryCount = 0;
		return;
	}

	if (count < old_count)
	{
		m_searchmap_case.clear();
		m_searchmap_nocase.clear();
	}

	m_entries.Get().resize(count);

	m_entryCount = count;
}

const CDirentry& CDirectoryListing::operator[](unsigned int index) const
{
	// Commented out, too heavy speed penalty
	// wxASSERT(index < m_entryCount);
	return *(*m_entries)[index];
}

CDirentry& CDirectoryListing::operator[](unsigned int index)
{
	// Commented out, too heavy speed penalty
	// wxASSERT(index < m_entryCount);
	return m_entries.Get()[index].Get();
}

void CDirectoryListing::Assign(std::deque<CRefcountObject<CDirentry>> &entries)
{
	m_entryCount = entries.size();

	std::vector<CRefcountObject<CDirentry> >& own_entries = m_entries.Get();
	own_entries.clear();
	own_entries.reserve(m_entryCount);

	m_flags &= ~(listing_has_dirs | listing_has_perms | listing_has_usergroup);

	for (auto & entry : entries) {
		if (entry->is_dir())
			m_flags |= listing_has_dirs;
		if (!entry->permissions->empty())
			m_flags |= listing_has_perms;
		if (!entry->ownerGroup->empty())
			m_flags |= listing_has_usergroup;
		own_entries.emplace_back(std::move(entry));
	}

	m_searchmap_case.clear();
	m_searchmap_nocase.clear();
}

bool CDirectoryListing::RemoveEntry(unsigned int index)
{
	if (index >= GetCount())
		return false;

	m_searchmap_case.clear();
	m_searchmap_nocase.clear();

	std::vector<CRefcountObject<CDirentry> >& entries = m_entries.Get();
	std::vector<CRefcountObject<CDirentry> >::iterator iter = entries.begin() + index;
	if ((*iter)->is_dir())
		m_flags |= CDirectoryListing::unsure_dir_removed;
	else
		m_flags |= CDirectoryListing::unsure_file_removed;
	entries.erase(iter);

	--m_entryCount;

	return true;
}

void CDirectoryListing::GetFilenames(std::vector<wxString> &names) const
{
	names.reserve(GetCount());
	for (unsigned int i = 0; i < GetCount(); ++i)
		names.push_back((*m_entries)[i]->name);
}

int CDirectoryListing::FindFile_CmpCase(const wxString& name) const
{
	if (!m_entryCount)
		return -1;

	if (!m_searchmap_case)
		m_searchmap_case.Get();

	// Search map
	std::multimap<wxString, unsigned int>::const_iterator iter = m_searchmap_case->find(name);
	if (iter != m_searchmap_case->end())
		return iter->second;

	unsigned int i = m_searchmap_case->size();
	if (i == m_entryCount)
		return -1;

	std::multimap<wxString, unsigned int>& searchmap_case = m_searchmap_case.Get();

	// Build map if not yet complete
	std::vector<CRefcountObject<CDirentry> >::const_iterator entry_iter = m_entries->begin() + i;
	for (; entry_iter != m_entries->end(); ++entry_iter, ++i)
	{
		const wxString& entry_name = (*entry_iter)->name;
		searchmap_case.insert(std::pair<const wxString, unsigned int>(entry_name, i));

		if (entry_name == name)
			return i;
	}

	// Map is complete, item not in it
	return -1;
}

int CDirectoryListing::FindFile_CmpNoCase(wxString name) const
{
	if (!m_entryCount)
		return -1;

	if (!m_searchmap_nocase)
		m_searchmap_nocase.Get();

	name.MakeLower();

	// Search map
	std::multimap<wxString, unsigned int>::const_iterator iter = m_searchmap_nocase->find(name);
	if (iter != m_searchmap_nocase->end())
		return iter->second;

	unsigned int i = m_searchmap_nocase->size();
	if (i == m_entryCount)
		return -1;

	std::multimap<wxString, unsigned int>& searchmap_nocase = m_searchmap_nocase.Get();

	// Build map if not yet complete
	std::vector<CRefcountObject<CDirentry> >::const_iterator entry_iter = m_entries->begin() + i;
	for (; entry_iter != m_entries->end(); ++entry_iter, ++i)
	{
		wxString entry_name = (*entry_iter)->name;
		entry_name.MakeLower();
		searchmap_nocase.insert(std::pair<const wxString, unsigned int>(entry_name, i));

		if (entry_name == name)
			return i;
	}

	// Map is complete, item not in it
	return -1;
}

void CDirectoryListing::ClearFindMap()
{
	if (!m_searchmap_case)
		return;

	m_searchmap_case.clear();
	m_searchmap_nocase.clear();
}
