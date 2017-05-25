#include <filezilla.h>

CConnectCommand::CConnectCommand(CServer const& server, bool retry_connecting /*=true*/)
	: m_Server(server)
	, m_retry_connecting(retry_connecting)
{
}

CServer const& CConnectCommand::GetServer() const
{
	return m_Server;
}

CListCommand::CListCommand(int flags /*=0*/)
	: m_flags(flags)
{
}

CListCommand::CListCommand(CServerPath path, wxString subDir, int flags)
	: m_path(path), m_subDir(subDir), m_flags(flags)
{
}

CServerPath CListCommand::GetPath() const
{
	return m_path;
}

wxString CListCommand::GetSubDir() const
{
	return m_subDir;
}

bool CListCommand::valid() const
{
	if (GetPath().empty() && !GetSubDir().empty())
		return false;

	if (GetFlags() & LIST_FLAG_LINK && GetSubDir().empty())
		return false;

	bool const refresh = (m_flags & LIST_FLAG_REFRESH) != 0;
	bool const avoid = (m_flags & LIST_FLAG_AVOID) != 0;
	if (refresh && avoid)
		return false;

	return true;
}

CFileTransferCommand::CFileTransferCommand(const wxString &localFile, const CServerPath& remotePath,
										   const wxString &remoteFile, bool download,
										   const CFileTransferCommand::t_transferSettings& transferSettings)
	: m_localFile(localFile), m_remotePath(remotePath), m_remoteFile(remoteFile)
	, m_download(download)
	, m_transferSettings(transferSettings)
{
}

wxString CFileTransferCommand::GetLocalFile() const
{
	return m_localFile;
}

CServerPath CFileTransferCommand::GetRemotePath() const
{
	return m_remotePath;
}

wxString CFileTransferCommand::GetRemoteFile() const
{
	return m_remoteFile;
}

bool CFileTransferCommand::Download() const
{
	return m_download;
}

CRawCommand::CRawCommand(const wxString &command)
{
	m_command = command;
}

wxString CRawCommand::GetCommand() const
{
	return m_command;
}

CDeleteCommand::CDeleteCommand(const CServerPath& path, const std::list<wxString>& files)
	: m_path(path), m_files(files)
{
}

CRemoveDirCommand::CRemoveDirCommand(const CServerPath& path, const wxString& subDir)
	: m_path(path), m_subDir(subDir)
{
}

bool CRemoveDirCommand::valid() const
{
	return !GetPath().empty() && !GetSubDir().empty();
}

CMkdirCommand::CMkdirCommand(const CServerPath& path)
	: m_path(path)
{
}

bool CMkdirCommand::valid() const
{
	return !GetPath().empty() && GetPath().HasParent();
}

CRenameCommand::CRenameCommand(const CServerPath& fromPath, const wxString& fromFile,
							   const CServerPath& toPath, const wxString& toFile)
	: m_fromPath(fromPath)
	, m_toPath(toPath)
	, m_fromFile(fromFile)
	, m_toFile(toFile)
{}

bool CRenameCommand::valid() const
{
	return !GetFromPath().empty() && !GetToPath().empty() && !GetFromFile().empty() && !GetToFile().empty();
}

CChmodCommand::CChmodCommand(const CServerPath& path, const wxString& file, const wxString& permission)
	: m_path(path)
	, m_file(file)
	, m_permission(permission)
{}

bool CChmodCommand::valid() const
{
	return !GetPath().empty() && !GetFile().empty() && !GetPermission().empty();
}