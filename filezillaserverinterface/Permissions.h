// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2016 - Tim Kosse <tim.kosse@filezilla-project.org>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// Permissions.h: Schnittstelle für die Klasse CPermissions.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PERMISSIONS_H__33DEA50E_AA34_4190_9ACD_355BF3D72FE0__INCLUDED_)
#define AFX_PERMISSIONS_H__33DEA50E_AA34_4190_9ACD_355BF3D72FE0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Accounts.h"

#include <functional>

#define FOP_READ		0x01
#define FOP_WRITE		0x02
#define FOP_DELETE		0x04
#define FOP_APPEND		0x08
#define FOP_CREATENEW	0x10
#define DOP_DELETE		0x20
#define DOP_CREATE		0x40
#define FOP_LIST		0x80

#define PERMISSION_DENIED			0x01
#define PERMISSION_NOTFOUND			0x02
#define PERMISSION_DIRNOTFILE		(0x04 | PERMISSION_DOESALREADYEXIST)
#define PERMISSION_FILENOTDIR		(0x08 | PERMISSION_DOESALREADYEXIST)
#define PERMISSION_DOESALREADYEXIST	0x10
#define PERMISSION_INVALIDNAME		0x20

#define MD5_HEX_FORM_LENGTH			 32
#define SHA512_HEX_FORM_LENGTH		128		//Length of the hex string representation of a SHA512 hash

class TiXmlElement;
class CPermissionsHelperWindow;
class COptions;

class CUser final : public t_user
{
public:
	CStdString homedir;

	// Replace :u and :g (if a group it exists)
	void DoReplacements(CStdString& path) const;

	/*
	 * t_alias is used in the alias maps.
	 * See implementation of PrepareAliasMap for a detailed
	 * description
	 */
	struct t_alias
	{
		CStdString targetFolder;
		CStdString name;
	};

	void PrepareAliasMap();

	// GetAliasTarget returns the target of the alias with the specified
	// path or returns an empty string if the alias can't be found.
	CStdString GetAliasTarget(const CStdString& virtualPath) const;

	std::map<CStdString, CStdString> virtualAliases;
	std::multimap<CStdString, CStdString> virtualAliasNames;
};

struct t_dirlisting
{
	t_dirlisting()
		: len()
	{
	}

	char buffer[8192];
	unsigned int len;
};

enum _facts {
	fact_type,
	fact_size,
	fact_modify,
	fact_perm
};

class CPermissions final
{
public:
	CPermissions(std::function<void()> const& updateCallback);
	~CPermissions();

	typedef void (*addFunc_t)(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *enabledFacts);
protected:
	/*
	 * CanonifyPath takes the current and the new server dir as parameter,
	 * concats the paths if neccessary and canonifies the dir:
	 * - remove dot-segments
	 * - convert backslashes into slashes
	 * - remove double slashes
	 */
	CStdString CanonifyServerDir(CStdString currentDir, CStdString newDir) const;

public:
	// Change current directory to the specified directory. Used by CWD and CDUP
	int ChangeCurrentDir(CUser const& user, CStdString& currentdir, CStdString &dir);

	// Retrieve a directory listing. Pass the actual formatting function as last parameter.
	int GetDirectoryListing(CUser const& user, CStdString currentDir, CStdString dirToDisplay,
							 std::list<t_dirlisting> &result, CStdString& physicalDir,
							 CStdString& logicalDir,
							 addFunc_t addFunc,
							 bool *enabledFacts = 0);

	// Full directory listing with all details. Used by LIST command
	static void AddLongListingEntry(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *);

	// Directory listing with just the filenames. Used by NLST command
	static void AddShortListingEntry(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *);

	// Directory listing format used by MLSD
	static void AddFactsListingEntry(std::list<t_dirlisting> &result, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *enabledFacts);

	int CheckDirectoryPermissions(CUser const& user, CStdString dirname, CStdString currentdir, int op, CStdString &physicalDir, CStdString &logicalDir);
	int CheckFilePermissions(CUser const& user, CStdString filename, CStdString currentdir, int op, CStdString &physicalDir, CStdString &logicalDir);

	CUser GetUser(CStdString const& username) const;
	bool CheckUserLogin(CUser const& user, LPCTSTR pass, BOOL noPasswordCheck = FALSE);

	bool GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength);
	BOOL ParseUsersCommand(unsigned char *pData, DWORD dwDataLength);
	void AutoCreateDirs(CUser const& user);
	void ReloadConfig();

	int GetFact(CUser const& user, CStdString const& currentDir, CStdString file, CStdString& fact, CStdString& logicalName, bool enabledFacts[3]);

protected:
	CStdString GetHomeDir(CUser const& user) const;

	bool Init();
	void UpdateInstances();
	void UpdatePermissions(bool notifyOwner);

	void ReadSettings();
	bool SaveSettings();

	void ReadPermissions(TiXmlElement *pXML, t_group &user, BOOL &bGotHome);
	void SavePermissions(TiXmlElement *pXML, const t_group &user);

	void ReadSpeedLimits(TiXmlElement *pXML, t_group &group);
	void SaveSpeedLimits(TiXmlElement *pXML, const t_group &group);

	void ReadIpFilter(TiXmlElement *pXML, t_group &group);
	void SaveIpFilter(TiXmlElement *pXML, const t_group &group);

	void SetKey(TiXmlElement *pXML, LPCTSTR name, LPCTSTR value);
	void SetKey(TiXmlElement *pXML, LPCTSTR name, int value);

	int GetRealDirectory(CStdString directory, const CUser &user, t_directory &ret, BOOL &truematch);

	static std::recursive_mutex m_mutex;

	bool WildcardMatch(CStdString string, CStdString pattern) const;

	typedef std::map<CStdString, CUser> t_UsersList;
	typedef std::vector<t_group> t_GroupsList;
	static t_UsersList m_sUsersList;
	static t_GroupsList m_sGroupsList;
	t_UsersList m_UsersList;
	t_GroupsList m_GroupsList;

	static std::list<CPermissions *> m_sInstanceList;
	CPermissionsHelperWindow *m_pPermissionsHelperWindow;

	friend CPermissionsHelperWindow;

	std::function<void()> const updateCallback_;
};

#endif // !defined(AFX_PERMISSIONS_H__33DEA50E_AA34_4190_9ACD_355BF3D72FE0__INCLUDED_)
