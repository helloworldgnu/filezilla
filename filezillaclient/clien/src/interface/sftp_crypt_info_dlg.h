#ifndef __SFTP_CRYPT_INFO_DLG_H__
#define __SFTP_CRYPT_INFO_DLG_H__

class wxDialogEx;
class CSftpEncryptioInfoDialog
{
public:
	void ShowDialog(CSftpEncryptionNotification* pNotification);

protected:
	void SetLabel(wxDialogEx& dlg, int id, const wxString& text);
};

#endif //__SFTP_CRYPT_INFO_DLG_H__
