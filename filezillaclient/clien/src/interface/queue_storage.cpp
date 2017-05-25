#include <filezilla.h>
#include "queue_storage.h"
#include "Options.h"
#include "queue.h"

#include <sqlite3.h>
#include <wx/wx.h>

#include <unordered_map>

#define INVALID_DATA -1

enum class Column_type
{
	text,
	integer
};

enum _column_flags
{
	not_null = 1,
	autoincrement
};

struct _column
{
	const wxChar* const name;
	Column_type type;
	unsigned int flags;
};

namespace server_table_column_names
{
	enum type
	{
		id,
		host,
		port,
		user,
		password,
		account,
		protocol,
		type,
		logontype,
		timezone_offset,
		transfer_mode,
		max_connections,
		encoding,
		bypass_proxy,
		post_login_commands,
		name
	};
}

_column server_table_columns[] = {
	{ _T("id"), Column_type::integer, not_null | autoincrement },
	{ _T("host"), Column_type::text, not_null },
	{ _T("port"), Column_type::integer, 0 },
	{ _T("user"), Column_type::text, 0 },
	{ _T("password"), Column_type::text, 0 },
	{ _T("account"), Column_type::text, 0 },
	{ _T("protocol"), Column_type::integer, 0 },
	{ _T("type"), Column_type::integer, 0 },
	{ _T("logontype"), Column_type::integer, 0 },
	{ _T("timezone_offset"), Column_type::integer, 0 },
	{ _T("transfer_mode"), Column_type::text, 0 },
	{ _T("max_connections"), Column_type::integer, 0 },
	{ _T("encoding"), Column_type::text, 0 },
	{ _T("bypass_proxy"), Column_type::integer, 0 },
	{ _T("post_login_commands"), Column_type::text, 0 },
	{ _T("name"), Column_type::text, 0 }
};

namespace file_table_column_names
{
	enum type
	{
		id,
		server,
		source_file,
		target_file,
		local_path,
		remote_path,
		download,
		size,
		error_count,
		priority,
		ascii_file,
		default_exists_action
	};
}

_column file_table_columns[] = {
	{ _T("id"), Column_type::integer, not_null | autoincrement },
	{ _T("server"), Column_type::integer, not_null },
	{ _T("source_file"), Column_type::text, 0 },
	{ _T("target_file"), Column_type::text, 0 },
	{ _T("local_path"), Column_type::integer, 0 },
	{ _T("remote_path"), Column_type::integer, 0 },
	{ _T("download"), Column_type::integer, not_null },
	{ _T("size"), Column_type::integer, 0 },
	{ _T("error_count"), Column_type::integer, 0 },
	{ _T("priority"), Column_type::integer, 0 },
	{ _T("ascii_file"), Column_type::integer, 0 },
	{ _T("default_exists_action"), Column_type::integer, 0 }
};

namespace path_table_column_names
{
	enum type
	{
		id,
		path
	};
}

_column path_table_columns[] = {
	{ _T("id"), Column_type::integer, not_null | autoincrement },
	{ _T("path"), Column_type::text, not_null }
};

struct fast_equal
{
	bool operator()(wxString const& lhs, wxString const& rhs) const
	{
		return lhs == rhs;
	}
};

class CQueueStorage::Impl
{
public:
	Impl()
		: db_()
		, insertServerQuery_()
		, insertFileQuery_()
		, insertLocalPathQuery_()
		, insertRemotePathQuery_()
		, selectServersQuery_()
		, selectFilesQuery_()
		, selectLocalPathQuery_()
		, selectRemotePathQuery_()
	{
	}


	void CreateTables();
	wxString CreateColumnDefs(_column* columns, size_t count);

	bool PrepareStatements();

	sqlite3_stmt* PrepareStatement(const wxString& query);
	sqlite3_stmt* PrepareInsertStatement(const wxString& name, const _column*, unsigned int count);

	bool SaveServer(const CServerItem& item);
	bool SaveFile(wxLongLong server, const CFileItem& item);
	bool SaveDirectory(wxLongLong server, const CFolderItem& item);

	int64_t SaveLocalPath(const CLocalPath& path);
	int64_t SaveRemotePath(const CServerPath& path);

	void ReadLocalPaths();
	void ReadRemotePaths();

	const CLocalPath& GetLocalPath(int64_t id) const;
	const CServerPath& GetRemotePath(int64_t id) const;

	bool Bind(sqlite3_stmt* statement, int index, int value);
	bool Bind(sqlite3_stmt* statement, int index, int64_t value);
	bool Bind(sqlite3_stmt* statement, int index, const wxString& value);
	bool Bind(sqlite3_stmt* statement, int index, const char* const value);
	bool BindNull(sqlite3_stmt* statement, int index);

	wxString GetColumnText(sqlite3_stmt* statement, int index, bool shrink = true);
	int64_t GetColumnInt64(sqlite3_stmt* statement, int index, int64_t def = 0);
	int GetColumnInt(sqlite3_stmt* statement, int index, int def = 0);

	int64_t ParseServerFromRow(CServer& server);
	int64_t ParseFileFromRow(CFileItem** pItem);

	bool MigrateSchema();

	sqlite3* db_;

	sqlite3_stmt* insertServerQuery_;
	sqlite3_stmt* insertFileQuery_;
	sqlite3_stmt* insertLocalPathQuery_;
	sqlite3_stmt* insertRemotePathQuery_;

	sqlite3_stmt* selectServersQuery_;
	sqlite3_stmt* selectFilesQuery_;
	sqlite3_stmt* selectLocalPathQuery_;
	sqlite3_stmt* selectRemotePathQuery_;

#ifndef __WXMSW__
	wxMBConvUTF16 utf16_;
#endif

	// Caches to speed up saving and loading
	void ClearCaches();

	std::unordered_map<wxString, int64_t, wxStringHash, fast_equal> localPaths_;
	std::unordered_map<wxString, int64_t, wxStringHash> remotePaths_; // No need for fast_equal as GetSafePath returns unshared string anyhow

	std::map<int64_t, CLocalPath> reverseLocalPaths_;
	std::map<int64_t, CServerPath> reverseRemotePaths_;
};


void CQueueStorage::Impl::ReadLocalPaths()
{
	if (!selectLocalPathQuery_)
		return;

	int res;
	do
	{
		res = sqlite3_step(selectLocalPathQuery_);
		if (res == SQLITE_ROW)
		{
			int64_t id = GetColumnInt64(selectLocalPathQuery_, path_table_column_names::id);
			wxString localPathRaw = GetColumnText(selectLocalPathQuery_, path_table_column_names::path);
			CLocalPath localPath;
			if (id > 0 && !localPathRaw.empty() && localPath.SetPath(localPathRaw))
				reverseLocalPaths_[id] = localPath;
		}
	}
	while (res == SQLITE_BUSY || res == SQLITE_ROW);

	sqlite3_reset(selectLocalPathQuery_);
}


void CQueueStorage::Impl::ReadRemotePaths()
{
	if (!selectRemotePathQuery_)
		return;

	int res;
	do
	{
		res = sqlite3_step(selectRemotePathQuery_);
		if (res == SQLITE_ROW)
		{
			int64_t id = GetColumnInt64(selectRemotePathQuery_, path_table_column_names::id);
			wxString remotePathRaw = GetColumnText(selectRemotePathQuery_, path_table_column_names::path);
			CServerPath remotePath;
			if (id > 0 && !remotePathRaw.empty() && remotePath.SetSafePath(remotePathRaw))
				reverseRemotePaths_[id] = remotePath;
		}
	}
	while (res == SQLITE_BUSY || res == SQLITE_ROW);

	sqlite3_reset(selectRemotePathQuery_);
}


const CLocalPath& CQueueStorage::Impl::GetLocalPath(int64_t id) const
{
	std::map<int64_t, CLocalPath>::const_iterator it = reverseLocalPaths_.find(id);
	if (it != reverseLocalPaths_.end())
		return it->second;

	static const CLocalPath empty;
	return empty;
}


const CServerPath& CQueueStorage::Impl::GetRemotePath(int64_t id) const
{
	std::map<int64_t, CServerPath>::const_iterator it = reverseRemotePaths_.find(id);
	if (it != reverseRemotePaths_.end())
		return it->second;

	static const CServerPath empty;
	return empty;
}


static int int_callback(void* p, int n, char** v, char**)
{
	int* i = static_cast<int*>(p);
	if (!i || !n || !v || !*v)
		return -1;

	*i = atoi(*v);
	return 0;
}


bool CQueueStorage::Impl::MigrateSchema()
{
	int version = 0;

	if (sqlite3_exec(db_, "PRAGMA user_version", int_callback, &version, 0) != SQLITE_OK)
		return false;

	if (version < 1)
		return sqlite3_exec(db_, "PRAGMA user_version = 1", 0, 0, 0) == SQLITE_OK;

	return true;
}


void CQueueStorage::Impl::ClearCaches()
{
	localPaths_.clear();
	remotePaths_.clear();
	reverseLocalPaths_.clear();
	reverseRemotePaths_.clear();
}


int64_t CQueueStorage::Impl::SaveLocalPath(const CLocalPath& path)
{
	std::unordered_map<wxString, int64_t, wxStringHash, fast_equal>::const_iterator it = localPaths_.find(path.GetPath());
	if (it != localPaths_.end())
		return it->second;

	Bind(insertLocalPathQuery_, path_table_column_names::path, path.GetPath());

	int res;
	do {
		res = sqlite3_step(insertLocalPathQuery_);
	} while (res == SQLITE_BUSY);

	sqlite3_reset(insertLocalPathQuery_);

	if (res == SQLITE_DONE)
	{
		int64_t id = sqlite3_last_insert_rowid(db_);
		localPaths_[path.GetPath()] = id;
		return id;
	}

	return -1;
}


int64_t CQueueStorage::Impl::SaveRemotePath(const CServerPath& path)
{
	wxString const& safePath = path.GetSafePath();
	std::unordered_map<wxString, int64_t, wxStringHash>::const_iterator it = remotePaths_.find(safePath);
	if (it != remotePaths_.end())
		return it->second;

	Bind(insertRemotePathQuery_, path_table_column_names::path, safePath);

	int res;
	do {
		res = sqlite3_step(insertRemotePathQuery_);
	} while (res == SQLITE_BUSY);

	sqlite3_reset(insertRemotePathQuery_);

	if (res == SQLITE_DONE) {
		int64_t id = sqlite3_last_insert_rowid(db_);
		remotePaths_[safePath] = id;
		return id;
	}

	return -1;
}


wxString CQueueStorage::Impl::CreateColumnDefs(_column* columns, size_t count)
{
	wxString query = _T("(");
	for (unsigned int i = 0; i < count; ++i) {
		if (i)
			query += _T(", ");
		query += columns[i].name;
		if (columns[i].type == Column_type::integer)
			query += _T(" INTEGER");
		else
			query += _T(" TEXT");

		if (columns[i].flags & autoincrement)
			query += _T(" PRIMARY KEY AUTOINCREMENT");
		if (columns[i].flags & not_null)
			query += _T(" NOT NULL");
	}
	query += _T(")");

	return query;
}

void CQueueStorage::Impl::CreateTables()
{
	if (!db_)
		return;

	{
		wxString query( _T("CREATE TABLE IF NOT EXISTS servers ") );
		query += CreateColumnDefs(server_table_columns, sizeof(server_table_columns) / sizeof(_column));

		if (sqlite3_exec(db_, query.ToUTF8(), 0, 0, 0) != SQLITE_OK)
		{
		}
	}
	{
		wxString query( _T("CREATE TABLE IF NOT EXISTS files ") );
		query += CreateColumnDefs(file_table_columns, sizeof(file_table_columns) / sizeof(_column));

		if (sqlite3_exec(db_, query.ToUTF8(), 0, 0, 0) != SQLITE_OK)
		{
		}

		query = _T("CREATE INDEX IF NOT EXISTS server_index ON files (server)");
		if (sqlite3_exec(db_, query.ToUTF8(), 0, 0, 0) != SQLITE_OK)
		{
		}
	}

	{
		wxString query( _T("CREATE TABLE IF NOT EXISTS local_paths ") );
		query += CreateColumnDefs(path_table_columns, sizeof(path_table_columns) / sizeof(_column));

		if (sqlite3_exec(db_, query.ToUTF8(), 0, 0, 0) != SQLITE_OK)
		{
		}
	}

	{
		wxString query( _T("CREATE TABLE IF NOT EXISTS remote_paths ") );
		query += CreateColumnDefs(path_table_columns, sizeof(path_table_columns) / sizeof(_column));

		if (sqlite3_exec(db_, query.ToUTF8(), 0, 0, 0) != SQLITE_OK)
		{
		}
	}
}

sqlite3_stmt* CQueueStorage::Impl::PrepareInsertStatement(const wxString& name, const _column* columns, unsigned int count)
{
	if (!db_)
		return 0;

	wxString query = _T("INSERT INTO ") + name + _T(" (");
	for (unsigned int i = 1; i < count; ++i)
	{
		if (i > 1)
			query += _T(", ");
		query += columns[i].name;
	}
	query += _T(") VALUES (");
	for (unsigned int i = 1; i < count; ++i)
	{
		if (i > 1)
			query += _T(",");
		query += wxString(_T(":")) + columns[i].name;
	}

	query += _T(")");

	return PrepareStatement(query);
}


sqlite3_stmt* CQueueStorage::Impl::PrepareStatement(const wxString& query)
{
	sqlite3_stmt* ret = 0;

	int res;
	do
	{
		res = sqlite3_prepare_v2(db_, query.ToUTF8(), -1, &ret, 0);
	} while (res == SQLITE_BUSY);

	if (res != SQLITE_OK)
		ret = 0;

	return ret;
}


bool CQueueStorage::Impl::PrepareStatements()
{
	if (!db_)
		return false;

	insertServerQuery_ = PrepareInsertStatement(_T("servers"), server_table_columns, sizeof(server_table_columns) / sizeof(_column));
	insertFileQuery_ = PrepareInsertStatement(_T("files"), file_table_columns, sizeof(file_table_columns) / sizeof(_column));
	insertLocalPathQuery_ = PrepareInsertStatement(_T("local_paths"), path_table_columns, sizeof(path_table_columns) / sizeof(_column));
	insertRemotePathQuery_ = PrepareInsertStatement(_T("remote_paths"), path_table_columns, sizeof(path_table_columns) / sizeof(_column));
	if (!insertServerQuery_ || !insertFileQuery_ || !insertLocalPathQuery_ || !insertRemotePathQuery_)
		return false;

	{
		wxString query = _T("SELECT ");
		for (unsigned int i = 0; i < (sizeof(server_table_columns) / sizeof(_column)); ++i)
		{
			if (i > 0)
				query += _T(", ");
			query += server_table_columns[i].name;
		}

		query += _T(" FROM servers ORDER BY id ASC");

		if (!(selectServersQuery_ = PrepareStatement(query)))
			return false;
	}

	{
		wxString query = _T("SELECT ");
		for (unsigned int i = 0; i < (sizeof(file_table_columns) / sizeof(_column)); ++i)
		{
			if (i > 0)
				query += _T(", ");
			query += file_table_columns[i].name;
		}

		query += _T(" FROM files WHERE server=:server ORDER BY id ASC");

		if (!(selectFilesQuery_ = PrepareStatement(query)))
			return false;
	}

	{
		wxString query = _T("SELECT id, path FROM local_paths");
		if (!(selectLocalPathQuery_ = PrepareStatement(query)))
			return false;
	}

	{
		wxString query = _T("SELECT id, path FROM remote_paths");
		if (!(selectRemotePathQuery_ = PrepareStatement(query)))
			return false;
	}
	return true;
}


bool CQueueStorage::Impl::Bind(sqlite3_stmt* statement, int index, int value)
{
	return sqlite3_bind_int(statement, index, value) == SQLITE_OK;
}


bool CQueueStorage::Impl::Bind(sqlite3_stmt* statement, int index, int64_t value)
{
	int res = sqlite3_bind_int64(statement, index, value);
	return res == SQLITE_OK;
}


#ifndef __WXMSW__
extern "C" {
static void custom_free(void* v)
{
	char* s = static_cast<char*>(v);
	delete [] s;
}
}
#endif

bool CQueueStorage::Impl::Bind(sqlite3_stmt* statement, int index, const wxString& value)
{
#ifdef __WXMSW__
	return sqlite3_bind_text16(statement, index, value.wc_str(), value.length() * 2, SQLITE_TRANSIENT) == SQLITE_OK;
#else
	char* out = new char[value.size() * 2];
	size_t outlen = utf16_.FromWChar(out, value.size() * 2, value.c_str(), value.size());
	bool ret = sqlite3_bind_text16(statement, index, out, outlen, custom_free) == SQLITE_OK;
	return ret;
#endif
}


bool CQueueStorage::Impl::Bind(sqlite3_stmt* statement, int index, const char* const value)
{
	return sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT) == SQLITE_OK;
}


bool CQueueStorage::Impl::BindNull(sqlite3_stmt* statement, int index)
{
	return sqlite3_bind_null(statement, index) == SQLITE_OK;
}


bool CQueueStorage::Impl::SaveServer(const CServerItem& item)
{
	bool kiosk_mode = COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) != 0;

	const CServer& server = item.GetServer();

	Bind(insertServerQuery_, server_table_column_names::host, server.GetHost());
	Bind(insertServerQuery_, server_table_column_names::port, static_cast<int>(server.GetPort()));
	Bind(insertServerQuery_, server_table_column_names::protocol, static_cast<int>(server.GetProtocol()));
	Bind(insertServerQuery_, server_table_column_names::type, static_cast<int>(server.GetType()));

	enum LogonType logonType = server.GetLogonType();
	if (server.GetLogonType() != ANONYMOUS)
	{
		Bind(insertServerQuery_, server_table_column_names::user, server.GetUser());

		if (server.GetLogonType() == NORMAL || server.GetLogonType() == ACCOUNT)
		{
			if (kiosk_mode)
			{
				logonType = ASK;
				BindNull(insertServerQuery_, server_table_column_names::password);
				BindNull(insertServerQuery_, server_table_column_names::account);
			}
			else
			{
				Bind(insertServerQuery_, server_table_column_names::password, server.GetPass());

				if (server.GetLogonType() == ACCOUNT)
					Bind(insertServerQuery_, server_table_column_names::account, server.GetAccount());
				else
					BindNull(insertServerQuery_, server_table_column_names::account);
			}
		}
		else
		{
			BindNull(insertServerQuery_, server_table_column_names::password);
			BindNull(insertServerQuery_, server_table_column_names::account);
		}
	}
	else
	{
			BindNull(insertServerQuery_, server_table_column_names::user);
			BindNull(insertServerQuery_, server_table_column_names::password);
			BindNull(insertServerQuery_, server_table_column_names::account);
	}
	Bind(insertServerQuery_, server_table_column_names::logontype, static_cast<int>(logonType));

	Bind(insertServerQuery_, server_table_column_names::timezone_offset, server.GetTimezoneOffset());

	switch (server.GetPasvMode())
	{
	case MODE_PASSIVE:
		Bind(insertServerQuery_, server_table_column_names::transfer_mode, _T("passive"));
		break;
	case MODE_ACTIVE:
		Bind(insertServerQuery_, server_table_column_names::transfer_mode, _T("active"));
		break;
	default:
		Bind(insertServerQuery_, server_table_column_names::transfer_mode, _T("default"));
		break;
	}
	Bind(insertServerQuery_, server_table_column_names::max_connections, server.MaximumMultipleConnections());

	switch (server.GetEncodingType())
	{
	default:
	case ENCODING_AUTO:
		Bind(insertServerQuery_, server_table_column_names::encoding, _T("Auto"));
		break;
	case ENCODING_UTF8:
		Bind(insertServerQuery_, server_table_column_names::encoding, _T("UTF-8"));
		break;
	case ENCODING_CUSTOM:
		Bind(insertServerQuery_, server_table_column_names::encoding, server.GetCustomEncoding());
		break;
	}

	if (CServer::SupportsPostLoginCommands(server.GetProtocol())) {
		const std::vector<wxString>& postLoginCommands = server.GetPostLoginCommands();
		if (!postLoginCommands.empty()) {
			wxString commands;
			for (std::vector<wxString>::const_iterator iter = postLoginCommands.begin(); iter != postLoginCommands.end(); ++iter) {
				if (!commands.empty())
					commands += _T("\n");
				commands += *iter;
			}
			Bind(insertServerQuery_, server_table_column_names::post_login_commands, commands);
		}
		else
			BindNull(insertServerQuery_, server_table_column_names::post_login_commands);
	}
	else
		BindNull(insertServerQuery_, server_table_column_names::post_login_commands);

	Bind(insertServerQuery_, server_table_column_names::bypass_proxy, server.GetBypassProxy() ? 1 : 0);
	if (!server.GetName().empty())
		Bind(insertServerQuery_, server_table_column_names::name, server.GetName());
	else
		BindNull(insertServerQuery_, server_table_column_names::name);

	int res;
	do {
		res = sqlite3_step(insertServerQuery_);
	} while (res == SQLITE_BUSY);

	sqlite3_reset(insertServerQuery_);

	bool ret = res == SQLITE_DONE;
	if (ret)
	{
		sqlite3_int64 serverId = sqlite3_last_insert_rowid(db_);
		Bind(insertFileQuery_, file_table_column_names::server, static_cast<int64_t>(serverId));

		const std::vector<CQueueItem*>& children = item.GetChildren();
		for (std::vector<CQueueItem*>::const_iterator it = children.begin() + item.GetRemovedAtFront(); it != children.end(); ++it)
		{
			CQueueItem* item = *it;
			if (item->GetType() == QueueItemType::File)
				ret &= SaveFile(serverId, *static_cast<CFileItem*>(item));
			else if (item->GetType() == QueueItemType::Folder)
				ret &= SaveDirectory(serverId, *static_cast<CFolderItem*>(item));
		}
	}
	return ret;
}


bool CQueueStorage::Impl::SaveFile(wxLongLong server, const CFileItem& file)
{
	if (file.m_edit != CEditHandler::none)
		return true;

	Bind(insertFileQuery_, file_table_column_names::source_file, file.GetSourceFile());
	auto const& targetFile = file.GetTargetFile();
	if (targetFile)
		Bind(insertFileQuery_, file_table_column_names::target_file, *targetFile);
	else
		BindNull(insertFileQuery_, file_table_column_names::target_file);

	int64_t localPathId = SaveLocalPath(file.GetLocalPath());
	int64_t remotePathId = SaveRemotePath(file.GetRemotePath());
	if (localPathId == -1 || remotePathId == -1)
		return false;

	Bind(insertFileQuery_, file_table_column_names::local_path, localPathId);
	Bind(insertFileQuery_, file_table_column_names::remote_path, remotePathId);

	Bind(insertFileQuery_, file_table_column_names::download, file.Download() ? 1 : 0);
	if (file.GetSize() != -1)
		Bind(insertFileQuery_, file_table_column_names::size, static_cast<int64_t>(file.GetSize().GetValue()));
	else
		BindNull(insertFileQuery_, file_table_column_names::size);
	if (file.m_errorCount)
		Bind(insertFileQuery_, file_table_column_names::error_count, file.m_errorCount);
	else
		BindNull(insertFileQuery_, file_table_column_names::error_count);
	Bind(insertFileQuery_, file_table_column_names::priority, static_cast<int>(file.GetPriority()));
	Bind(insertFileQuery_, file_table_column_names::ascii_file, file.Ascii() ? 1 : 0);

	if (file.m_defaultFileExistsAction != CFileExistsNotification::unknown)
		Bind(insertFileQuery_, file_table_column_names::default_exists_action, file.m_defaultFileExistsAction);
	else
		BindNull(insertFileQuery_, file_table_column_names::default_exists_action);

	int res;
	do {
		res = sqlite3_step(insertFileQuery_);
	} while (res == SQLITE_BUSY);

	sqlite3_reset(insertFileQuery_);

	return res == SQLITE_DONE;
}


bool CQueueStorage::Impl::SaveDirectory(wxLongLong server, const CFolderItem& directory)
{
	if (directory.Download())
		BindNull(insertFileQuery_, file_table_column_names::source_file);
	else
		Bind(insertFileQuery_, file_table_column_names::source_file, directory.GetSourceFile());
	BindNull(insertFileQuery_, file_table_column_names::target_file);

	int64_t localPathId = directory.Download() ? SaveLocalPath(directory.GetLocalPath()) : -1;
	int64_t remotePathId = directory.Download() ? -1 : SaveRemotePath(directory.GetRemotePath());
	if (localPathId == -1 && remotePathId == -1)
		return false;

	Bind(insertFileQuery_, file_table_column_names::local_path, localPathId);
	Bind(insertFileQuery_, file_table_column_names::remote_path, remotePathId);

	Bind(insertFileQuery_, file_table_column_names::download, directory.Download() ? 1 : 0);
	BindNull(insertFileQuery_, file_table_column_names::size);
	if (directory.m_errorCount)
		Bind(insertFileQuery_, file_table_column_names::error_count, directory.m_errorCount);
	else
		BindNull(insertFileQuery_, file_table_column_names::error_count);
	Bind(insertFileQuery_, file_table_column_names::priority, static_cast<int>(directory.GetPriority()));
	BindNull(insertFileQuery_, file_table_column_names::ascii_file);

	BindNull(insertFileQuery_, file_table_column_names::default_exists_action);

	int res;
	do {
		res = sqlite3_step(insertFileQuery_);
	} while (res == SQLITE_BUSY);

	sqlite3_reset(insertFileQuery_);

	return res == SQLITE_DONE;
}


wxString CQueueStorage::Impl::GetColumnText(sqlite3_stmt* statement, int index, bool shrink)
{
	wxString ret;

#ifdef __WXMSW__
	(void)shrink;
	const wxChar* text = static_cast<const wxChar*>(sqlite3_column_text16(statement, index));
	if (text)
		ret = text;
#else
	const char* text = static_cast<const char*>(sqlite3_column_text16(statement, index));
	int len = sqlite3_column_bytes16(statement, index);
	if (text)
	{
		wxStringBuffer buffer(ret, len);
		wxChar* out = buffer;

		int outlen = utf16_.ToWChar( out, len, text, len );
		buffer[outlen] = 0;
	}
	if (shrink)
		ret.Shrink();
#endif

	return ret;
}

int64_t CQueueStorage::Impl::GetColumnInt64(sqlite3_stmt* statement, int index, int64_t def)
{
	if (sqlite3_column_type(statement, index) == SQLITE_NULL)
		return def;
	else
		return sqlite3_column_int64(statement, index);
}

int CQueueStorage::Impl::GetColumnInt(sqlite3_stmt* statement, int index, int def)
{
	if (sqlite3_column_type(statement, index) == SQLITE_NULL)
		return def;
	else
		return sqlite3_column_int(statement, index);
}

int64_t CQueueStorage::Impl::ParseServerFromRow(CServer& server)
{
	server = CServer();

	wxString host = GetColumnText(selectServersQuery_, server_table_column_names::host);
	if (host.empty())
		return INVALID_DATA;

	int port = GetColumnInt(selectServersQuery_, server_table_column_names::port);
	if (port < 1 || port > 65535)
		return INVALID_DATA;

	if (!server.SetHost(host, port))
		return INVALID_DATA;

	int const protocol = GetColumnInt(selectServersQuery_, server_table_column_names::protocol);
	if (protocol < 0 || protocol > MAX_VALUE)
		return INVALID_DATA;
	server.SetProtocol(static_cast<ServerProtocol>(protocol));

	int type = GetColumnInt(selectServersQuery_, server_table_column_names::type);
	if (type < 0 || type >= SERVERTYPE_MAX)
		return INVALID_DATA;

	server.SetType((enum ServerType)type);

	int logonType = GetColumnInt(selectServersQuery_, server_table_column_names::logontype);
	if (logonType < 0 || logonType >= LOGONTYPE_MAX)
		return INVALID_DATA;

	server.SetLogonType((enum LogonType)logonType);

	if (server.GetLogonType() != ANONYMOUS)
	{
		wxString user = GetColumnText(selectServersQuery_, server_table_column_names::user);

		wxString pass;
		if ((long)NORMAL == logonType || (long)ACCOUNT == logonType)
			pass = GetColumnText(selectServersQuery_, server_table_column_names::password);

		if (!server.SetUser(user, pass))
			return INVALID_DATA;

		if ((long)ACCOUNT == logonType)
		{
			wxString account = GetColumnText(selectServersQuery_, server_table_column_names::account);
			if (account.empty())
				return INVALID_DATA;
			if (!server.SetAccount(account))
				return INVALID_DATA;
		}
	}

	int timezoneOffset = GetColumnInt(selectServersQuery_, server_table_column_names::timezone_offset);
	if (!server.SetTimezoneOffset(timezoneOffset))
		return INVALID_DATA;

	wxString pasvMode = GetColumnText(selectServersQuery_, server_table_column_names::transfer_mode);
	if (pasvMode == _T("passive"))
		server.SetPasvMode(MODE_PASSIVE);
	else if (pasvMode == _T("active"))
		server.SetPasvMode(MODE_ACTIVE);
	else
		server.SetPasvMode(MODE_DEFAULT);

	int maximumMultipleConnections = GetColumnInt(selectServersQuery_, server_table_column_names::max_connections);
	if (maximumMultipleConnections < 0)
		return INVALID_DATA;
	server.MaximumMultipleConnections(maximumMultipleConnections);

	wxString encodingType = GetColumnText(selectServersQuery_, server_table_column_names::encoding);
	if (encodingType.empty() || encodingType == _T("Auto"))
		server.SetEncodingType(ENCODING_AUTO);
	else if (encodingType == _T("UTF-8"))
		server.SetEncodingType(ENCODING_UTF8);
	else
	{
		if (!server.SetEncodingType(ENCODING_CUSTOM, encodingType))
			return INVALID_DATA;
	}

	if (CServer::SupportsPostLoginCommands(server.GetProtocol())) {
		std::vector<wxString> postLoginCommands;

		wxString commands = GetColumnText(selectServersQuery_, server_table_column_names::post_login_commands);
		while (!commands.empty())
		{
			int pos = commands.Find('\n');
			if (!pos)
				commands = commands.Mid(1);
			else if (pos == -1)
			{
				postLoginCommands.push_back(commands);
				commands.clear();
			}
			else
			{
				postLoginCommands.push_back(commands.Left(pos));
				commands = commands.Mid(pos + 1);
			}
		}
		if (!server.SetPostLoginCommands(postLoginCommands))
			return INVALID_DATA;
	}


	server.SetBypassProxy(GetColumnInt(selectServersQuery_, server_table_column_names::bypass_proxy) == 1 );
	server.SetName( GetColumnText(selectServersQuery_, server_table_column_names::name) );

	return GetColumnInt64(selectServersQuery_, server_table_column_names::id);
}


int64_t CQueueStorage::Impl::ParseFileFromRow(CFileItem** pItem)
{
	wxString sourceFile = GetColumnText(selectFilesQuery_, file_table_column_names::source_file);
	wxString targetFile = GetColumnText(selectFilesQuery_, file_table_column_names::target_file);

	int64_t localPathId = GetColumnInt64(selectFilesQuery_, file_table_column_names::local_path, false);
	int64_t remotePathId = GetColumnInt64(selectFilesQuery_, file_table_column_names::remote_path, false);

	const CLocalPath& localPath(GetLocalPath(localPathId));
	const CServerPath& remotePath(GetRemotePath(remotePathId));

	bool download = GetColumnInt(selectFilesQuery_, file_table_column_names::download) != 0;

	if (localPathId == -1 || remotePathId == -1)
	{
		// QueueItemType::Folder
		if ((download && localPath.empty()) ||
			(!download && remotePath.empty()))
		{
			return INVALID_DATA;
		}

		if (download)
			*pItem = new CFolderItem(0, true, localPath);
		else
			*pItem = new CFolderItem(0, true, remotePath, sourceFile);
	}
	else
	{
		wxLongLong size = GetColumnInt64(selectFilesQuery_, file_table_column_names::size);
		unsigned char errorCount = static_cast<unsigned char>(GetColumnInt(selectFilesQuery_, file_table_column_names::error_count));
		int priority = GetColumnInt(selectFilesQuery_, file_table_column_names::priority, static_cast<int>(QueuePriority::normal));

		bool ascii = GetColumnInt(selectFilesQuery_, file_table_column_names::ascii_file) != 0;
		int overwrite_action = GetColumnInt(selectFilesQuery_, file_table_column_names::default_exists_action, CFileExistsNotification::unknown);

		if (sourceFile.empty() || localPath.empty() ||
			remotePath.empty() ||
			size < -1 ||
			priority < 0 || priority >= static_cast<int>(QueuePriority::count))
		{
			return INVALID_DATA;
		}

		CFileItem* fileItem = new CFileItem(0, true, download, sourceFile, targetFile, localPath, remotePath, size);
		*pItem = fileItem;
		fileItem->SetAscii(ascii);
		fileItem->SetPriorityRaw(QueuePriority(priority));
		fileItem->m_errorCount = errorCount;

		if (overwrite_action > 0 && overwrite_action < CFileExistsNotification::ACTION_COUNT)
			fileItem->m_defaultFileExistsAction = (CFileExistsNotification::OverwriteAction)overwrite_action;
	}

	return GetColumnInt64(selectFilesQuery_, file_table_column_names::id);
}

CQueueStorage::CQueueStorage()
: d_(new Impl)
{
	int ret = sqlite3_open(GetDatabaseFilename().ToUTF8(), &d_->db_ );
	if (ret != SQLITE_OK)
		d_->db_ = 0;

	if (sqlite3_exec(d_->db_, "PRAGMA encoding=\"UTF-16le\"", 0, 0, 0) == SQLITE_OK)
	{
		d_->MigrateSchema();
		d_->CreateTables();
		d_->PrepareStatements();
	}
}

CQueueStorage::~CQueueStorage()
{
	sqlite3_finalize(d_->insertServerQuery_);
	sqlite3_finalize(d_->insertFileQuery_);
	sqlite3_finalize(d_->insertLocalPathQuery_);
	sqlite3_finalize(d_->insertRemotePathQuery_);
	sqlite3_finalize(d_->selectServersQuery_);
	sqlite3_finalize(d_->selectFilesQuery_);
	sqlite3_finalize(d_->selectLocalPathQuery_);
	sqlite3_finalize(d_->selectRemotePathQuery_);
	sqlite3_close(d_->db_);
	delete d_;
}

bool CQueueStorage::SaveQueue(std::vector<CServerItem*> const& queue)
{
	d_->ClearCaches();

	bool ret = true;
	if (sqlite3_exec(d_->db_, "BEGIN TRANSACTION", 0, 0, 0) == SQLITE_OK) {
		for (std::vector<CServerItem*>::const_iterator it = queue.begin(); it != queue.end(); ++it)
			ret &= d_->SaveServer(**it);

		// Even on previous failure, we want to at least try to commit the data we have so far
		ret &= sqlite3_exec(d_->db_, "END TRANSACTION", 0, 0, 0) == SQLITE_OK;

		d_->ClearCaches();
	}
	else
		ret = false;

	return ret;
}

int64_t CQueueStorage::GetServer(CServer& server, bool fromBeginning)
{
	int64_t ret = -1;

	if (d_->selectServersQuery_)
	{
		if (fromBeginning)
		{
			d_->ReadLocalPaths();
			d_->ReadRemotePaths();
			sqlite3_reset(d_->selectServersQuery_);
		}

		for (;;)
		{
			int res;
			do
			{
				res = sqlite3_step(d_->selectServersQuery_);
			}
			while (res == SQLITE_BUSY);

			if (res == SQLITE_ROW)
			{
				ret = d_->ParseServerFromRow(server);
				if (ret > 0)
					break;
			}
			else if (res == SQLITE_DONE)
			{
				ret = 0;
				sqlite3_reset(d_->selectServersQuery_);
				break;
			}
			else
			{
				ret = -1;
				sqlite3_reset(d_->selectServersQuery_);
				break;
			}
		}
	}
	else {
		ret = -1;
	}

	return ret;
}


int64_t CQueueStorage::GetFile(CFileItem** pItem, int64_t server)
{
	int64_t ret = -1;
	*pItem = 0;

	if (d_->selectFilesQuery_)
	{
		if (server > 0)
		{
			sqlite3_reset(d_->selectFilesQuery_);
			sqlite3_bind_int64(d_->selectFilesQuery_, 1, server);
		}

		for (;;)
		{
			int res;
			do
			{
				res = sqlite3_step(d_->selectFilesQuery_);
			}
			while (res == SQLITE_BUSY);

			if (res == SQLITE_ROW)
			{
				ret = d_->ParseFileFromRow(pItem);
				if (ret > 0)
					break;
			}
			else if (res == SQLITE_DONE)
			{
				ret = 0;
				sqlite3_reset(d_->selectFilesQuery_);
				break;
			}
			else
			{
				ret = -1;
				sqlite3_reset(d_->selectFilesQuery_);
				break;
			}
		}
	}
	else {
		ret = -1;
	}

	return ret;
}

bool CQueueStorage::Clear()
{
	if (!d_->db_)
		return false;

	if (sqlite3_exec(d_->db_, "DELETE FROM files", 0, 0, 0) != SQLITE_OK)
		return false;

	if (sqlite3_exec(d_->db_, "DELETE FROM servers", 0, 0, 0) != SQLITE_OK)
		return false;

	if (sqlite3_exec(d_->db_, "DELETE FROM local_paths", 0, 0, 0) != SQLITE_OK)
		return false;

	if (sqlite3_exec(d_->db_, "DELETE FROM remote_paths", 0, 0, 0) != SQLITE_OK)
		return false;

	d_->ClearCaches();

	return true;
}

wxString CQueueStorage::GetDatabaseFilename()
{
	wxFileName file(COptions::Get()->GetOption(OPTION_DEFAULT_SETTINGSDIR), _T("queue.sqlite3"));

	return file.GetFullPath();
}

bool CQueueStorage::BeginTransaction()
{
	return sqlite3_exec(d_->db_, "BEGIN TRANSACTION", 0, 0, 0) == SQLITE_OK;
}

bool CQueueStorage::EndTransaction()
{
	return sqlite3_exec(d_->db_, "END TRANSACTION", 0, 0, 0) == SQLITE_OK;
}

bool CQueueStorage::Vacuum()
{
	return sqlite3_exec(d_->db_, "VACUUM", 0, 0, 0) == SQLITE_OK;
}
