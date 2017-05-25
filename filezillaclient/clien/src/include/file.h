#ifndef FILEZILLA_FILE_HEADER
#define FILEZILLA_FILE_HEADER

// Lean replacement of wxFile that is implemented in terms of CreateFile instead of _open on Windows.
class CFile final
{
public:
	enum mode {
		read,
		write
	};

	// Only evaluated when opening existing files for writing
	// Non-existing files will always be created when writing.
	// Opening for reading never creates files
	enum disposition
	{
		existing, // Keep existing data
		truncate  // Truncate file
	};

	CFile();
	CFile(wxString const& f, mode m, disposition d = existing);

	~CFile();

	CFile(CFile const&) = delete;
	CFile& operator=(CFile const&) = delete;

	bool Opened() const;

	bool Open(wxString const& f, mode m, disposition d = existing);
	void Close();

	enum seekMode
	{
		begin,
		current,
		end
	};

	// Gets size of file
	// Returns -1 on error
	wxFileOffset Length() const;

	// Relative seek based on seek mode
	// Returns -1 on error, otherwise new absolute offset in file
	// On failure, the new position in the file is undefined.
	wxFileOffset Seek(wxFileOffset offset, seekMode m);

	// Truncate the file to the current position of the file pointer.
	bool Truncate();

	// Returns number of bytes read or -1 on error
	ssize_t Read(void *buf, size_t count);

	// Returns number of bytes written or -1 on error
	ssize_t Write(void const* buf, size_t count);

protected:
#ifdef __WXMSW__
	HANDLE hFile_{INVALID_HANDLE_VALUE};
#else
	int fd_{-1};
#endif
};

#endif
