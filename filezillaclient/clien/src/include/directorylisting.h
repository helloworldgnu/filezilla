#ifndef __DIRECTORYLISTING_H__
#define __DIRECTORYLISTING_H__

#include "optional.h"
#include "timeex.h"

#include <map>

class CDirentry
{
public:
	wxString name;
	wxLongLong size;
	CRefcountObject<wxString> permissions;
	CRefcountObject<wxString> ownerGroup;

	enum _flags
	{
		flag_dir = 1,
		flag_link = 2,
		flag_unsure = 4 // May be set on cached items if any changes were made to the file
	};
	int flags;

	inline bool is_dir() const
	{
		return (flags & flag_dir) != 0;
	}
	inline bool is_link() const
	{
		return (flags & flag_link) != 0;
	}

	inline bool is_unsure() const
	{
		return (flags & flag_unsure) != 0;
	}

	inline bool has_date() const
	{
		return time.IsValid();;
	}
	inline bool has_time() const
	{
		return time.IsValid() && time.GetAccuracy() >= CDateTime::hours;
	}
	inline bool has_seconds() const
	{
		return time.IsValid() && time.GetAccuracy() >= CDateTime::seconds;
	}

	CSparseOptional<wxString> target; // Set to linktarget it link is true

	CDateTime time;

	wxString dump() const;
	bool operator==(const CDirentry &op) const;
};

#include "refcount.h"

class CDirectoryListing final
{
public:
	typedef CDirentry value_type;

	CDirectoryListing();
	CDirectoryListing(const CDirectoryListing& listing);

	CServerPath path;
	CDirectoryListing& operator=(const CDirectoryListing &a);

	const CDirentry& operator[](unsigned int index) const;

	// Word of caution: You MUST NOT change the name of the returned
	// entry if you do not call ClearFindMap afterwards
	CDirentry& operator[](unsigned int index);

	void SetCount(unsigned int count);
	unsigned int GetCount() const { return m_entryCount; }

	int FindFile_CmpCase(const wxString& name) const;
	int FindFile_CmpNoCase(wxString name) const;

	void ClearFindMap();

	CMonotonicTime m_firstListTime;

	enum
	{
		unsure_file_added = 0x01,
		unsure_file_removed = 0x02,
		unsure_file_changed = 0x04,
		unsure_file_mask = 0x07,
		unsure_dir_added = 0x08,
		unsure_dir_removed = 0x10,
		unsure_dir_changed = 0x20,
		unsure_dir_mask = 0x38,
		unsure_unknown = 0x40,
		unsure_invalid = 0x80, // Recommended action: Do a full refresh
		unsure_mask = 0xff,

		listing_failed = 0x100,
		listing_has_dirs = 0x200,
		listing_has_perms = 0x400,
		listing_has_usergroup = 0x800
	};
	// Lowest bit indicates a file got added
	// Next bit indicates a file got removed
	// 3rd bit indicates a file got changed.
	// 4th bit is set if an update cannot be applied to
	// one of the other categories.
	//
	// These bits should help the user interface to choose an appropriate sorting
	// algorithm for modified listings
	int m_flags;

	int get_unsure_flags() const { return m_flags & unsure_mask; }
	bool failed() const { return (m_flags & listing_failed) != 0; }
	bool has_dirs() const { return (m_flags & listing_has_dirs) != 0; }
	bool has_perms() const { return (m_flags & listing_has_perms) != 0; }
	bool has_usergroup() const { return (m_flags & listing_has_usergroup) != 0; }

	void Assign(std::deque<CRefcountObject<CDirentry>> & entries);

	bool RemoveEntry(unsigned int index);

	void GetFilenames(std::vector<wxString> &names) const;

protected:

	CRefcountObject_Uninitialized<std::vector<CRefcountObject<CDirentry> > > m_entries;

	mutable CRefcountObject_Uninitialized<std::multimap<wxString, unsigned int> > m_searchmap_case;
	mutable CRefcountObject_Uninitialized<std::multimap<wxString, unsigned int> > m_searchmap_nocase;

	unsigned int m_entryCount;
};

#endif
