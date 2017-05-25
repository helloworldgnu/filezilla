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

// UsersListCtrl.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "UsersListCtrl.h"
#include "mainfrm.h"
#include "OutputFormat.h"

#if defined(_DEBUG) 
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define NUMCOLUMNS 6
#define COLUMN_ID 0
#define COLUMN_USER 1
#define COLUMN_IP 2
#define COLUMN_TRANSFERINIT 3
#define COLUMN_TRANSFERPROGRESS 4
#define COLUMN_TRANSFERSPEED 5

#define SPEED_MEAN_SECONDS 10

class CConnectionData
{
public:
	CConnectionData()
	{
		for (int i = 1; i < NUMCOLUMNS; i++)
			itemImages[i] = -1;
		itemImages[0] = 5;

		ResetSpeed();
	}

	~CConnectionData() { }
	int userid;
	unsigned int port;
	unsigned char transferMode;
	CString physicalFile;
	CString logicalFile;
	__int64 totalSize;
	__int64 currentOffset;
	unsigned int speed;

	int listIndex;
	CString columnText[NUMCOLUMNS];
	int itemImages[NUMCOLUMNS];

	inline void AddBytes(int bytes)
	{
		*current_speed += bytes;
		UpdateSpeed();
	}

	inline void UpdateSpeed()
	{
		speed = 0;
		int max = speedDidWrap ? SPEED_MEAN_SECONDS : (current_speed - speed_mean + 1);
		for (int i = 0; i < max; i++)
			speed += speed_mean[i];
		speed /= max;
	}

	inline void NextSpeed()
	{
		if (!*current_speed)
			UpdateSpeed();

		if ((++current_speed - speed_mean) >= SPEED_MEAN_SECONDS)
		{
			speedDidWrap = true;
			current_speed = speed_mean;
		}
		*current_speed = 0;
	}

	inline void ResetSpeed()
	{
		speedDidWrap = false;
		current_speed = speed_mean;
		*current_speed = 0;
	}

private:
	unsigned int speed_mean[SPEED_MEAN_SECONDS];
	unsigned int *current_speed;
	bool speedDidWrap;
};

/////////////////////////////////////////////////////////////////////////////
// CUsersListCtrl

CUsersListCtrl::CUsersListCtrl(CMainFrame *pOwner)
{
	ASSERT(pOwner);
	m_pOwner = pOwner;
	m_sortColumn = 0;
	m_sortDir = 0;
}

CUsersListCtrl::~CUsersListCtrl()
{
	for (std::vector<CConnectionData*>::iterator iter = m_connectionDataArray.begin(); iter != m_connectionDataArray.end(); ++iter)
		delete *iter;
}


BEGIN_MESSAGE_MAP(CUsersListCtrl, CListCtrl)
	//{{AFX_MSG_MAP(CUsersListCtrl)
	ON_WM_CREATE()
	ON_COMMAND(ID_USERVIEWCONTEXT_KICK, OnContextmenuKick)
	ON_COMMAND(ID_USERVIEWCONTEXT_BAN, OnContextmenuBan)
	ON_WM_CONTEXTMENU()
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnGetdispinfo)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnclick)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten CUsersListCtrl

int CUsersListCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CListCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_ImageList.Create(IDB_TRANSFERINFO, 16, 6, RGB(255, 0, 255));
	SetImageList(&m_ImageList, LVSIL_SMALL);

	SetExtendedStyle(LVS_EX_LABELTIP | LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);

	InsertColumn(COLUMN_ID, _T("ID"), LVCFMT_RIGHT, 75);
	InsertColumn(COLUMN_USER, _T("Account"), LVCFMT_LEFT, 150);
	InsertColumn(COLUMN_IP, _T("IP"), LVCFMT_RIGHT, 100);
	InsertColumn(COLUMN_TRANSFERINIT, _T("Transfer"), LVCFMT_LEFT, 250);
	InsertColumn(COLUMN_TRANSFERPROGRESS, _T("Progress"), LVCFMT_RIGHT, 150);
	InsertColumn(COLUMN_TRANSFERSPEED, _T("Speed"), LVCFMT_LEFT, 80);

	m_SortImg.Create( 8, 8, ILC_MASK, 3, 3 );
	HICON Icon;
	Icon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_EMPTY));
	m_SortImg.Add(Icon);
	Icon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_UP));
	m_SortImg.Add(Icon);
	Icon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DOWN));
	m_SortImg.Add(Icon);
	m_SortImg.SetBkColor(CLR_NONE);

	CHeaderCtrl *header = GetHeaderCtrl( );
	if (header)
		header->SetImageList(&m_SortImg);

	m_nSpeedinfoTimer = SetTimer(232, 1000, 0);

	return 0;
}

bool CUsersListCtrl::ProcessConnOp(unsigned char *pData, DWORD dwDataLength)
{
	int op = pData[1];

	if (op < 0 || op > 4)
		return FALSE;

	if (dwDataLength < 6)
		return FALSE;

	if (op == USERCONTROL_CONNOP_ADD)
	{
		int userid;
		memcpy(&userid, pData + 2, 4);
		CConnectionData* pConnectionData = new CConnectionData;
		pConnectionData->currentOffset = 0;
		pConnectionData->totalSize = -1;

		pConnectionData->userid = userid;

		unsigned int pos = 6;

		if (dwDataLength < 8)
		{
			delete pConnectionData;
			return FALSE;
		}

		unsigned int len = pData[pos] * 256 + pData[pos+1];
		pos += 2;
		if (pos+len > dwDataLength)
		{
			delete pConnectionData;
			return FALSE;
		}

		char* ip = new char[len + 1];
		memcpy(ip, pData + pos, len);
		ip[len] = 0;
		pos += len;
		//pConnectionData->columnText[COLUMN_IP] = ConvFromNetwork(ip);
		delete [] ip;

		if ((pos+4) > dwDataLength)
		{
			delete pConnectionData;
			return FALSE;
		}
		memcpy(&pConnectionData->port, pData + pos, 4);

		pConnectionData->columnText[COLUMN_ID].Format(_T("%06d"), userid);
		m_connectionDataMap[userid] = pConnectionData;
		pConnectionData->listIndex = m_connectionDataArray.size();
		m_connectionDataArray.push_back(pConnectionData);

		pConnectionData->columnText[COLUMN_USER] = _T("(not logged in)");
		SetItemCount(GetItemCount() + 1);
		SetSortColumn(m_sortColumn, m_sortDir);

		if (GetItemCount() == 1)
			m_pOwner->SetIcon();
	}
	else if (op == USERCONTROL_CONNOP_CHANGEUSER)
	{
		int userid;
		memcpy(&userid, pData + 2, 4);

		if (dwDataLength < 8)
			return FALSE;

		std::map<int, CConnectionData*>::iterator iter = m_connectionDataMap.find(userid);
		if (iter == m_connectionDataMap.end())
			return FALSE;

		CConnectionData* pConnectionData = iter->second;

		unsigned int pos = 6;

		unsigned int len = pData[pos] * 256 + pData[pos+1];
		pos += 2;
		if ((pos + len) > dwDataLength)
			return FALSE;

		char* user = new char[len + 1];
		memcpy(user, pData + pos, len);
		user[len] = 0;
		pos += len;
		//pConnectionData->columnText[COLUMN_USER] = ConvFromNetwork(user);
		delete [] user;

		if (pConnectionData->columnText[COLUMN_USER] == _T(""))
		{
			pConnectionData->itemImages[COLUMN_ID] = 5;
			pConnectionData->columnText[COLUMN_USER] = _T("(not logged in)");
		}
		else
		{
			pConnectionData->itemImages[COLUMN_ID] = 4;
		}
		RedrawItems(pConnectionData->listIndex, pConnectionData->listIndex);
		SetSortColumn(m_sortColumn, m_sortDir);
	}
	else if (op == USERCONTROL_CONNOP_REMOVE)
	{
		int userid;
		memcpy(&userid, pData + 2, 4);

		std::map<int, CConnectionData*>::iterator iter = m_connectionDataMap.find(userid);
		if (iter == m_connectionDataMap.end())
			return FALSE;

		CConnectionData *pConnectionData = iter->second;

		m_connectionDataMap.erase(iter);
		for (std::vector<CConnectionData*>::iterator iter2 = m_connectionDataArray.begin() + pConnectionData->listIndex + 1; iter2 != m_connectionDataArray.end(); ++iter2)
			(*iter2)->listIndex--;
		m_connectionDataArray.erase(m_connectionDataArray.begin() + pConnectionData->listIndex);
		delete pConnectionData;

		SetItemCount(m_connectionDataArray.size());

		if (!GetItemCount())
			m_pOwner->SetIcon();
	}
	else if (op == USERCONTROL_CONNOP_TRANSFERINFO)
	{
		int userid;
		memcpy(&userid, pData + 2, 4);

		if (dwDataLength < 7)
			return FALSE;
		std::map<int, CConnectionData*>::iterator iter = m_connectionDataMap.find(userid);
		if (iter == m_connectionDataMap.end())
			return FALSE;

		CConnectionData* pConnectionData = iter->second;

		pConnectionData->transferMode = pData[6];

		if (!pConnectionData->transferMode)
		{
			pConnectionData->physicalFile = _T("");
			pConnectionData->logicalFile = _T("");
			pConnectionData->currentOffset = 0;
			pConnectionData->totalSize = -1;
			pConnectionData->ResetSpeed();

			pConnectionData->columnText[COLUMN_TRANSFERPROGRESS] =  _T("");
			pConnectionData->columnText[COLUMN_TRANSFERSPEED] =  _T("");
		}
		else
		{
			unsigned int pos = 7;
			if ((pos + 2) > dwDataLength)
				return FALSE;

			unsigned int len = pData[pos] * 256 + pData[pos+1];
			pos += 2;
			if ((pos + len + 2) > dwDataLength)
				return FALSE;

			char* physicalFile = new char[len + 1];
			memcpy(physicalFile, pData + pos, len);
			physicalFile[len] = 0;
			pos += len;
			//pConnectionData->physicalFile = ConvFromNetwork(physicalFile);
			delete [] physicalFile;

			len = pData[pos] * 256 + pData[pos+1];
			pos += 2;
			if ((pos + len) > dwDataLength)
				return FALSE;

			char* logicalFile = new char[len + 1];
			memcpy(logicalFile, pData + pos, len);
			logicalFile[len] = 0;
			pos += len;
			//pConnectionData->logicalFile = ConvFromNetwork(logicalFile);
			delete [] logicalFile;

			if (pConnectionData->transferMode & 0x20)
			{
				if ((pos + 8) > dwDataLength)
					return FALSE;

				memcpy(&pConnectionData->currentOffset, pData + pos, 8);
				pos += 8;
			}
			else
				pConnectionData->currentOffset = 0;

			if (pConnectionData->transferMode & 0x40)
			{
				if ((pos + 8) > dwDataLength)
					return FALSE;
				memcpy(&pConnectionData->totalSize, pData + pos, 8);
				pos += 8;
			}
			else
				pConnectionData->totalSize = -1;

			// Filter out indicator bits
			pConnectionData->transferMode &= 0x9F;
		}

		pConnectionData->columnText[COLUMN_TRANSFERINIT] =  m_showPhysical ? pConnectionData->physicalFile : pConnectionData->logicalFile;
		pConnectionData->itemImages[COLUMN_TRANSFERINIT] =  pConnectionData->transferMode;

		RedrawItems(pConnectionData->listIndex, pConnectionData->listIndex);
	}
	else if (op == USERCONTROL_CONNOP_TRANSFEROFFSETS)
	{
		std::map<int, CConnectionData*>::iterator iter = m_connectionDataMap.begin();
		unsigned char* p = pData + 2;
		int max = dwDataLength - 12;
		while ((p - pData) <= max)
		{
			int* userid = (int*)p;

			CConnectionData *pConnectionData;
			while (true)
			{
				if (iter == m_connectionDataMap.end())
					return FALSE;

				if (iter->first == *userid)
				{
					pConnectionData = iter->second;
					break;
				}

				++iter;
			}
			__int64* currentOffset = (__int64*)(p + 4);

			pConnectionData->AddBytes((int)(*currentOffset - pConnectionData->currentOffset));
			pConnectionData->currentOffset = *currentOffset;

			CString str;
			if (pConnectionData->totalSize != -1)
			{
				double percent = (double)pConnectionData->currentOffset / pConnectionData->totalSize * 100;
				str.Format(_T("%s bytes (%1.1f%%)"), makeUserFriendlyString(pConnectionData->currentOffset).GetString(), percent);
			}
			else
				str.Format(_T("%s bytes"), makeUserFriendlyString(pConnectionData->currentOffset).GetString());
			pConnectionData->columnText[COLUMN_TRANSFERPROGRESS] =  str;

			if (pConnectionData->speed > 1024 * 1024)
				str.Format(_T("%1.1f MB/s"), (double)pConnectionData->speed / 1024 / 1024);
			else if (pConnectionData->speed > 1024)
				str.Format(_T("%1.1f KB/s"), (double)pConnectionData->speed / 1024);
			else
				str.Format(_T("%1.1f bytes/s"), (double)pConnectionData->speed);
			pConnectionData->columnText[COLUMN_TRANSFERSPEED] =  str;

			p += 12;
		}
		RedrawItems(GetTopIndex(), GetTopIndex() + GetCountPerPage());
	}

	return TRUE;
}

void CUsersListCtrl::OnContextmenuKick()
{
	if (AfxMessageBox(_T("Do you really want to kick the selected user?"), MB_ICONQUESTION|MB_YESNO)!=IDYES)
		return;
	POSITION pos = GetFirstSelectedItemPosition();
	while (pos)
	{
		int nItem = GetNextSelectedItem(pos);

		CConnectionData *data = m_connectionDataArray[nItem];

		unsigned char buffer[5];
		buffer[0]=USERCONTROL_KICK;
		memcpy(buffer+1, &data->userid, 4);
		m_pOwner->SendCommand(3, &buffer, 5);
	}
}

void CUsersListCtrl::OnContextmenuBan()
{
	if (AfxMessageBox(_T("Do you really want to kick the selected user and ban his IP address?"), MB_ICONQUESTION|MB_YESNO)!=IDYES)
		return;
	POSITION pos = GetFirstSelectedItemPosition();
	while (pos)
	{
		int nItem = GetNextSelectedItem(pos);

		CConnectionData *data = m_connectionDataArray[nItem];

		unsigned char buffer[5];
		buffer[0] = USERCONTROL_BAN;
		memcpy(buffer+1, &data->userid, 4);
		m_pOwner->SendCommand(3, &buffer, 5);
	}
}

void CUsersListCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CMenu menu;
	menu.LoadMenu(IDR_USERVIEWCONTEXT);

	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	CWnd* pWndPopupOwner = this;
	//while (pWndPopupOwner->GetStyle() & WS_CHILD)
	//	pWndPopupOwner = pWndPopupOwner->GetParent();

	POSITION pos = GetFirstSelectedItemPosition();
	if (!pos)
	{
		pPopup->EnableMenuItem(ID_USERVIEWCONTEXT_KICK, MF_GRAYED);
		pPopup->EnableMenuItem(ID_USERVIEWCONTEXT_BAN, MF_GRAYED);
	}

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
		pWndPopupOwner);
}

BOOL CUsersListCtrl::ParseUserControlCommand(unsigned char *pData, DWORD dwDataLength)
{
	int type = *pData;
	if (type < 0 || type > 4)
	{
		m_pOwner->ShowStatus(_T("Protocol error: Invalid data"), 1);
		return FALSE;
	}
	switch (type)
	{
	case USERCONTROL_GETLIST:
		if (dwDataLength < 4) {
			return FALSE;
		}
		else {
			for (std::vector<CConnectionData*>::iterator iter = m_connectionDataArray.begin(); iter != m_connectionDataArray.end(); ++iter) {
				delete *iter;
			}

			m_connectionDataMap.clear();
			m_connectionDataArray.clear();

			int num = pData[1] * 256 * 256 + pData[2] * 256 + pData[3];
			unsigned int pos = 4;
			for (int i = 0; i < num; ++i) {
				if ((pos + 6) > dwDataLength) {
					return FALSE;
				}
				CConnectionData* pConnectionData = new CConnectionData;;
				memcpy(&pConnectionData->userid, pData+pos, 4);
				pos += 4;
				int len = pData[pos] * 256 + pData[pos+1];
				pos += 2;
				if (pos+len > dwDataLength) {
					delete pConnectionData;
					return FALSE;
				}

				char* ip = new char[len + 1];
				memcpy(ip, pData + pos, len);
				ip[len] = 0;
				pos += len;
				//pConnectionData->columnText[COLUMN_IP] = ConvFromNetwork(ip);
				delete [] ip;

				if ((pos+6) > dwDataLength) {
					delete pConnectionData;
					return FALSE;
				}
				memcpy(&pConnectionData->port, pData+pos, 4);

				pos += 4;

				len = pData[pos] * 256 + pData[pos+1];
				pos += 2;
				if ((pos + len + 1) > dwDataLength) {
					delete pConnectionData;
					return FALSE;
				}

				char* user = new char[len + 1];
				memcpy(user, pData + pos, len);
				user[len] = 0;
				pos += len;
				//pConnectionData->columnText[COLUMN_USER] = ConvFromNetwork(user);
				delete [] user;

				pConnectionData->transferMode = pData[pos++];

				if (pConnectionData->transferMode) {
					if ((pos + 2) > dwDataLength) {
						delete pConnectionData;
						return FALSE;
					}
					len = pData[pos] * 256 + pData[pos+1];
					pos += 2;

					if ((pos+len) > dwDataLength) {
						delete pConnectionData;
						return FALSE;
					}

					char* physicalFile = new char[len + 1];
					memcpy(physicalFile, pData + pos, len);
					physicalFile[len] = 0;
					pos += len;
					/*pConnectionData->physicalFile = ConvFromNetwork(physicalFile)*/;
					delete [] physicalFile;

					if ((pos + 2) > dwDataLength) {
						delete pConnectionData;
						return FALSE;
					}
					len = pData[pos] * 256 + pData[pos+1];
					pos += 2;


					if ((pos+len) > dwDataLength) {
						delete pConnectionData;
						return FALSE;
					}

					char* logicalFile = new char[len + 1];
					memcpy(logicalFile, pData + pos, len);
					logicalFile[len] = 0;
					pos += len;
					//pConnectionData->logicalFile = ConvFromNetwork(logicalFile);
					delete [] logicalFile;

					if (pConnectionData->transferMode & 0x20) {
						memcpy(&pConnectionData->currentOffset, pData + pos, 8);
						pos += 8;
					}
					else {
						pConnectionData->currentOffset = 0;
					}

					if (pConnectionData->transferMode & 0x40) {
						memcpy(&pConnectionData->totalSize, pData + pos, 8);
						pos += 8;
					}
					else {
						pConnectionData->totalSize = -1;
					}

					// Filter out indicator bits
					pConnectionData->transferMode &= 0x9F;
				}
				else {
					pConnectionData->currentOffset = 0;
					pConnectionData->totalSize = -1;
				}

				pConnectionData->columnText[COLUMN_ID].Format(_T("%06d"), pConnectionData->userid);
				m_connectionDataMap[pConnectionData->userid] = pConnectionData;
				pConnectionData->listIndex = m_connectionDataArray.size();
				m_connectionDataArray.push_back(pConnectionData);

				if (pConnectionData->columnText[COLUMN_USER] == _T("")) {
					pConnectionData->columnText[COLUMN_USER] = _T("(not logged in)");
				}
				else {
					pConnectionData->itemImages[COLUMN_ID] = 4;
				}

				pConnectionData->itemImages[COLUMN_TRANSFERINIT] = pConnectionData->transferMode;
				pConnectionData->columnText[COLUMN_TRANSFERINIT] = m_showPhysical ? pConnectionData->physicalFile : pConnectionData->logicalFile;
			}
			SetSortColumn(m_sortColumn, m_sortDir);
			SetItemCount(m_connectionDataArray.size());
			m_pOwner->SetIcon();
		}
		break;
	case USERCONTROL_CONNOP:
		return ProcessConnOp(pData, dwDataLength);
		break;
	case USERCONTROL_KICK:
	case USERCONTROL_BAN:
		break;
	default:
		m_pOwner->ShowStatus(_T("Protocol error: Specified usercontrol option not implemented"), 1);
		return FALSE;
		break;
	}
	return TRUE;
}

void CUsersListCtrl::OnSize(UINT nType, int cx, int cy)
{
	CListCtrl::OnSize(nType, cx, cy);
}

void CUsersListCtrl::SetDisplayPhysicalNames(bool showPhysical)
{
	m_showPhysical = showPhysical;

	// Iterate through all items and reset the transfer column text
	for (std::vector<CConnectionData*>::iterator iter = m_connectionDataArray.begin(); iter != m_connectionDataArray.end(); ++iter)
	{
		CConnectionData* pData = *iter;
		pData->columnText[COLUMN_TRANSFERINIT] = showPhysical ? pData->physicalFile : pData->logicalFile;
	}
	RedrawItems(0, GetItemCount() - 1);
}

void CUsersListCtrl::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent != m_nSpeedinfoTimer)
		return;

	for (std::vector<CConnectionData*>::iterator iter = m_connectionDataArray.begin(); iter != m_connectionDataArray.end(); ++iter)
	{
		CConnectionData* pConnectionData = *iter;
		if (pConnectionData->transferMode)
			pConnectionData->NextSpeed();
	}
	RedrawItems(0, GetItemCount() - 1);
}

void CUsersListCtrl::SetSortColumn(int sortColumn /*=-1*/, int dir /*=-1*/)
{
	if (m_sortColumn == sortColumn || sortColumn == -1)
	{
		if (dir == -1)
			m_sortDir = m_sortDir ? 0 : 1;
		else
			m_sortDir = dir ? 1 : 0;

		CHeaderCtrl *header = GetHeaderCtrl();
		if (header)
		{
			HDITEM hdi;
			hdi.mask = HDI_IMAGE | HDI_FORMAT;
			hdi.fmt = HDF_IMAGE | HDF_STRING | HDF_BITMAP_ON_RIGHT;
			hdi.iImage = m_sortDir + 1;
			header->SetItem(m_sortColumn, &hdi);
		}
	}
	else
	{
		if (dir == -1)
			m_sortDir = 0;
		else
			m_sortDir = dir ? 1 : 0;

		if (sortColumn < 0 || sortColumn > 2)
			sortColumn = 0;

		CHeaderCtrl *header = GetHeaderCtrl();
		if (header)
		{
			HDITEM hdi;
			hdi.mask = HDI_IMAGE | HDI_FORMAT;
			hdi.fmt = HDF_STRING;
			hdi.iImage = 0;
			header->SetItem(m_sortColumn, &hdi);

			hdi.fmt = HDF_IMAGE | HDF_STRING | HDF_BITMAP_ON_RIGHT;
			hdi.iImage = m_sortDir + 1;
			header->SetItem(sortColumn, &hdi);
		}

		m_sortColumn = sortColumn;
	}

	if (m_connectionDataArray.size() < 2)
		return;

	if (sortColumn == 1)
		QSortList(m_sortDir, 0, m_connectionDataArray.size() - 1, CmpUser);
	else if (sortColumn == 2)
		QSortList(m_sortDir, 0, m_connectionDataArray.size() - 1, CmpIP);
	else
		QSortList(m_sortDir, 0, m_connectionDataArray.size() - 1, CmpUserid);

	for (unsigned int i = 0; i < m_connectionDataArray.size(); i++)
		m_connectionDataArray[i]->listIndex = i;

	RedrawItems(0, m_connectionDataArray.size() - 1);
}

void CUsersListCtrl::QSortList(const unsigned int dir, int anf, int ende, int (*comp)(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData))
{
	int l = anf;
	int r = ende;
	const unsigned int ref = (l + r) / 2;
	const CConnectionData* refData = m_connectionDataArray[ref];
	do
	{
		if (!dir)
		{
			while ((comp(this, l, refData) < 0) && (l<ende)) l++;
			while ((comp(this, r, refData) > 0) && (r>anf)) r--;
		}
		else
		{
			while ((comp(this, l, refData) > 0) && (l<ende)) l++;
			while ((comp(this, r, refData) < 0) && (r>anf)) r--;
		}
		if (l<=r)
		{
			CConnectionData* tmp = m_connectionDataArray[l];
			m_connectionDataArray[l] = m_connectionDataArray[r];
			m_connectionDataArray[r] = tmp;
			l++;
			r--;
		}
	}
	while (l<=r);

	if (anf<r) QSortList(dir, anf, r, comp);
	if (l<ende) QSortList(dir, l, ende, comp);
}

int CUsersListCtrl::CmpUserid(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData)
{
	const CConnectionData* data = pList->m_connectionDataArray[index];

	if (data->userid > refData->userid)
		return 1;
	else if (data->userid < refData->userid)
		return -1;

	return 0;
}

int CUsersListCtrl::CmpUser(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData)
{
	const CConnectionData* data = pList->m_connectionDataArray[index];

	int res = data->columnText[COLUMN_USER].CompareNoCase(refData->columnText[COLUMN_USER]);
	if (res)
		return res;

	if (data->userid > refData->userid)
		return 1;
	else if (data->userid < refData->userid)
		return -1;

	return 0;
}

int CUsersListCtrl::CmpIP(const CUsersListCtrl *pList, unsigned int index, const CConnectionData* refData)
{
	const CConnectionData* data = pList->m_connectionDataArray[index];

	int res = data->columnText[COLUMN_IP].CompareNoCase(refData->columnText[COLUMN_IP]);
	if (res)
		return res;

	if (data->userid > refData->userid)
		return 1;
	else if (data->userid < refData->userid)
		return -1;

	return 0;
}

void CUsersListCtrl::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	LV_ITEM* pItem= &(pDispInfo)->item;

	if (static_cast<int>(m_connectionDataArray.size()) <= pItem->iItem)
		return;

	if (pItem->mask & LVIF_TEXT)
	{
		if (_tcslen(m_connectionDataArray[pItem->iItem]->columnText[pItem->iSubItem]) >= static_cast<size_t>(pItem->cchTextMax))
		{
			_tcsncpy(pItem->pszText, m_connectionDataArray[pItem->iItem]->columnText[pItem->iSubItem], pItem->cchTextMax - 4);
			pItem->pszText[pItem->cchTextMax - 4] = '.';
			pItem->pszText[pItem->cchTextMax - 3] = '.';
			pItem->pszText[pItem->cchTextMax - 2] = '.';
			pItem->pszText[pItem->cchTextMax - 1] = 0;
		}
		else
			lstrcpy(pItem->pszText, m_connectionDataArray[pItem->iItem]->columnText[pItem->iSubItem]);
	}
	if (pItem->mask & LVIF_IMAGE)
		pItem->iImage = m_connectionDataArray[pItem->iItem]->itemImages[pItem->iSubItem];
}

void CUsersListCtrl::OnColumnclick(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	SetSortColumn(pNMListView->iSubItem);

	*pResult = 0;
}
