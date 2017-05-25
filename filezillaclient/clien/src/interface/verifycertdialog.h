#ifndef __VERIFYCERTDIALOG_H__
#define __VERIFYCERTDIALOG_H__

#include "xmlfunctions.h"

class wxDialogEx;
class CVerifyCertDialog : protected wxEvtHandler
{
public:
	CVerifyCertDialog();
	virtual ~CVerifyCertDialog();

	bool IsTrusted(CCertificateNotification const& notification);
	void ShowVerificationDialog(CCertificateNotification& notification, bool displayOnly = false);

protected:
	struct t_certData {
		wxString host;
		int port{};
		unsigned char* data{};
		unsigned int len{};
	};

	bool IsTrusted(const wxString& host, int port, const unsigned char* data, unsigned int len, bool permanentOnly);
	bool DoIsTrusted(const wxString& host, int port, const unsigned char* data, unsigned int len, std::list<t_certData> const& trustedCerts);

	bool DisplayCert(wxDialogEx* pDlg, const CCertificate& cert);

	void ParseDN(wxWindow* parent, const wxString& dn, wxSizer* pSizer);
	void ParseDN_by_prefix(wxWindow* parent, std::list<wxString>& tokens, wxString prefix, const wxString& name, wxSizer* pSizer, bool decode = false);

	wxString DecodeValue(const wxString& value);

	wxString ConvertHexToString(const unsigned char* data, unsigned int len);
	unsigned char* ConvertStringToHex(const wxString& str, unsigned int &len);

	void SetPermanentlyTrusted(CCertificateNotification const& notification);

	void LoadTrustedCerts();

	std::list<t_certData> m_trustedCerts;
	std::list<t_certData> m_sessionTrustedCerts;

	CXmlFile m_xmlFile;

	std::vector<CCertificate> m_certificates;
	wxDialogEx* m_pDlg{};
	wxSizer* m_pSubjectSizer{};
	wxSizer* m_pIssuerSizer{};
	int line_height_{};

	void OnCertificateChoice(wxCommandEvent& event);
};

#endif //__VERIFYCERTDIALOG_H__
