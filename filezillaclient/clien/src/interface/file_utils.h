#ifndef FILEZILLA_FILE_UTILS_HEADER
#define FILEZILLA_FILE_UTILS_HEADER

bool UnquoteCommand(wxString& command, wxString& arguments, bool is_dde = false);
bool ProgramExists(const wxString& editor);
bool PathExpand(wxString& cmd);

wxString GetSystemOpenCommand(wxString file, bool &program_exists);

// Returns a file:// URL
wxString GetAsURL(const wxString& dir);

// Opens specified directory in local file manager, e.g. Explorer on Windows
bool OpenInFileManager(const wxString& dir);

bool RenameFile(wxWindow* pWnd, wxString dir, wxString from, wxString to);

CLocalPath GetDownloadDir();

#endif
