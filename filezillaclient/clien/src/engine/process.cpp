#include <filezilla.h>
#include "process.h"

#ifdef __WXMSW__

namespace {
void ResetHandle(HANDLE& handle)
{
	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}
};

bool Uninherit(HANDLE& handle)
{
	if (handle != INVALID_HANDLE_VALUE) {
		HANDLE newHandle = INVALID_HANDLE_VALUE;

		if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &newHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			newHandle = INVALID_HANDLE_VALUE;
		}
		CloseHandle(handle);
		handle = newHandle;
	}

	return handle != INVALID_HANDLE_VALUE;
}

class Pipe final
{
public:
	Pipe() = default;

	~Pipe()
	{
		reset();
	}

	Pipe(Pipe const&) = delete;
	Pipe& operator=(Pipe const&) = delete;

	bool Create(bool local_is_input)
	{
		reset();

		SECURITY_ATTRIBUTES sa{};
		sa.bInheritHandle = TRUE;
		sa.nLength = sizeof(sa);

		BOOL res = CreatePipe(&read_, &write_, &sa, 0);
		if (res) {
			// We only want one side of the pipe to be inheritable
			if (!Uninherit(local_is_input ? read_ : write_)) {
				reset();
			}
		}
		else {
			read_ = INVALID_HANDLE_VALUE;
			write_ = INVALID_HANDLE_VALUE;
		}
		return valid();
	}

	bool valid() const {
		return read_ != INVALID_HANDLE_VALUE && write_ != INVALID_HANDLE_VALUE;
	}

	void reset()
	{
		ResetHandle(read_);
		ResetHandle(write_);
	}

	HANDLE read_{INVALID_HANDLE_VALUE};
	HANDLE write_{INVALID_HANDLE_VALUE};
};
}

class CProcess::Impl
{
public:
	Impl() = default;
	~Impl()
	{
		Kill();
	}

	Impl(Impl const&) = delete;
	Impl& operator=(Impl const&) = delete;

	bool CreatePipes()
	{
		return
			in_.Create(false) &&
			out_.Create(true) &&
			err_.Create(true);
	}

	bool Execute(wxString const& cmd, wxString const& args)
	{
		DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_DEFAULT_ERROR_MODE | CREATE_NO_WINDOW;

		if (!CreatePipes()) {
			return false;
		}

		STARTUPINFO si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = in_.read_;
		si.hStdOutput = out_.write_;
		si.hStdError = err_.write_;

		auto cmdline = GetCmdLine(cmd, args);

		PROCESS_INFORMATION pi{};
		BOOL res = CreateProcess(cmd, cmdline.get(), 0, 0, TRUE, flags, 0, 0, &si, &pi);
		if (!res) {
			return false;
		}

		process_ = pi.hProcess;

		// We don't need to use these
		ResetHandle(pi.hThread);
		ResetHandle(in_.read_);
		ResetHandle(out_.write_);
		ResetHandle(err_.write_);

		return true;
	}

	void Kill()
	{
		if (process_ != INVALID_HANDLE_VALUE) {
			in_.reset();
			if (WaitForSingleObject(process_, 500) == WAIT_TIMEOUT) {
				TerminateProcess(process_, 0);
			}
			ResetHandle(process_);
			out_.reset();
			err_.reset();
		}
	}

	int Read(char* buffer, unsigned int len)
	{
		DWORD read = 0;
		BOOL res = ReadFile(out_.read_, buffer, len, &read, 0);
		if (!res) {
			return -1;
		}
		return read;
	}

	bool Write(char const* buffer, unsigned int len)
	{
		while (len > 0) {
			DWORD written = 0;
			BOOL res = WriteFile(in_.write_, buffer, len, &written, 0);
			if (!res || written == 0) {
				return false;
			}
			buffer += written;
			len -= written;
		}
		return true;
	}

private:
	std::unique_ptr<wxChar[]> GetCmdLine(wxString const& cmd, wxString const& args)
	{
		wxString cmdline = _T("\"") + cmd + _T("\" ") + args;
		std::unique_ptr<wxChar[]> ret;
		ret.reset(new wxChar[cmdline.size() + 1]);
		wxStrcpy(ret.get(), cmdline);
		return ret;
	}

	HANDLE process_{INVALID_HANDLE_VALUE};

	Pipe in_;
	Pipe out_;
	Pipe err_;
};

#else

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

namespace {
void ResetFd(int& fd)
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

class Pipe final
{
public:
	Pipe() = default;

	~Pipe()
	{
		reset();
	}

	Pipe(Pipe const&) = delete;
	Pipe& operator=(Pipe const&) = delete;

	bool Create()
	{
		reset();

		int fds[2];
		if (pipe(fds) != 0) {
			return false;
		}

		read_ = fds[0];
		write_ = fds[1];

		return valid();
	}

	bool valid() const {
		return read_ != -1 && write_ != -1;
	}

	void reset()
	{
		ResetFd(read_);
		ResetFd(write_);
	}

	int read_{-1};
	int write_{-1};
};
}

class CProcess::Impl
{
public:
	Impl() = default;
	~Impl()
	{
		Kill();
	}

	Impl(Impl const&) = delete;
	Impl& operator=(Impl const&) = delete;

	bool CreatePipes()
	{
		return
			in_.Create() &&
			out_.Create() &&
			err_.Create();
	}

	bool Execute(wxString const& cmd, wxString const& args)
	{
		if (!CreatePipes()) {
			return false;
		}

		int pid = fork();
		if (pid < 0) {
			return false;
		}
		else if (!pid) {
			// We're the child.

			// Close uneeded descriptors
			ResetFd(in_.write_);
			ResetFd(out_.read_);
			ResetFd(err_.read_);

			// Redirect to pipe
			if (dup2(in_.read_, STDIN_FILENO) == -1 ||
				dup2(out_.write_, STDOUT_FILENO) == -1 ||
				dup2(err_.write_, STDERR_FILENO) == -1)
			{
				_exit(-1);
			}

			// Execute process
			execl(cmd, args, (char*)0); // noreturn on success

			_exit(-1);
		}
		else {
			// We're the parent
			pid_ = pid;

			// Close unneeded descriptors
			ResetFd(in_.read_);
			ResetFd(out_.write_);
			ResetFd(err_.write_);
		}

		return true;
	}

	void Kill()
	{
		in_.reset();

		if (pid_ != -1) {
			kill(pid_, SIGTERM);

			int ret;
			do {
			}
			while((ret = waitpid(pid_, 0, 0)) == -1 && errno == EINTR);

			(void)ret;

			pid_ = -1;
		}

		out_.reset();
		err_.reset();
	}

	int Read(char* buffer, unsigned int len)
	{
		int r;
		do {
			r = read(out_.read_, buffer, len);
		}
		while (r == -1 && (errno == EAGAIN || errno == EINTR));

		return r;
	}

	bool Write(char const* buffer, unsigned int len)
	{
		while (len) {
			int written;
			do {
				written = write(in_.write_, buffer, len);
			}
			while (written == -1 && (errno == EAGAIN || errno == EINTR));

			if (written <= 0) {
				return false;
			}

			len -= written;
			buffer += written;
		}
		return true;
	}

	Pipe in_;
	Pipe out_;
	Pipe err_;

	int pid_{-1};
};

#endif


CProcess::CProcess()
	: impl_(make_unique<Impl>())
{
}

CProcess::~CProcess()
{
	impl_.reset();
}

bool CProcess::Execute(wxString const& cmd, wxString const& args)
{
	return impl_ ? impl_->Execute(cmd, args) : false;
}

void CProcess::Kill()
{
	if (impl_) {
		impl_->Kill();
	}
}

int CProcess::Read(char* buffer, unsigned int len)
{
	return impl_ ? impl_->Read(buffer, len) : -1;
}

bool CProcess::Write(char const* buffer, unsigned int len)
{
	return impl_ ? impl_->Write(buffer, len) : false;
}
