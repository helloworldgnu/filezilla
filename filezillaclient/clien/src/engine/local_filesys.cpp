#include <filezilla.h>

#include "local_filesys.h"
#include <msgbox.h>

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>

#ifdef __WXMSW__
const wxChar CLocalFileSystem::path_separator = '\\';
#else
const wxChar CLocalFileSystem::path_separator = '/';
#endif

namespace {
template<typename T>
int64_t make_int64_t(T hi, T lo)
{
	return (static_cast<int64_t>(hi) << 32) + static_cast<int64_t>(lo);
}
}

CLocalFileSystem::~CLocalFileSystem()
{
	EndFindFiles();
}

CLocalFileSystem::local_fileType CLocalFileSystem::GetFileType(const wxString& path)
{
#ifdef __WXMSW__
	DWORD result = GetFileAttributes(path);
	if (result == INVALID_FILE_ATTRIBUTES)
		return unknown;

	if (result & FILE_ATTRIBUTE_REPARSE_POINT)
		return link;

	if (result & FILE_ATTRIBUTE_DIRECTORY)
		return dir;

	return file;
#else
	if (!path.empty() && path.Last() == '/' && path != _T("/")) {
		wxString tmp = path;
		tmp.RemoveLast();
		return GetFileType(tmp);
	}

	wxStructStat buf;
	int result = wxLstat(path, &buf);
	if (result)
		return unknown;

#ifdef S_ISLNK
	if (S_ISLNK(buf.st_mode))
		return link;
#endif

	if (S_ISDIR(buf.st_mode))
		return dir;

	return file;
#endif
}

bool CLocalFileSystem::RecursiveDelete(const wxString& path, wxWindow* parent)
{
	std::list<wxString> paths;
	paths.push_back(path);
	return RecursiveDelete(paths, parent);
}

bool CLocalFileSystem::RecursiveDelete(std::list<wxString> dirsToVisit, wxWindow* parent)
{
	// Under Windows use SHFileOperation to delete files and directories.
	// Under other systems, we have to recurse into subdirectories manually
	// to delete all contents.

#ifdef __WXMSW__
	// SHFileOperation accepts a list of null-terminated strings. Go through all
	// paths to get the required buffer length

	size_t len = 1; // String list terminated by empty string

	for (auto const& dir : dirsToVisit) {
		len += dir.size() + 1;
	}

	// Allocate memory
	wxChar* pBuffer = new wxChar[len];
	wxChar* p = pBuffer;

	for (auto& dir : dirsToVisit) {
		if (!dir.empty() && dir.Last() == wxFileName::GetPathSeparator())
			dir.RemoveLast();
		if (GetFileType(dir) == unknown)
			continue;

		_tcscpy(p, dir);
		p += dir.size() + 1;
	}
	if (p != pBuffer) {
		*p = 0;

		// Now we can delete the files in the buffer
		SHFILEOPSTRUCT op;
		memset(&op, 0, sizeof(op));
		op.hwnd = parent ? (HWND)parent->GetHandle() : 0;
		op.wFunc = FO_DELETE;
		op.pFrom = pBuffer;

		if (parent) {
			// Move to trash if shift is not pressed, else delete
			op.fFlags = wxGetKeyState(WXK_SHIFT) ? 0 : FOF_ALLOWUNDO;
		}
		else
			op.fFlags = FOF_NOCONFIRMATION;

		SHFileOperation(&op);
	}
	delete [] pBuffer;

	return true;
#else
	if (parent) {
		if (wxMessageBoxEx(_("Really delete all selected files and/or directories from your computer?"), _("Confirmation needed"), wxICON_QUESTION | wxYES_NO, parent) != wxYES)
			return true;
	}

	for (auto& dir : dirsToVisit) {
		if (!dir.empty() && dir.Last() == '/' && dir != _T("/"))
			dir.RemoveLast();
	}

	bool encodingError = false;

	// Remember the directories to delete after recursing into them
	std::list<wxString> dirsToDelete;

	CLocalFileSystem fs;

	// Process all dirctories that have to be visited
	while (!dirsToVisit.empty()) {
		auto const iter = dirsToVisit.begin();
		wxString const& path = *iter;

		if (GetFileType(path) != dir) {
			wxRemoveFile(path);
			dirsToVisit.erase(iter);
			continue;
		}

		dirsToDelete.splice(dirsToDelete.begin(), dirsToVisit, iter);

		if (!fs.BeginFindFiles(path, false)) {
			continue;
		}

		// Depending on underlying platform, wxDir does not handle
		// changes to the directory contents very well.
		// See http://trac.filezilla-project.org/ticket/3482
		// To work around this, delete files after enumerating everything in current directory
		std::list<wxString> filesToDelete;

		wxString file;
		while (fs.GetNextFile(file)) {
			if (file.empty()) {
				encodingError = true;
				continue;
			}

			const wxString& fullName = path + _T("/") + file;

			if (CLocalFileSystem::GetFileType(fullName) == CLocalFileSystem::dir)
				dirsToVisit.push_back(fullName);
			else
				filesToDelete.push_back(fullName);
		}
		fs.EndFindFiles();

		// Delete all files and links in current directory enumerated before
		for (auto const& file : filesToDelete) {
			wxRemoveFile(file);
		}
	}

	// Delete the now empty directories
	for (auto const& dir : dirsToDelete) {
		wxRmdir(dir);
	}

	return !encodingError;
#endif
}

CLocalFileSystem::local_fileType CLocalFileSystem::GetFileInfo(const wxString& path, bool &isLink, int64_t* size, CDateTime* modificationTime, int *mode)
{
#ifdef __WXMSW__
	if (!path.empty() && path.Last() == wxFileName::GetPathSeparator() && path != wxFileName::GetPathSeparator()) {
		wxString tmp = path;
		tmp.RemoveLast();
		return GetFileInfo(tmp, isLink, size, modificationTime, mode);
	}

	isLink = false;

	WIN32_FILE_ATTRIBUTE_DATA attributes;
	BOOL result = GetFileAttributesEx(path, GetFileExInfoStandard, &attributes);
	if (!result) {
		if (size)
			*size = -1;
		if (mode)
			*mode = 0;
		if (modificationTime)
			*modificationTime = CDateTime();
		return unknown;
	}

	bool is_dir = (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	if (attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		isLink = true;

		HANDLE hFile = is_dir ? INVALID_HANDLE_VALUE : CreateFile(path, FILE_READ_ATTRIBUTES | FILE_READ_EA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (hFile != INVALID_HANDLE_VALUE) {
			BY_HANDLE_FILE_INFORMATION info{};
			int ret = GetFileInformationByHandle(hFile, &info);
			CloseHandle(hFile);
			if (ret != 0 && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {

				if (modificationTime) {
					if (!ConvertFileTimeToCDateTime(*modificationTime, info.ftLastWriteTime))
						ConvertFileTimeToCDateTime(*modificationTime, info.ftCreationTime);
				}

				if (mode)
					*mode = (int)info.dwFileAttributes;

				if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					if (size)
						*size = -1;
					return dir;
				}

				if (size)
					*size = make_int64_t(info.nFileSizeHigh, info.nFileSizeLow);

				return file;
			}
		}

		if (size)
			*size = -1;
		if (mode)
			*mode = 0;
		if (modificationTime)
			*modificationTime = CDateTime();
		return is_dir ? dir : unknown;
	}

	if (modificationTime) {
		if (!ConvertFileTimeToCDateTime(*modificationTime, attributes.ftLastWriteTime))
			ConvertFileTimeToCDateTime(*modificationTime, attributes.ftCreationTime);
	}

	if (mode)
		*mode = (int)attributes.dwFileAttributes;

	if (is_dir) {
		if (size)
			*size = -1;
		return dir;
	}
	else {
		if (size)
			*size = make_int64_t(attributes.nFileSizeHigh, attributes.nFileSizeLow);
		return file;
	}
#else
	if (!path.empty() && path.Last() == '/' && path != _T("/")) {
		wxString tmp = path;
		tmp.RemoveLast();
		return GetFileInfo(tmp, isLink, size, modificationTime, mode);
	}

	const wxCharBuffer p = path.fn_str();
	return GetFileInfo((const char*)p, isLink, size, modificationTime, mode);
#endif
}

#ifndef __WXMSW__
CLocalFileSystem::local_fileType CLocalFileSystem::GetFileInfo(const char* path, bool &isLink, int64_t* size, CDateTime* modificationTime, int *mode)
{
	struct stat buf;
	int result = lstat(path, &buf);
	if (result)
	{
		isLink = false;
		if (size)
			*size = -1;
		if (mode)
			*mode = -1;
		if (modificationTime)
			*modificationTime = CDateTime();
		return unknown;
	}

#ifdef S_ISLNK
	if (S_ISLNK(buf.st_mode))
	{
		isLink = true;
		int result = stat(path, &buf);
		if (result)
		{
			if (size)
				*size = -1;
			if (mode)
				*mode = -1;
			if (modificationTime)
				*modificationTime = CDateTime();
			return unknown;
		}
	}
	else
#endif
		isLink = false;

	if (modificationTime)
		*modificationTime = CDateTime(wxDateTime(buf.st_mtime), CDateTime::seconds);

	if (mode)
		*mode = buf.st_mode & 0x777;

	if (S_ISDIR(buf.st_mode))
	{
		if (size)
			*size = -1;
		return dir;
	}

	if (size)
		*size = buf.st_size;

	return file;
}
#endif

#ifdef __WXMSW__

// This is the offset between FILETIME epoch and the Unix/wxDateTime Epoch.
static wxInt64 EPOCH_OFFSET_IN_MSEC = 11644473600000ll;

bool CLocalFileSystem::ConvertFileTimeToCDateTime(CDateTime& time, const FILETIME &ft)
{
	if (!ft.dwHighDateTime && !ft.dwLowDateTime)
		return false;

	// See http://trac.wxwidgets.org/changeset/74423 and http://trac.wxwidgets.org/ticket/13098
	// Directly converting to time_t

	int64_t t = make_int64_t(ft.dwHighDateTime, ft.dwLowDateTime);
	t /= 10000; // Convert hundreds of nanoseconds to milliseconds.
	t -= EPOCH_OFFSET_IN_MSEC;
	if (t < 0) {
		return false;
	}

	// Interestingly wxDateTime has this constructor which
	// even more interestingly isn't even marked explicit.
	time = CDateTime(wxDateTime(wxLongLong(t)), CDateTime::milliseconds);
	return time.IsValid();
}

bool CLocalFileSystem::ConvertCDateTimeToFileTime(FILETIME &ft, CDateTime const& time)
{
	if (!time.IsValid())
		return false;

	int64_t t = time.Degenerate().GetValue().GetValue();

	t += EPOCH_OFFSET_IN_MSEC;
	t *= 10000;

	ft.dwHighDateTime = t >> 32;
	ft.dwLowDateTime = t & 0xffffffffll;

	return true;
}
#endif

bool CLocalFileSystem::BeginFindFiles(wxString path, bool dirs_only)
{
	if (path.empty()) {
		return false;
	}

	EndFindFiles();

	m_dirs_only = dirs_only;
#ifdef __WXMSW__
	if (path.Last() != '/' && path.Last() != '\\') {
		m_find_path = path + _T("\\");
		path += _T("\\*");
	}
	else {
		m_find_path = path;
		path += '*';
	}

	m_hFind = FindFirstFileEx(path, FindExInfoStandard, &m_find_data, dirs_only ? FindExSearchLimitToDirectories : FindExSearchNameMatch, 0, 0);
	if (m_hFind == INVALID_HANDLE_VALUE) {
		m_found = false;
		return false;
	}

	m_found = true;
	return true;
#else
	if (path != _T("/") && path.Last() == '/')
		path.RemoveLast();

	const wxCharBuffer s = path.fn_str();

	m_dir = opendir(s);
	if (!m_dir)
		return false;

	const wxCharBuffer p = path.fn_str();
	const int len = strlen(p);
	m_raw_path = new char[len + 2048 + 2];
	m_buffer_length = len + 2048 + 2;
	strcpy(m_raw_path, p);
	if (len > 1)
	{
		m_raw_path[len] = '/';
		m_file_part = m_raw_path + len + 1;
	}
	else
		m_file_part = m_raw_path + len;

	return true;
#endif
}

void CLocalFileSystem::EndFindFiles()
{
#ifdef __WXMSW__
	m_found = false;
	if (m_hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(m_hFind);
		m_hFind = INVALID_HANDLE_VALUE;
	}
#else
	if (m_dir)
	{
		closedir(m_dir);
		m_dir = 0;
	}
	delete [] m_raw_path;
	m_raw_path = 0;
	m_file_part = 0;
#endif
}

bool CLocalFileSystem::GetNextFile(wxString& name)
{
#ifdef __WXMSW__
	if (!m_found)
		return false;
	do {
		name = m_find_data.cFileName;
		if (name.empty()) {
			m_found = FindNextFile(m_hFind, &m_find_data) != 0;
			return true;
		}
		if (name == _T(".") || name == _T(".."))
			continue;

		if (m_dirs_only && !(m_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;

		m_found = FindNextFile(m_hFind, &m_find_data) != 0;
		return true;
	} while ((m_found = FindNextFile(m_hFind, &m_find_data) != 0));

	return false;
#else
	if (!m_dir)
		return false;

	struct dirent* entry;
	while ((entry = readdir(m_dir))) {
		if (!entry->d_name[0] ||
			!strcmp(entry->d_name, ".") ||
			!strcmp(entry->d_name, ".."))
			continue;

		if (m_dirs_only) {
#if HAVE_STRUCT_DIRENT_D_TYPE
			if (entry->d_type == DT_LNK)
			{
				bool wasLink;
				AllocPathBuffer(entry->d_name);
				strcpy(m_file_part, entry->d_name);
				if (GetFileInfo(m_raw_path, wasLink, 0, 0, 0) != dir)
					continue;
			}
			else if (entry->d_type != DT_DIR)
				continue;
#else
			// Solaris doesn't have d_type
			bool wasLink;
			AllocPathBuffer(entry->d_name);
			strcpy(m_file_part, entry->d_name);
			if (GetFileInfo(m_raw_path, wasLink, 0, 0, 0) != dir)
				continue;
#endif
		}

		name = wxString(entry->d_name, *wxConvFileName);

		return true;
	}

	return false;
#endif
}

bool CLocalFileSystem::GetNextFile(wxString& name, bool &isLink, bool &is_dir, int64_t* size, CDateTime* modificationTime, int* mode)
{
#ifdef __WXMSW__
	if (!m_found)
		return false;
	do
	{
		if (!m_find_data.cFileName[0]) {
			m_found = FindNextFile(m_hFind, &m_find_data) != 0;
			return true;
		}
		if (m_dirs_only && !(m_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;

		if (m_find_data.cFileName[0] == '.' && (!m_find_data.cFileName[1] || (m_find_data.cFileName[1] == '.' && !m_find_data.cFileName[2])))
			continue;
		name = m_find_data.cFileName;

		is_dir = (m_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		isLink = (m_find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		if (isLink) {
			HANDLE hFile = is_dir ? INVALID_HANDLE_VALUE : CreateFile(m_find_path + name, FILE_READ_ATTRIBUTES | FILE_READ_EA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if (hFile != INVALID_HANDLE_VALUE) {
				BY_HANDLE_FILE_INFORMATION info{};
				int ret = GetFileInformationByHandle(hFile, &info);
				CloseHandle(hFile);
				if (ret != 0 && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {

					if (modificationTime) {
						if (!ConvertFileTimeToCDateTime(*modificationTime, info.ftLastWriteTime))
							ConvertFileTimeToCDateTime(*modificationTime, info.ftCreationTime);
					}

					if (mode)
						*mode = (int)info.dwFileAttributes;

					is_dir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
					if (size) {
						if (is_dir) {
							*size = -1;
						}
						else {
							*size = make_int64_t(info.nFileSizeHigh, info.nFileSizeLow);
						}
					}

					m_found = FindNextFile(m_hFind, &m_find_data) != 0;
					return true;
				}
			}

			if (m_dirs_only && !is_dir) {
				continue;
			}

			if (size)
				*size = -1;
			if (mode)
				*mode = 0;
			if (modificationTime)
				*modificationTime = CDateTime();
		}
		else {
			if (modificationTime)
				ConvertFileTimeToCDateTime(*modificationTime, m_find_data.ftLastWriteTime);

			if (mode)
				*mode = (int)m_find_data.dwFileAttributes;

			if (size) {
				if (is_dir) {
					*size = -1;
				}
				else {
					*size = make_int64_t(m_find_data.nFileSizeHigh, m_find_data.nFileSizeLow);
				}
			}
		}
		m_found = FindNextFile(m_hFind, &m_find_data) != 0;
		return true;
	} while ((m_found = FindNextFile(m_hFind, &m_find_data) != 0));

	return false;
#else
	if (!m_dir)
		return false;

	struct dirent* entry;
	while ((entry = readdir(m_dir)))
	{
		if (!entry->d_name[0] ||
			!strcmp(entry->d_name, ".") ||
			!strcmp(entry->d_name, ".."))
			continue;

#if HAVE_STRUCT_DIRENT_D_TYPE
		if (m_dirs_only)
		{
			if (entry->d_type == DT_LNK)
			{
				AllocPathBuffer(entry->d_name);
				strcpy(m_file_part, entry->d_name);
				local_fileType type = GetFileInfo(m_raw_path, isLink, size, modificationTime, mode);
				if (type != dir)
					continue;

				name = wxString(entry->d_name, *wxConvFileName);
				is_dir = true;
				return true;
			}
			else if (entry->d_type != DT_DIR)
				continue;
		}
#endif

		AllocPathBuffer(entry->d_name);
		strcpy(m_file_part, entry->d_name);
		local_fileType type = GetFileInfo(m_raw_path, isLink, size, modificationTime, mode);

		if (type == unknown) // Happens for example in case of permission denied
		{
#if HAVE_STRUCT_DIRENT_D_TYPE
			type = entry->d_type == DT_DIR ? dir : file;
#else
			type = file;
#endif
			isLink = 0;
			if (size)
				*size = -1;
			if (modificationTime)
				*modificationTime = CDateTime();
			if (mode)
				*mode = 0;
		}
		if (m_dirs_only && type != dir)
			continue;

		is_dir = type == dir;

		name = wxString(entry->d_name, *wxConvFileName);

		return true;
	}

	return false;
#endif
}

#ifndef __WXMSW__
void CLocalFileSystem::AllocPathBuffer(const char* file)
{
	int len = strlen(file);
	int pathlen = m_file_part - m_raw_path;

	if (len + pathlen >= m_buffer_length)
	{
		m_buffer_length = (len + pathlen) * 2;
		char* tmp = new char[m_buffer_length];
		memcpy(tmp, m_raw_path, pathlen);
		delete [] m_raw_path;
		m_raw_path = tmp;
		m_file_part = m_raw_path + pathlen;
	}
}
#endif

CDateTime CLocalFileSystem::GetModificationTime( const wxString& path)
{
	CDateTime mtime;

	bool tmp;
	if (GetFileInfo(path, tmp, 0, &mtime, 0) == unknown)
		mtime = CDateTime();

	return mtime;
}

bool CLocalFileSystem::SetModificationTime(const wxString& path, const CDateTime& t)
{
	if (!t.IsValid())
		return false;

#ifdef __WXMSW__
	FILETIME ft;
	if (!ConvertCDateTimeToFileTime(ft, t))
		return false;

	HANDLE h = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	bool ret = SetFileTime(h, 0, &ft, &ft) == TRUE;
	CloseHandle(h);
	return ret;
#else
	wxFileName fn(path);
	wxDateTime d = t.Degenerate();
	return fn.SetTimes( &d, &d, 0 );
#endif
}

int64_t CLocalFileSystem::GetSize(wxString const& path, bool* isLink)
{
	int64_t ret = -1;
	bool tmp{};
	local_fileType t = GetFileInfo(path, isLink ? *isLink : tmp, &ret, 0, 0);
	if( t != file ) {
		ret = -1;
	}

	return ret;
}

wxString CLocalFileSystem::GetSymbolicLinkTarget(wxString const& path)
{
	wxString target;

#ifdef __WXMSW__
	HANDLE hFile = CreateFile(path, FILE_READ_ATTRIBUTES | FILE_READ_EA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD const size = 1024;
		wxChar out[size];
		DWORD ret = GetFinalPathNameByHandle(hFile, out, size, 0);
		if (ret > 0 && ret < size) {
			target = out;
		}
		CloseHandle(hFile);
	}
#else
	size_t const size = 1024;
	char out[size];

	const wxCharBuffer p = path.fn_str();
	ssize_t res = readlink(static_cast<char const*>(p), out, size);
	if( res > 0 && static_cast<size_t>(res) < size ) {
		out[res] = 0;
		target = wxString(out, *wxConvFileName);
	}
#endif
	return target;
}
