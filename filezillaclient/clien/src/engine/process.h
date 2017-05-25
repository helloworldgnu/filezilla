#ifndef FILEZILLA_ENGINE_PROCESS_HEADER
#define FILEZILLA_ENGINE_PROCESS_HEADER

/*
The CProcess class manages an asynchronous process with redirected IO.
No console window is being created.

To use, spawn the process and call read from a different thread.
*/

#include <memory>
#include <vector>

class CProcess final
{
public:
	CProcess();
	~CProcess();

	CProcess(CProcess const&) = delete;
	CProcess& operator=(CProcess const&) = delete;

	// args must be properly quoted
	bool Execute(wxString const& cmd, wxString const& args);

	void Kill();

	// Blocking function. Returns Number of bytes read, 0 on EOF, -1 on error.
	int Read(char* buffer, unsigned int len);

	// Blocking function
	bool Write(char const* buffer, unsigned int len);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

#endif
