#include <filezilla.h>

#include "file.h"

CFile::CFile()
{
}

CFile::CFile(wxString const& f, mode m, disposition d)
{
	Open(f, m, d);
}

CFile::~CFile()
{
	Close();
}

#ifdef __WXMSW__
bool CFile::Open(wxString const& f, mode m, disposition d)
{
	Close();

	DWORD dispositionFlags;
	if (m == write) {
		if (d == truncate) {
			dispositionFlags = CREATE_ALWAYS;
		}
		else {
			dispositionFlags = OPEN_ALWAYS;
		}
	}
	else {
		dispositionFlags = OPEN_EXISTING;
	}

	DWORD shareMode = FILE_SHARE_READ;
	if (m == read) {
		shareMode |= FILE_SHARE_WRITE;
	}

	hFile_ = CreateFile(f, (m == read) ? GENERIC_READ : GENERIC_WRITE, shareMode, 0, dispositionFlags, FILE_FLAG_SEQUENTIAL_SCAN, 0);

	return hFile_ != INVALID_HANDLE_VALUE;
}

void CFile::Close()
{
	if (hFile_ != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile_);
		hFile_ = INVALID_HANDLE_VALUE;
	}
}

wxFileOffset CFile::Length() const
{
	wxFileOffset ret = -1;

	LARGE_INTEGER size{};
	if (GetFileSizeEx(hFile_, &size)) {
		ret = size.QuadPart;
	}
	return ret;
}

wxFileOffset CFile::Seek(wxFileOffset offset, seekMode m)
{
	wxFileOffset ret = -1;

	LARGE_INTEGER dist{};
	dist.QuadPart = offset;

	DWORD method = FILE_BEGIN;
	if (m == current) {
		method = FILE_CURRENT;
	}
	else if (m == end) {
		method = FILE_END;
	}

	LARGE_INTEGER newPos{};
	if (SetFilePointerEx(hFile_, dist, &newPos, method)) {
		ret = newPos.QuadPart;
	}
	return ret;
}

bool CFile::Truncate()
{
	return !!SetEndOfFile(hFile_);
}

ssize_t CFile::Read(void *buf, size_t count)
{
	ssize_t ret = -1;

	DWORD read = 0;
	if (ReadFile(hFile_, buf, count, &read, 0)) {
		ret = static_cast<ssize_t>(read);
	}

	return ret;
}

ssize_t CFile::Write(void const* buf, size_t count)
{
	ssize_t ret = -1;

	DWORD written = 0;
	if (WriteFile(hFile_, buf, count, &written, 0)) {
		ret = static_cast<ssize_t>(written);
	}

	return ret;
}

bool CFile::Opened() const
{
	return hFile_ != INVALID_HANDLE_VALUE;
}

#else

#include <errno.h>
#include <sys/stat.h>

bool CFile::Open(wxString const& f, mode m, disposition d)
{
	Close();

	int flags = O_CLOEXEC;
	if (m == read) {
		flags |= O_RDONLY;
	}
	else {
		flags |= O_WRONLY | O_CREAT;
		if (d == truncate) {
			flags |= O_TRUNC;
		}
	}
	fd_ = open(f.fn_str(), flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

#if HAVE_POSIX_FADVISE
	if (fd_ != -1) {
		(void)posix_fadvise(fd_, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);
	}
#endif

	return fd_ != -1;
}

void CFile::Close()
{
	if (fd_ != -1) {
		close(fd_);
		fd_ = -1;
	}
}

wxFileOffset CFile::Length() const
{
	wxFileOffset ret = -1;

	struct stat buf;
	if (!fstat(fd_, &buf)) {
		ret = buf.st_size;
	}

	return ret;
}

wxFileOffset CFile::Seek(wxFileOffset offset, seekMode m)
{
	wxFileOffset ret = -1;

	int whence = SEEK_SET;
	if (m == current) {
		whence = SEEK_CUR;
	}
	else if (m == end) {
		whence = SEEK_END;
	}

	auto newPos = lseek(fd_, offset, whence);
	if (newPos != static_cast<off_t>(-1)) {
		ret = newPos;
	}

	return ret;
}

bool CFile::Truncate()
{
	bool ret = false;

	auto length = lseek(fd_, 0, SEEK_CUR);
	if (length != static_cast<off_t>(-1)) {
		do {
			ret = !ftruncate(fd_, length);
		} while (!ret && (errno == EAGAIN || errno == EINTR));
	}

	return ret;
}

ssize_t CFile::Read(void *buf, size_t count)
{
	ssize_t ret;
	do {
		ret = ::read(fd_, buf, count);
	} while (ret == -1 && (errno == EAGAIN || errno == EINTR));

	return ret;
}

ssize_t CFile::Write(void const* buf, size_t count)
{
	ssize_t ret;
	do {
		ret = ::write(fd_, buf, count);
	} while (ret == -1 && (errno == EAGAIN || errno == EINTR));

	return ret;
}

bool CFile::Opened() const
{
	return fd_ != -1;
}

#endif
