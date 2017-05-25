#include <filezilla.h>
#include "volume_enumerator.h"

#ifdef __WXMSW__

#include <wx/msw/registry.h>

DEFINE_EVENT_TYPE(fzEVT_VOLUMEENUMERATED)
DEFINE_EVENT_TYPE(fzEVT_VOLUMESENUMERATED)

CVolumeDescriptionEnumeratorThread::CVolumeDescriptionEnumeratorThread(wxEvtHandler* pEvtHandler)
	: wxThread(wxTHREAD_JOINABLE),
	  m_pEvtHandler(pEvtHandler)
{
	m_failure = false;
	m_stop = false;
	m_running = true;

	if (Create() != wxTHREAD_NO_ERROR)
	{
		m_running = false;
		m_failure = true;
	}
	Run();
}

CVolumeDescriptionEnumeratorThread::~CVolumeDescriptionEnumeratorThread()
{
	m_stop = true;
	Wait(wxTHREAD_WAIT_BLOCK);

	for (std::list<t_VolumeInfoInternal>::const_iterator iter = m_volumeInfo.begin(); iter != m_volumeInfo.end(); ++iter)
	{
		delete [] iter->pVolume;
		delete [] iter->pVolumeName;
	}
	m_volumeInfo.clear();
}

wxThread::ExitCode CVolumeDescriptionEnumeratorThread::Entry()
{
	if (!GetDriveLabels())
		m_failure = true;

	m_running = false;

	m_pEvtHandler->QueueEvent(new wxCommandEvent(fzEVT_VOLUMESENUMERATED));

	return 0;
}

void CVolumeDescriptionEnumeratorThread::ProcessDrive(wxString const& drive)
{
	if( GetDriveLabel(drive) ) {
		m_pEvtHandler->QueueEvent(new wxCommandEvent(fzEVT_VOLUMEENUMERATED));
	}
}

bool CVolumeDescriptionEnumeratorThread::GetDriveLabel(wxString const& drive)
{
	int len = drive.size();
	wxChar* pVolume = new wxChar[drive.size() + 1];
	wxStrcpy(pVolume, drive);
	if (pVolume[drive.size() - 1] == '\\') {
		pVolume[drive.size() - 1] = 0;
		--len;
	}
	if (!*pVolume) {
		delete [] pVolume;
		return false;
	}

	// Check if it is a network share
	wxChar *share_name = new wxChar[512];
	DWORD dwSize = 511;
	if (!WNetGetConnection(pVolume, share_name, &dwSize)) {
		scoped_lock l(sync_);
		t_VolumeInfoInternal volumeInfo;
		volumeInfo.pVolume = pVolume;
		volumeInfo.pVolumeName = share_name;
		m_volumeInfo.push_back(volumeInfo);
		return true;
	}
	else
		delete [] share_name;

	// Get the label of the drive
	wxChar* pVolumeName = new wxChar[501];
	int oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	BOOL res = GetVolumeInformation(drive, pVolumeName, 500, 0, 0, 0, 0, 0);
	SetErrorMode(oldErrorMode);
	if (res && pVolumeName[0]) {
		scoped_lock l(sync_);
		t_VolumeInfoInternal volumeInfo;
		volumeInfo.pVolume = pVolume;
		volumeInfo.pVolumeName = pVolumeName;
		m_volumeInfo.push_back(volumeInfo);
		return true;
	}

	delete [] pVolumeName;
	delete [] pVolume;

	return false;
}

bool CVolumeDescriptionEnumeratorThread::GetDriveLabels()
{
	std::list<wxString> drives = GetDrives();

	if( drives.empty() ) {
		return true;
	}

	std::list<wxString>::const_iterator drive_a = drives.end();
	for( std::list<wxString>::const_iterator it = drives.begin(); it != drives.end() && !m_stop; ++it ) {
		if (m_stop) {
			return false;
		}

		wxString const& drive = *it;
		if( (drive[0] == 'a' || drive[0] == 'A') && drive_a == drives.end() ) {
			// Defer processing of A:, most commonly the slowest of all drives.
			drive_a = it;
		}
		else {
			ProcessDrive(drive);
		}
	}

	if( drive_a != drives.end() && !m_stop ) {
		ProcessDrive(*drive_a);
	}

	return !m_stop;
}

long CVolumeDescriptionEnumeratorThread::GetDrivesToHide()
{
	long drivesToHide = 0;
	// Adhere to the NODRIVES group policy
	wxRegKey key(_T("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"));
	if (key.Exists()) {
		wxLogNull null; // QueryValue can fail if item has wrong type
		if (!key.HasValue(_T("NoDrives")) || !key.QueryValue(_T("NoDrives"), &drivesToHide))
			drivesToHide = 0;
	}
	return drivesToHide;
}

bool CVolumeDescriptionEnumeratorThread::IsHidden(wxChar const* drive, long noDrives)
{
	int bit = 0;
	if (drive && drive[0] != 0 && drive[1] == ':') {
		wxChar letter = drive[0];
		if (letter >= 'A' && letter <= 'Z')
			bit = 1 << (letter - 'A');
		else if (letter >= 'a' && letter <= 'z')
			bit = 1 << (letter - 'a');
	}

	return (noDrives & bit) != 0;
}

std::list<wxString> CVolumeDescriptionEnumeratorThread::GetDrives()
{
	std::list<wxString> ret;

	long drivesToHide = GetDrivesToHide();

	int len = GetLogicalDriveStrings(0, 0);
	if (len) {
		wxChar* drives = new wxChar[len + 1];
		if (GetLogicalDriveStrings(len, drives)) {
			const wxChar* pDrive = drives;
			while (*pDrive) {
				const int len = wxStrlen(pDrive);

				if( !IsHidden(pDrive, drivesToHide) ) {
					ret.push_back(pDrive);
				}

				pDrive += len + 1;
			}
		}

		delete [] drives;
	}

	return ret;
}


std::list<CVolumeDescriptionEnumeratorThread::t_VolumeInfo> CVolumeDescriptionEnumeratorThread::GetVolumes()
{
	std::list<t_VolumeInfo> volumeInfo;

	scoped_lock l(sync_);

	for (auto const& internal_info : m_volumeInfo) {
		t_VolumeInfo info;
		info.volume = internal_info.pVolume;
		delete[] internal_info.pVolume;
		info.volumeName = internal_info.pVolumeName;
		delete[] internal_info.pVolumeName;
		volumeInfo.push_back(info);
	}
	m_volumeInfo.clear();

	return volumeInfo;
}

#endif //__WXMSW__
