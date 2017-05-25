#include <filezilla.h>
#include "filezillaapp.h"
#include "verifycertdialog.h"
#include "dialogex.h"
#include "ipcmutex.h"
#include "Options.h"
#include "xrc_helper.h"

#include <wx/scrolwin.h>
#include <wx/statbox.h>
#include <wx/tokenzr.h>

CVerifyCertDialog::CVerifyCertDialog()
	: m_xmlFile(wxGetApp().GetSettingsFile(_T("trustedcerts")))
{
}

CVerifyCertDialog::~CVerifyCertDialog()
{
	for (auto iter = m_trustedCerts.begin(); iter != m_trustedCerts.end(); ++iter)
		delete [] iter->data;
	for (auto iter = m_sessionTrustedCerts.begin(); iter != m_sessionTrustedCerts.end(); ++iter)
		delete [] iter->data;
}

bool CVerifyCertDialog::DisplayCert(wxDialogEx* pDlg, const CCertificate& cert)
{
	bool warning = false;
	if (cert.GetActivationTime().IsValid()) {
		if (cert.GetActivationTime() > wxDateTime::Now()) {
			pDlg->SetChildLabel(XRCID("ID_ACTIVATION_TIME"), wxString::Format(_("%s - Not yet valid!"), cert.GetActivationTime().FormatDate()));
			warning = true;
		}
		else
			pDlg->SetChildLabel(XRCID("ID_ACTIVATION_TIME"), cert.GetActivationTime().FormatDate());
	}
	else {
		warning = true;
		pDlg->SetChildLabel(XRCID("ID_ACTIVATION_TIME"), _("Invalid date"));
	}

	if (cert.GetExpirationTime().IsValid()) {
		if (cert.GetExpirationTime() < wxDateTime::Now()) {
			pDlg->SetChildLabel(XRCID("ID_EXPIRATION_TIME"), wxString::Format(_("%s - Certificate expired!"), cert.GetExpirationTime().FormatDate()));
			warning = true;
		}
		else
			pDlg->SetChildLabel(XRCID("ID_EXPIRATION_TIME"), cert.GetExpirationTime().FormatDate());
	}
	else {
		warning = true;
		pDlg->SetChildLabel(XRCID("ID_EXPIRATION_TIME"), _("Invalid date"));
	}

	if (!cert.GetSerial().empty())
		pDlg->SetChildLabel(XRCID("ID_SERIAL"), cert.GetSerial());
	else
		pDlg->SetChildLabel(XRCID("ID_SERIAL"), _("None"));

	pDlg->SetChildLabel(XRCID("ID_PKALGO"), wxString::Format(_("%s with %d bits"), cert.GetPkAlgoName(), cert.GetPkAlgoBits()));
	pDlg->SetChildLabel(XRCID("ID_SIGNALGO"), cert.GetSignatureAlgorithm());

	wxString const& sha256 = cert.GetFingerPrintSHA256();
	pDlg->SetChildLabel(XRCID("ID_FINGERPRINT_SHA256"), sha256.Left(sha256.size() / 2 + 1) + _T("\n") + sha256.Mid(sha256.size() / 2 + 1));
	pDlg->SetChildLabel(XRCID("ID_FINGERPRINT_SHA1"), cert.GetFingerPrintSHA1());

	ParseDN(XRCCTRL(*pDlg, "ID_ISSUER_BOX", wxStaticBox), cert.GetIssuer(), m_pIssuerSizer);

	auto subjectPanel = XRCCTRL(*pDlg, "ID_SUBJECT_PANEL", wxScrolledWindow);
	subjectPanel->Freeze();

	ParseDN(subjectPanel, cert.GetSubject(), m_pSubjectSizer);

	auto const& altNames = cert.GetAltSubjectNames();
	if (!altNames.empty()) {
		wxString str;
		for (auto const& altName : altNames) {
			str += altName + _T("\n");
		}
		str.RemoveLast();
		m_pSubjectSizer->Add(new wxStaticText(subjectPanel, wxID_ANY, wxPLURAL("Alternative name:", "Alternative names:", altNames.size())));
		m_pSubjectSizer->Add(new wxStaticText(subjectPanel, wxID_ANY, str));
	}
	m_pSubjectSizer->Fit(subjectPanel);

	wxSize min = m_pSubjectSizer->CalcMin();
	int const maxHeight = (line_height_ + m_pDlg->ConvertDialogToPixels(wxPoint(0, 1)).y) * 15;
	if (min.y >= maxHeight) {
		min.y = maxHeight;
		min.x += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
	}
	subjectPanel->SetMinSize(min);
	subjectPanel->Thaw();

	return warning;
}

#include <wx/scrolwin.h>

void CVerifyCertDialog::ShowVerificationDialog(CCertificateNotification& notification, bool displayOnly /*=false*/)
{
	LoadTrustedCerts();

	m_pDlg = new wxDialogEx;
	if (!m_pDlg->Load(0, _T("ID_VERIFYCERT"))) {
		wxBell();
		delete m_pDlg;
		m_pDlg = 0;
		return;
	}

	if (displayOnly) {
		xrc_call(*m_pDlg, "ID_DESC", &wxWindow::Hide);
		xrc_call(*m_pDlg, "ID_ALWAYS_DESC", &wxWindow::Hide);
		xrc_call(*m_pDlg, "ID_ALWAYS", &wxWindow::Hide);
		xrc_call(*m_pDlg, "wxID_CANCEL", &wxWindow::Hide);
		m_pDlg->SetTitle(_T("Certificate details"));
	}
	else {
		m_pDlg->WrapText(m_pDlg, XRCID("ID_DESC"), 400);

		if (COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) == 2)
			XRCCTRL(*m_pDlg, "ID_ALWAYS", wxCheckBox)->Hide();
	}

	m_certificates = notification.GetCertificates();
	if (m_certificates.size() == 1) {
		XRCCTRL(*m_pDlg, "ID_CHAIN_DESC", wxStaticText)->Hide();
		XRCCTRL(*m_pDlg, "ID_CHAIN", wxChoice)->Hide();
	}
	else {
		wxChoice* pChoice = XRCCTRL(*m_pDlg, "ID_CHAIN", wxChoice);
		for (unsigned int i = 0; i < m_certificates.size(); ++i) {
			pChoice->Append(wxString::Format(_T("%d"), i));
		}
		pChoice->SetSelection(0);

		pChoice->Connect(wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler(CVerifyCertDialog::OnCertificateChoice), 0, this);
	}

	m_pDlg->SetChildLabel(XRCID("ID_HOST"), wxString::Format(_T("%s:%d"), notification.GetHost(), notification.GetPort()));

	line_height_ = XRCCTRL(*m_pDlg, "ID_SUBJECT_DUMMY", wxStaticText)->GetSize().y;

	m_pSubjectSizer = XRCCTRL(*m_pDlg, "ID_SUBJECT_DUMMY", wxStaticText)->GetContainingSizer();
	m_pSubjectSizer->Clear(true);

	m_pIssuerSizer = XRCCTRL(*m_pDlg, "ID_ISSUER_DUMMY", wxStaticText)->GetContainingSizer();
	m_pIssuerSizer->Clear(true);

	wxSize minSize(0, 0);
	for (unsigned int i = 0; i < m_certificates.size(); ++i) {
		DisplayCert(m_pDlg, m_certificates[i]);
		m_pDlg->Layout();
		m_pDlg->GetSizer()->Fit(m_pDlg);
		minSize.IncTo(m_pDlg->GetSizer()->GetMinSize());
	}
	m_pDlg->GetSizer()->SetMinSize(minSize);

	bool warning = DisplayCert(m_pDlg, m_certificates[0]);

	m_pDlg->SetChildLabel(XRCID("ID_PROTOCOL"), notification.GetProtocol());
	m_pDlg->SetChildLabel(XRCID("ID_KEYEXCHANGE"), notification.GetKeyExchange());
	m_pDlg->SetChildLabel(XRCID("ID_CIPHER"), notification.GetSessionCipher());
	m_pDlg->SetChildLabel(XRCID("ID_MAC"), notification.GetSessionMac());

	if (warning) {
		XRCCTRL(*m_pDlg, "ID_IMAGE", wxStaticBitmap)->SetBitmap(wxArtProvider::GetBitmap(wxART_WARNING));
		if (!displayOnly)
			XRCCTRL(*m_pDlg, "ID_ALWAYS", wxCheckBox)->Enable(false);
	}

	m_pDlg->GetSizer()->Fit(m_pDlg);
	m_pDlg->GetSizer()->SetSizeHints(m_pDlg);

	int res = m_pDlg->ShowModal();

	if (!displayOnly) {
		if (res == wxID_OK) {
			wxASSERT(!IsTrusted(notification));

			notification.m_trusted = true;

			if (!warning && XRCCTRL(*m_pDlg, "ID_ALWAYS", wxCheckBox)->GetValue())
				SetPermanentlyTrusted(notification);
			else {
				t_certData cert;
				cert.host = notification.GetHost();
				cert.port = notification.GetPort();
				const unsigned char* data = m_certificates[0].GetRawData(cert.len);
				cert.data = new unsigned char[cert.len];
				memcpy(cert.data, data, cert.len);
				m_sessionTrustedCerts.push_back(cert);
			}
		}
		else
			notification.m_trusted = false;
	}

	delete m_pDlg;
	m_pDlg = 0;
}

void CVerifyCertDialog::ParseDN(wxWindow* parent, const wxString& dn, wxSizer* pSizer)
{
	pSizer->Clear(true);

	wxStringTokenizer tokens(dn, _T(","));

	std::list<wxString> tokenlist;
	while (tokens.HasMoreTokens())
		tokenlist.push_back(tokens.GetNextToken());

	ParseDN_by_prefix(parent, tokenlist, _T("CN"), _("Common name:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("O"), _("Organization:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("2.5.4.15"), _("Business category:"), pSizer, true);
	ParseDN_by_prefix(parent, tokenlist, _T("OU"), _("Unit:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("T"), _("Title:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("C"), _("Country:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("ST"), _("State or province:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("L"), _("Locality:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("2.5.4.17"), _("Postal code:"), pSizer, true);
	ParseDN_by_prefix(parent, tokenlist, _T("postalCode"), _("Postal code:"), pSizer, true);
	ParseDN_by_prefix(parent, tokenlist, _T("STREET"), _("Street:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("EMAIL"), _("E-Mail:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("serialNumber"), _("Serial number:"), pSizer);
	ParseDN_by_prefix(parent, tokenlist, _T("1.3.6.1.4.1.311.60.2.1.3"), _("Jurisdiction country:"), pSizer, true);
	ParseDN_by_prefix(parent, tokenlist, _T("1.3.6.1.4.1.311.60.2.1.2"), _("Jurisdiction state or province:"), pSizer, true);
	ParseDN_by_prefix(parent, tokenlist, _T("1.3.6.1.4.1.311.60.2.1.1"), _("Jurisdiction locality:"), pSizer, true);

	if (!tokenlist.empty())
	{
		wxString value = tokenlist.front();
		for (std::list<wxString>::const_iterator iter = ++tokenlist.begin(); iter != tokenlist.end(); ++iter)
			value += _T(",") + *iter;

		pSizer->Add(new wxStaticText(parent, wxID_ANY, _("Other:")));
		pSizer->Add(new wxStaticText(parent, wxID_ANY, value));
	}
}

void CVerifyCertDialog::ParseDN_by_prefix(wxWindow* parent, std::list<wxString>& tokens, wxString prefix, const wxString& name, wxSizer* pSizer, bool decode /*=false*/)
{
	prefix += _T("=");
	int len = prefix.Length();

	wxString value;

	bool append = false;

	auto iter = tokens.begin();
	while (iter != tokens.end())
	{
		if (!append)
		{
			if (iter->Left(len) != prefix)
			{
				++iter;
				continue;
			}

			if (!value.empty())
				value += _T("\n");
		}
		else
		{
			append = false;
			value += _T(",");
		}

		value += iter->Mid(len);

		if (iter->Last() == '\\')
		{
			value.RemoveLast();
			append = true;
			len = 0;
		}

		auto remove = iter++;
		tokens.erase(remove);
	}

	if (decode)
		value = DecodeValue(value);

	if (!value.empty())
	{
		pSizer->Add(new wxStaticText(parent, wxID_ANY, name));
		pSizer->Add(new wxStaticText(parent, wxID_ANY, value));
	}
}

bool CVerifyCertDialog::IsTrusted(CCertificateNotification const& notification)
{
	LoadTrustedCerts();

	unsigned int len;
	CCertificate cert =  notification.GetCertificates()[0];
	const unsigned char* data = cert.GetRawData(len);

	return IsTrusted(notification.GetHost(), notification.GetPort(), data, len, false);
}

bool CVerifyCertDialog::DoIsTrusted(const wxString& host, int port, const unsigned char* data, unsigned int len, std::list<CVerifyCertDialog::t_certData> const& trustedCerts)
{
	if( !data || !len ) {
		return false;
	}

	for ( auto const& cert : trustedCerts ) {
		if (host != cert.host)
			continue;

		if (port != cert.port)
			continue;

		if (cert.len != len)
			continue;

		if (!memcmp(cert.data, data, len))
			return true;
	}

	return false;
}

bool CVerifyCertDialog::IsTrusted(const wxString& host, int port, const unsigned char* data, unsigned int len, bool permanentOnly)
{
	bool trusted = DoIsTrusted(host, port, data, len, m_trustedCerts);
	if( !trusted && !permanentOnly ) {
		trusted = DoIsTrusted(host, port, data, len, m_sessionTrustedCerts);
	}

	return trusted;
}

wxString CVerifyCertDialog::ConvertHexToString(const unsigned char* data, unsigned int len)
{
	wxString str;
	for (unsigned int i = 0; i < len; i++)
	{
		const unsigned char& c = data[i];

		const unsigned char low = c & 0x0F;
		const unsigned char high = (c & 0xF0) >> 4;

		if (high < 10)
			str += '0' + high;
		else
			str += 'A' + high - 10;

		if (low < 10)
			str += '0' + low;
		else
			str += 'A' + low - 10;
	}

	return str;
}

unsigned char* CVerifyCertDialog::ConvertStringToHex(const wxString& str, unsigned int &len)
{
	if (str.size() % 2) {
		return 0;
	}

	len = str.size() / 2;
	unsigned char* data = new unsigned char[len];

	unsigned int j = 0;
	for (unsigned int i = 0; i < str.size(); ++i, ++j)
	{
		wxChar high = str[i++];
		wxChar low = str[i];

		if (high >= '0' && high <= '9')
			high -= '0';
		else if (high >= 'A' && high <= 'F')
			high -= 'A' - 10;
		else
		{
			delete [] data;
			return 0;
		}

		if (low >= '0' && low <= '9')
			low -= '0';
		else if (low >= 'A' && low <= 'F')
			low -= 'A' - 10;
		else
		{
			delete [] data;
			return 0;
		}

		data[j] = ((unsigned char)high << 4) + (unsigned char)low;
	}

	return data;
}

void CVerifyCertDialog::LoadTrustedCerts()
{
	CReentrantInterProcessMutexLocker mutex(MUTEX_TRUSTEDCERTS);
	if (!m_xmlFile.Modified()) {
		return;
	}

	TiXmlElement* pElement = m_xmlFile.Load();
	if (!pElement) {
		return;
	}

	m_trustedCerts.clear();

	if (!(pElement = pElement->FirstChildElement("TrustedCerts")))
		return;

	bool modified = false;

	TiXmlElement* pCert = pElement->FirstChildElement("Certificate");
	while (pCert) {
		wxString value = GetTextElement(pCert, "Data");

		TiXmlElement* pRemove = 0;

		t_certData data;
		if (value.empty() || !(data.data = ConvertStringToHex(value, data.len)))
			pRemove = pCert;

		data.host = GetTextElement(pCert, "Host");
		data.port = GetTextElementInt(pCert, "Port");
		if (data.host.empty() || data.port < 1 || data.port > 65535)
			pRemove = pCert;

		wxLongLong activationTime = GetTextElementLongLong(pCert, "ActivationTime", 0);
		if (activationTime == 0 || activationTime > wxDateTime::GetTimeNow())
			pRemove = pCert;

		wxLongLong expirationTime = GetTextElementLongLong(pCert, "ExpirationTime", 0);
		if (expirationTime == 0 || expirationTime < wxDateTime::GetTimeNow())
			pRemove = pCert;

		if (IsTrusted(data.host, data.port, data.data, data.len, true)) // Weed out duplicates
			pRemove = pCert;

		if (!pRemove)
			m_trustedCerts.push_back(data);
		else
			delete [] data.data;

		pCert = pCert->NextSiblingElement("Certificate");

		if (pRemove)
		{
			modified = true;
			pElement->RemoveChild(pRemove);
		}
	}

	if (modified)
		m_xmlFile.Save(false);
}

void CVerifyCertDialog::SetPermanentlyTrusted(CCertificateNotification const& notification)
{
	const CCertificate certificate = notification.GetCertificates()[0];
	unsigned int len;
	const unsigned char* const data = certificate.GetRawData(len);

	CReentrantInterProcessMutexLocker mutex(MUTEX_TRUSTEDCERTS);
	LoadTrustedCerts();

	if (IsTrusted(notification.GetHost(), notification.GetPort(), data, len, true))	{
		return;
	}

	t_certData cert;
	cert.host = notification.GetHost();
	cert.port = notification.GetPort();
	cert.len = len;
	cert.data = new unsigned char[len];
	memcpy(cert.data, data, len);
	m_trustedCerts.push_back(cert);

	if (COptions::Get()->GetOptionVal(OPTION_DEFAULT_KIOSKMODE) == 2) {
		return;
	}

	TiXmlElement* pElement = m_xmlFile.GetElement();
	if (!pElement) {
		return;
	}

	TiXmlElement* pCerts = pElement->FirstChildElement("TrustedCerts");
	if (!pCerts)
		pCerts = pElement->LinkEndChild(new TiXmlElement("TrustedCerts"))->ToElement();

	TiXmlElement* pCert = pCerts->LinkEndChild(new TiXmlElement("Certificate"))->ToElement();

	AddTextElement(pCert, "Data", ConvertHexToString(data, len));

	wxLongLong time = certificate.GetActivationTime().GetTicks();
	AddTextElement(pCert, "ActivationTime", time.ToString());

	time = certificate.GetExpirationTime().GetTicks();
	AddTextElement(pCert, "ExpirationTime", time.ToString());

	AddTextElement(pCert, "Host", notification.GetHost());
	AddTextElement(pCert, "Port", notification.GetPort());

	m_xmlFile.Save(true);
}

wxString CVerifyCertDialog::DecodeValue(const wxString& value)
{
	// Decodes string in hex notation
	// #xxxx466F6F626172 -> Foobar
	// First two encoded bytes are ignored, some weird type information I don't care about
	// Only accepts ASCII for now.
	if (value.empty() || value[0] != '#')
		return value;

	unsigned int len = value.Len();

	wxString out;

	for (unsigned int i = 5; i + 1 < len; i += 2)
	{
		wxChar c = value[i];
		wxChar d = value[i + 1];
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'a' && c <= 'z')
			c -= 'a' - 10;
		else if (c >= 'A' && c <= 'Z')
			c -= 'A' - 10;
		else
			continue;
			//return value;
		if (d >= '0' && d <= '9')
			d -= '0';
		else if (d >= 'a' && d <= 'z')
			d -= 'a' - 10;
		else if (d >= 'A' && d <= 'Z')
			d -= 'A' - 10;
		else
			continue;
			//return value;

		c = c * 16 + d;
		if (c > 127 || c < 32)
			continue;
		out += c;
	}

	return out;
}

void CVerifyCertDialog::OnCertificateChoice(wxCommandEvent& event)
{
	int sel = event.GetSelection();
	if (sel < 0 || sel > (int)m_certificates.size())
		return;
	DisplayCert(m_pDlg, m_certificates[sel]);

	m_pDlg->Layout();
	m_pDlg->GetSizer()->Fit(m_pDlg);
	m_pDlg->Refresh();
}
