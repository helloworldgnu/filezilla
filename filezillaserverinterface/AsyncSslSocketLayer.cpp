/*           CAsyncSslSocketLayer by Tim Kosse 
          mailto: tim.kosse@filezilla-project.org)
                 Version 2.0 (2005-02-27)
-------------------------------------------------------------

Introduction
------------

CAsyncSslSocketLayer is a layer class for CAsyncSocketEx which allows you to establish SSL secured
connections. Support for both client and server side is provided.

How to use
----------

Using this class is really simple. In the easiest case, just add an instance of
CAsyncSslSocketLayer to your socket and call InitClientSsl after creation of the socket.

This class only has a couple of public functions:
- InitSSLConnection(bool clientMode);
  This functions establishes an SSL connection. The clientMode parameter specifies wether the SSL connection
  is in server or in client mode.
  Most likely you want to call this function right after calling Create for the socket.
  But sometimes, you'll need to call this function later. One example is for an FTP connection
  with explicit SSL: In this case you would have to call InitSSLConnection after receiving the reply
  to an 'AUTH SSL' command.
- Is UsingSSL();
  Returns true if you've previously called InitClientSsl()
- SetNotifyReply(SetNotifyReply(int nID, int nCode, int result);
  You can call this function only after receiving a layerspecific callback with the SSL_VERIFY_CERT
  id. Set result to 1 if you trust the certificate and 0 if you don't trust it.
  nID has to be the priv_data element of the t_SslCertData structure and nCode has to be SSL_VERIFY_CERT.
- CreateSslCertificate(LPCTSTR filename, int bits, unsigned char* country, unsigned char* state,
			unsigned char* locality, unsigned char* organization, unsigned char* unit, unsigned char* cname,
			unsigned char *email, CString& err);
  Creates a new self-signed SSL certificate and stores it in the given file
- SendRaw(const void* lpBuf, int nBufLen)
  Sends a raw, unencrypted message. This may be useful after successful initialization to tell the other
  side that can use SSL.

This layer sends some layerspecific notifications to your socket instance, you can handle them in
OnLayerCallback of your socket class.
Valid notification IDs are:
- SSL_INFO 0
  There are two possible values for param2:
	SSL_INFO_ESTABLISHED 0 - You'll get this notification if the SSL negotiation was successful
	SSL_INFO_SHUTDOWNCOMPLETE 1 - You'll get this notification if the SSL connection has been shut
                                  down successfully. See below for details.
- SSL_FAILURE 1
  This notification is sent if the SSL connection could not be established or if an existing
  connection failed. Valid values for param2 are:
  - SSL_FAILURE_UNKNOWN 0 - Details may have been sent with a SSL_VERBOSE_* notification.
  - SSL_FAILURE_ESTABLISH 1 - Problem during SSL negotiation
  - SSL_FAILURE_LOADDLLS 2
  - SSL_FAILURE_INITSSL 4
  - SSL_FAILURE_VERIFYCERT 8 - The remote SSL certificate was invalid
  - SSL_FAILURE_CERTREJECTED 16 - The remote SSL certificate was rejected by user
- SSL_VERBOSE_WARNING 3
  SSL_VERBOSE_INFO 4
  This two notifications contain some additional information. The value given by param2 is a
  pointer to a null-terminated char string (char *) with some useful information.
- SSL_VERIFY_CERT 2
  This notification is sent each time a remote certificate has to be verified.
  param2 is a pointer to a t_SslCertData structure which contains some information
  about the remote certificate.
  You have to set the reply to this message using the SetNotifyReply function.

Be careful with closing the connection after sending data, not all data may have been sent already.
Before closing the connection, you should call Shutdown() and wait for the SSL_INFO_SHUTDOWNCOMPLETE
notification. This assures that all encrypted data really has been sent.

License
-------

Feel free to use this class, as long as you don't claim that you wrote it
and this copyright notice stays intact in the source files.
If you want to use this class in a commercial application, a short message
to tim.kosse@filezilla-project.org would be appreciated but is not required.

This product includes software developed by the OpenSSL Project
for use in the OpenSSL Toolkit. (http://www.openssl.org/)

Version history
---------------

Version 2.0:
- Add server support
- a lot of bug fixes

*/

#include "stdafx.h"
#include "AsyncSslSocketLayer.h"

#include <algorithm>

typedef std::lock_guard<std::recursive_mutex> simple_lock;
typedef std::unique_lock<std::recursive_mutex> scoped_lock;


// Simple macro to declare function type and function pointer based on the
// three given parametrs:
// r - return type,
// n - function name
// a - argument list
//
// Example:
// def(int, foo, (int x)) becomes the following:
// typedef int (*tfoo)(int x);
// static tfoo pfoo;

#define def(r, n, a) \
	typedef r (*t##n) a; \
	static t##n p##n = 0;

// Macro to load the given macro from a dll:
#define proc(dll, n) \
	dll.load_func( #n, (void**)&p##n); \
	if (!p##n) \
		bError = true;

//The following functions from the SSL libraries are used:
def(int, SSL_state, (const SSL *s));
def(const char*, SSL_state_string_long, (const SSL *s));
def(void, SSL_set_info_callback, (SSL *ssl, void (*cb)(const SSL *ssl,int type,int val)));
def(void, SSL_set_bio, (SSL *s, BIO *rbio, BIO *wbio));
def(void, SSL_set_connect_state, (SSL *s));
def(int, SSL_set_session, (SSL *to, SSL_SESSION *session));
def(BIO_METHOD*, BIO_f_ssl, (void));
def(SSL*, SSL_new, (SSL_CTX *ctx));
def(SSL_CTX*, SSL_CTX_new, (SSL_METHOD *meth));
def(SSL_METHOD*, SSLv23_method, (void));
def(void, SSL_load_error_strings, (void));
def(int, SSL_library_init, (void));
def(void, SSL_CTX_free, (SSL_CTX *));
def(void, SSL_free, (SSL *ssl));
def(int, SSL_get_error, (const SSL *s, int retcode));
def(int, SSL_shutdown, (SSL *s));
def(int, SSL_get_shutdown, (const SSL *ssl));
def(const char*, SSL_alert_type_string_long, (int value));
def(const char*, SSL_alert_desc_string_long, (int value));
def(void, SSL_CTX_set_verify, (SSL_CTX *ctx, int mode, int (*callback)(int, X509_STORE_CTX *)));
def(X509_STORE*, SSL_CTX_get_cert_store, (const SSL_CTX *));
def(long, SSL_get_verify_result, (const SSL *ssl));
def(X509*, SSL_get_peer_certificate, (const SSL *s));
def(const char*, SSL_get_version, (const SSL *ssl));
def(SSL_CIPHER*, SSL_get_current_cipher, (const SSL *ssl));
def(const char*, SSL_CIPHER_get_name, (const SSL_CIPHER *cipher));
def(char*, SSL_CIPHER_get_version, (const SSL_CIPHER *cipher));
def(int, SSL_get_ex_data_X509_STORE_CTX_idx, (void));
def(int, SSL_CTX_load_verify_locations, (SSL_CTX *ctx, const char *CAfile, const char *CApath));
def(long, SSL_ctrl, (SSL *ssl, int cmd, long larg, void *parg));
def(void, SSL_set_accept_state, (SSL *ssl));
def(int, SSL_CTX_use_PrivateKey_file, (SSL_CTX *ctx, const char *file, int type));
def(int, SSL_CTX_use_certificate_file, (SSL_CTX *ctx, const char *file, int type));
def(int, SSL_CTX_check_private_key, (const SSL_CTX *ctx));
def(void, SSL_CTX_set_default_passwd_cb, (SSL_CTX *ctx, pem_password_cb *cb));
def(void, SSL_CTX_set_default_passwd_cb_userdata, (SSL_CTX *ctx, void *u));
def(int, SSL_CTX_use_certificate_chain_file, (SSL_CTX *ctx, const char *file));
def(long, SSL_CTX_ctrl, (SSL_CTX *ctx, int cmd, long larg, void *parg));
def(int, SSL_set_cipher_list, (SSL *ssl, const char *str));
def(const char*, SSL_get_cipher_list, (const SSL *ssl, int priority));
def(X509*, SSL_CTX_get0_certificate, (const SSL_CTX *ctx));
def(long, SSL_get_default_timeout, (const SSL *ssl));
def(long, SSL_CTX_set_timeout, (SSL_CTX *ctx, long t));
def(SSL_SESSION*, SSL_get_session, (const SSL *ssl));
def(SSL_SESSION*, SSL_get1_session, (const SSL *ssl));
def(long, SSL_SESSION_set_timeout, (SSL_SESSION *s, long t));
def(void, SSL_CTX_flush_sessions, (SSL_CTX *ctx, long tm));
def(int, SSL_CTX_remove_session, (SSL_CTX *, SSL_SESSION *c));
def(void, SSL_CTX_sess_set_new_cb, (SSL_CTX *ctx, int (*new_session_cb)(SSL *, SSL_SESSION *)));
def(void, SSL_CTX_sess_set_remove_cb, (SSL_CTX *ctx, void (*remove_session_cb)(SSL_CTX *ctx, SSL_SESSION *)));
def(void, SSL_CTX_sess_set_get_cb, (SSL_CTX *ctx, SSL_SESSION *(*get_session_cb)(SSL *, unsigned char *, int, int *)));
def(long, SSL_CTX_callback_ctrl, (SSL_CTX *, int, void (*)(void)));
def(int, SSL_set_ex_data, (SSL *ssl, int idx, void *arg));
def(void*, SSL_get_ex_data, (const SSL *ssl, int idx));

def(size_t, BIO_ctrl_pending, (BIO *b));
def(int, BIO_read, (BIO *b, void *data, int len));
def(long, BIO_ctrl, (BIO *bp, int cmd, long larg, void *parg));
def(int, BIO_write, (BIO *b, const void *data, int len));
def(size_t, BIO_ctrl_get_write_guarantee, (BIO *b));
def(int, BIO_new_bio_pair, (BIO **bio1, size_t writebuf1, BIO **bio2, size_t writebuf2));
def(BIO*, BIO_new, (BIO_METHOD *type));
def(int, BIO_free, (BIO *a));
def(int, i2t_ASN1_OBJECT, (char *buf, int buf_len, ASN1_OBJECT *a));
def(int, OBJ_obj2nid, (const ASN1_OBJECT *o));
def(ASN1_OBJECT*, X509_NAME_ENTRY_get_object, (X509_NAME_ENTRY *ne));
def(X509_NAME_ENTRY*, X509_NAME_get_entry, (X509_NAME *name, int loc));
def(int, X509_NAME_entry_count, (X509_NAME *name));
def(X509_NAME*, X509_get_subject_name, (X509 *a));
def(X509_NAME*, X509_get_issuer_name, (X509 *a));
def(const char*, OBJ_nid2sn, (int n));
def(ASN1_STRING*, X509_NAME_ENTRY_get_data, (X509_NAME_ENTRY *ne));
def(void, X509_STORE_CTX_set_error, (X509_STORE_CTX *ctx, int s));
def(int, X509_digest, (const X509 *data, const EVP_MD *type, unsigned char *md, unsigned int *len));
def(const EVP_MD*, EVP_sha1, (void));
def(const EVP_MD*, EVP_sha256, (void));
def(X509*, X509_STORE_CTX_get_current_cert, (X509_STORE_CTX *ctx));
def(int, X509_STORE_CTX_get_error, (X509_STORE_CTX *ctx));
def(void, X509_free, (X509 *a));
def(EVP_PKEY*, X509_get_pubkey, (X509 *x));
def(int, BN_num_bits, (const BIGNUM *a));
def(void, EVP_PKEY_free, (EVP_PKEY *pkey));
def(void*, X509_STORE_CTX_get_ex_data, (X509_STORE_CTX *ctx, int idx));
def(char*, X509_NAME_oneline, (X509_NAME *a, char *buf, int size));
def(const char*, X509_verify_cert_error_string, (long n));
def(int, X509_STORE_CTX_get_error_depth, (X509_STORE_CTX *ctx));
def(unsigned long, ERR_get_error, (void));
def(const char*, ERR_error_string, (unsigned long e, char *buf));
def(int, ASN1_STRING_to_UTF8, (unsigned char **out, ASN1_STRING *in));
def(void, CRYPTO_free, (void *p));
def(RSA*, RSA_generate_key, (int bits, unsigned long e, void (*callback)(int,int,void *), void *cb_arg));
def(int, X509_set_version, (X509 *x,long version));
def(ASN1_TIME*, X509_gmtime_adj, (ASN1_TIME *s, long adj));
def(int, X509_set_pubkey, (X509 *x, EVP_PKEY *pkey));
def(int, X509_NAME_add_entry_by_txt, (X509_NAME *name, const char *field, int type, const unsigned char *bytes, int len, int loc, int set));
def(int, X509_NAME_add_entry_by_NID, (X509_NAME *name, int nid, int type, unsigned char *bytes, int len, int loc, int set));
def(int, X509_set_issuer_name, (X509 *x, X509_NAME *name));
def(int, X509_sign, (X509 *x, EVP_PKEY *pkey, const EVP_MD *md));
def(EVP_PKEY*, EVP_PKEY_new, (void));
def(int, EVP_PKEY_assign, (EVP_PKEY *pkey, int type, char *key));
def(X509*, X509_new, (void));
def(int, ASN1_INTEGER_set, (ASN1_INTEGER *a, long v));
def(ASN1_INTEGER*, X509_get_serialNumber, (X509 *x));
def(int, PEM_ASN1_write_bio, (int (*i2d)(),const char *name,BIO *bp,char *x, const EVP_CIPHER *enc,unsigned char *kstr,int klen, pem_password_cb *callback, void *u));
def(int, i2d_X509, (X509 *x, unsigned char **out));
def(BIO_METHOD *, BIO_s_mem, (void));
def(int, i2d_PrivateKey, (EVP_PKEY *a, unsigned char **pp));
def(int, BIO_test_flags, (const BIO *b, int flags));
def(void, ERR_free_strings, (void));
def(void, CRYPTO_set_locking_callback, (void (*func)(int mode, int type, char const* file, int line)));
def(int, CRYPTO_num_locks, (void));
def(void, CRYPTO_cleanup_all_ex_data, (void));
def(void, ENGINE_cleanup, (void));
def(void, EVP_cleanup, (void));
def(void, CONF_modules_unload, (int));
def(void, CONF_modules_free, (void));
def(DH *, DH_new, (void));
def(void, DH_free, (DH *dh));
def(int, DH_generate_parameters_ex, (DH *dh, int prime_len, int generator, BN_GENCB *cb));
def(int, DH_check, (const DH *dh, int *codes));
def(int, i2d_DHparams, (const DH *a, unsigned char **pp));
def(DH *, d2i_DHparams, (DH **a, const unsigned char **pp, long length));
def(EC_KEY *, EC_KEY_new_by_curve_name, (int nid));
def(void, EC_KEY_free, (EC_KEY *key));
def(unsigned char *, SHA512, (const unsigned char *d, size_t n,	unsigned char *md));

template<typename Ret, typename ...Args, typename ...Args2>
Ret safe_call(Ret(*f)(Args...), Args2&& ... args)
{
	Ret ret{};
	if (f) {
		ret = f(std::forward<Args2>(args)...);
	}

	return ret;
}

template<typename ...Args, typename ...Args2>
void safe_call(void(*f)(Args...), Args2&& ... args)
{
	if (f) {
		f(std::forward<Args2>(args)...);
	}
}


namespace {
static LPCTSTR diffie_hellman_params = _T("30820108028201010094BB8D4FA2977558C5EE8E7D382E9E3AA884DBB640B7BF28F22175F82CF983108C0E573AD4DEED66688004E9E61715F3D919D5662D2475757995E97B63DC7E3720C6FDFF8989CD0E6281C46C9B605137A3C00B5BB47F7A2E6B2FF033D440FCA25BFFC3B3956FDBBE5813DB944F2FDBD17305A2710C9405A5FC035E3BD0FC7FBC91A9C0245EE599298E7FBD94A66E3CCDC581A845828E97F5A6ECE2E943DF37A037E0EAD31CFEBEB2F882499D1CCA50B1E85B6CAF733144BE48F9A661B875E2F4F7E06FC076141FDCB301F0121546E7BE4453ED8481587D8643692BB616305D345374A6F3629EE310EEA878A24661BB0D75ACC349570755BD3489B6208A73414B020102FD");
}

/////////////////////////////////////////////////////////////////////////////
// CAsyncSslSocketLayer
std::recursive_mutex CAsyncSslSocketLayer::m_mutex;

int CAsyncSslSocketLayer::m_nSslRefCount = 0;
DLL CAsyncSslSocketLayer::m_sslDll1;
DLL CAsyncSslSocketLayer::m_sslDll2;
DH* CAsyncSslSocketLayer::m_dh = 0;

//Used internally by openssl via callbacks
static std::recursive_mutex *openssl_mutexes;

CAsyncSslSocketLayer::CAsyncSslSocketLayer(int minTlsVersion)
	: m_minTlsVersion(minTlsVersion)
{
}

CAsyncSslSocketLayer::~CAsyncSslSocketLayer()
{
	UnloadSSL();
	delete [] m_pNetworkSendBuffer;
	delete [] m_pRetrySendBuffer;
	delete [] m_pKeyPassword;
}

extern "C" static void locking_callback(int mode, int type, char const*, int)
{
	if (mode & CRYPTO_LOCK) {
		openssl_mutexes[type].lock();
	}
	else {
		openssl_mutexes[type].unlock();
	}
}

void clear_locking_callback()
{
	if (openssl_mutexes) {
		safe_call(pCRYPTO_set_locking_callback, (void(*)(int, int, const char*, int))0);
		delete [] openssl_mutexes;
		openssl_mutexes = 0;
	}
}

int CAsyncSslSocketLayer::InitSSL()
{
	if (m_bSslInitialized)
		return 0;

	simple_lock lock(m_mutex);

	if (!m_nSslRefCount) {
		if (!m_sslDll2.load(_T("libeay32.dll"))) {
			return SSL_FAILURE_LOADDLLS;
		}

		bool bError = false;
		proc(m_sslDll2, BIO_ctrl_pending);
		proc(m_sslDll2, BIO_read);
		proc(m_sslDll2, BIO_ctrl);
		proc(m_sslDll2, BIO_write);
		proc(m_sslDll2, BIO_ctrl_get_write_guarantee);
		proc(m_sslDll2, BIO_new_bio_pair);
		proc(m_sslDll2, BIO_new);
		proc(m_sslDll2, BIO_free);
		proc(m_sslDll2, i2t_ASN1_OBJECT);
		proc(m_sslDll2, OBJ_obj2nid);
		proc(m_sslDll2, X509_NAME_ENTRY_get_object);
		proc(m_sslDll2, X509_NAME_get_entry);
		proc(m_sslDll2, X509_NAME_entry_count);
		proc(m_sslDll2, X509_get_subject_name);
		proc(m_sslDll2, X509_get_issuer_name);
		proc(m_sslDll2, OBJ_nid2sn);
		proc(m_sslDll2, X509_NAME_ENTRY_get_data);
		proc(m_sslDll2, X509_STORE_CTX_set_error);
		proc(m_sslDll2, X509_digest);
		proc(m_sslDll2, EVP_sha1);
		proc(m_sslDll2, EVP_sha256);
		proc(m_sslDll2, X509_STORE_CTX_get_current_cert);
		proc(m_sslDll2, X509_STORE_CTX_get_error);
		proc(m_sslDll2, X509_free);
		proc(m_sslDll2, X509_get_pubkey);
		proc(m_sslDll2, BN_num_bits);
		proc(m_sslDll2, EVP_PKEY_free);
		proc(m_sslDll2, X509_STORE_CTX_get_ex_data);
		proc(m_sslDll2, X509_NAME_oneline);
		proc(m_sslDll2, X509_verify_cert_error_string);
		proc(m_sslDll2, X509_STORE_CTX_get_error_depth);
		proc(m_sslDll2, ERR_get_error);
		proc(m_sslDll2, ERR_error_string);
		proc(m_sslDll2, ASN1_STRING_to_UTF8);
		proc(m_sslDll2, CRYPTO_free);
		proc(m_sslDll2, RSA_generate_key);
		proc(m_sslDll2, X509_set_version);
		proc(m_sslDll2, X509_gmtime_adj);
		proc(m_sslDll2, X509_set_pubkey);
		proc(m_sslDll2, X509_NAME_add_entry_by_txt);
		proc(m_sslDll2, X509_NAME_add_entry_by_NID);
		proc(m_sslDll2, X509_set_issuer_name);
		proc(m_sslDll2, X509_sign);
		proc(m_sslDll2, EVP_PKEY_new);
		proc(m_sslDll2, EVP_PKEY_assign);
		proc(m_sslDll2, X509_new);
		proc(m_sslDll2, ASN1_INTEGER_set);
		proc(m_sslDll2, X509_get_serialNumber);
		proc(m_sslDll2, PEM_ASN1_write_bio);
		proc(m_sslDll2, i2d_X509);
		proc(m_sslDll2, BIO_s_mem);
		proc(m_sslDll2, i2d_PrivateKey);
		proc(m_sslDll2, BIO_test_flags);
		proc(m_sslDll2, CRYPTO_set_locking_callback);
		proc(m_sslDll2, CRYPTO_num_locks);
		proc(m_sslDll2, ERR_free_strings);
		proc(m_sslDll2, CRYPTO_cleanup_all_ex_data);
		proc(m_sslDll2, ENGINE_cleanup);
		proc(m_sslDll2, EVP_cleanup);
		proc(m_sslDll2, CONF_modules_unload);
		proc(m_sslDll2, CONF_modules_free);
		proc(m_sslDll2, DH_new);
		proc(m_sslDll2, DH_free);
		proc(m_sslDll2, DH_generate_parameters_ex);
		proc(m_sslDll2, DH_check);
		proc(m_sslDll2, i2d_DHparams);
		proc(m_sslDll2, d2i_DHparams);
		proc(m_sslDll2, EC_KEY_new_by_curve_name);
		proc(m_sslDll2, EC_KEY_free);
		proc(m_sslDll2, SHA512);

		if (bError) {
			DoUnloadLibrary();
			return SSL_FAILURE_LOADDLLS;
		}

		openssl_mutexes = new std::recursive_mutex[pCRYPTO_num_locks()];
		pCRYPTO_set_locking_callback(&locking_callback);

		if (!m_sslDll1.load(_T("ssleay32.dll"))) {
			DoUnloadLibrary();
			return SSL_FAILURE_LOADDLLS;
		}
		proc(m_sslDll1, SSL_state_string_long);
		proc(m_sslDll1, SSL_state);
		proc(m_sslDll1, SSL_set_info_callback);
		proc(m_sslDll1, SSL_set_bio);
		proc(m_sslDll1, SSL_set_connect_state);
		proc(m_sslDll1, SSL_set_session);
		proc(m_sslDll1, BIO_f_ssl);
		proc(m_sslDll1, SSL_new);
		proc(m_sslDll1, SSL_CTX_new);
		proc(m_sslDll1, SSLv23_method);
		proc(m_sslDll1, SSL_load_error_strings);
		proc(m_sslDll1, SSL_library_init);
		proc(m_sslDll1, SSL_CTX_free);
		proc(m_sslDll1, SSL_free);
		proc(m_sslDll1, SSL_get_error);
		proc(m_sslDll1, SSL_shutdown);
		proc(m_sslDll1, SSL_get_shutdown);
		proc(m_sslDll1, SSL_alert_type_string_long);
		proc(m_sslDll1, SSL_alert_desc_string_long);
		proc(m_sslDll1, SSL_CTX_set_verify);
		proc(m_sslDll1, SSL_CTX_get_cert_store);
		proc(m_sslDll1, SSL_get_verify_result);
		proc(m_sslDll1, SSL_get_peer_certificate);
		proc(m_sslDll1, SSL_get_version);
		proc(m_sslDll1, SSL_get_current_cipher);
		proc(m_sslDll1, SSL_CIPHER_get_name);
		proc(m_sslDll1, SSL_CIPHER_get_version);
		proc(m_sslDll1, SSL_get_ex_data_X509_STORE_CTX_idx);
		proc(m_sslDll1, SSL_CTX_load_verify_locations);
		proc(m_sslDll1, SSL_ctrl);
		proc(m_sslDll1, SSL_set_accept_state);
		proc(m_sslDll1, SSL_CTX_use_PrivateKey_file);
		proc(m_sslDll1, SSL_CTX_use_certificate_file);
		proc(m_sslDll1, SSL_CTX_check_private_key);
		proc(m_sslDll1, SSL_CTX_set_default_passwd_cb_userdata);
		proc(m_sslDll1, SSL_CTX_set_default_passwd_cb);
		proc(m_sslDll1, SSL_CTX_use_certificate_chain_file);
		proc(m_sslDll1, SSL_CTX_ctrl);
		proc(m_sslDll1, SSL_get_cipher_list);
		proc(m_sslDll1, SSL_set_cipher_list);
		proc(m_sslDll1, SSL_CTX_get0_certificate);
		proc(m_sslDll1, SSL_get_default_timeout);
		proc(m_sslDll1, SSL_CTX_set_timeout);
		proc(m_sslDll1, SSL_get_session);
		proc(m_sslDll1, SSL_get1_session);
		proc(m_sslDll1, SSL_SESSION_set_timeout);
		proc(m_sslDll1, SSL_CTX_flush_sessions);
		proc(m_sslDll1, SSL_CTX_remove_session);
		proc(m_sslDll1, SSL_CTX_sess_set_new_cb);
		proc(m_sslDll1, SSL_CTX_sess_set_remove_cb);
		proc(m_sslDll1, SSL_CTX_sess_set_get_cb);
		proc(m_sslDll1, SSL_CTX_callback_ctrl);
		proc(m_sslDll1, SSL_set_ex_data);
		proc(m_sslDll1, SSL_get_ex_data);

		if (bError) {
			DoUnloadLibrary();
			return SSL_FAILURE_LOADDLLS;
		}

		pSSL_load_error_strings();
		if (!pSSL_library_init()) {
			DoUnloadLibrary();
			return SSL_FAILURE_INITSSL;
		}

		if (!SetDiffieHellmanParameters(diffie_hellman_params)) {
			DoUnloadLibrary();
			return SSL_FAILURE_INITSSL;
		}
	}

	++m_nSslRefCount;

	m_bSslInitialized = true;

	return 0;
}

void CAsyncSslSocketLayer::OnReceive(int nErrorCode)
{
	if (m_bUseSSL) {
		if (m_bBlocking) {
			m_mayTriggerRead = true;
			return;
		}
		if (m_nNetworkError)
			return;

		m_mayTriggerRead = false;

		//Get number of bytes we can receive and store in the network input bio
		size_t len = pBIO_ctrl_get_write_guarantee(m_nbio);
		if (!len)
		{
			m_mayTriggerRead = true;
			TriggerEvents();
			return;
		}

		char buffer[65536];
		len = std::min(len, sizeof(buffer));

		int numread = 0;

		// Receive data
		numread = ReceiveNext(buffer, len);
		if (numread > 0)
		{
			//Store it in the network input bio and process data
			int numwritten = pBIO_write(m_nbio, buffer, numread);
			pBIO_ctrl(m_nbio, BIO_CTRL_FLUSH, 0, NULL);

			// I have no idea why this call is needed, but without it, connections
			// will stall. Perhaps it triggers some internal processing.
			// Also, ignore return value, don't do any error checking. This function
			// can report errors, even though a later call can succeed.
			char dummy;
			pBIO_read(m_sslbio, &dummy, 0);
		}
		else if (!numread)
		{
			if (GetLayerState() == connected)
				TriggerEvent(FD_CLOSE, nErrorCode, TRUE);
		}
		else if (numread == SOCKET_ERROR)
		{
			int nError = GetLastError();
			if (nError != WSAEWOULDBLOCK && nError != WSAENOTCONN)
			{
				m_nNetworkError = GetLastError();
				TriggerEvent(FD_CLOSE, 0, TRUE);
				return;
			}
		}

		if (m_pRetrySendBuffer)
		{
			int numwrite = pBIO_write(m_sslbio, m_pRetrySendBuffer, m_nRetrySendBufferLen);
			if (numwrite >= 0)
			{
				pBIO_ctrl(m_sslbio, BIO_CTRL_FLUSH, 0, NULL);
				delete [] m_pRetrySendBuffer;
				m_pRetrySendBuffer = 0;
			}
			else if (numwrite == -1)
			{
				if (!pBIO_test_flags(m_sslbio, BIO_FLAGS_SHOULD_RETRY))
				{
					if (PrintLastErrorMsg())
					{
						delete [] m_pRetrySendBuffer;
						m_pRetrySendBuffer = 0;

						SetLastError(WSAECONNABORTED);
						TriggerEvent(FD_CLOSE, 0, TRUE);
						return;
					}
				}
			}
		}

		if (shutDownState == ShutDownState::none && pSSL_get_shutdown(m_ssl) && pBIO_ctrl_pending(m_sslbio) <= 0) {
			if (DoShutDown() || GetLastError() == WSAEWOULDBLOCK) {
				TriggerEvent(FD_CLOSE, 0, TRUE);
			}
			else {
				m_nNetworkError = WSAECONNABORTED;
				WSASetLastError(WSAECONNABORTED);
				TriggerEvent(FD_CLOSE, WSAECONNABORTED, TRUE);
			}
			return;
		}

		if (shutDownState != ShutDownState::none) {
			DoShutDown();
		}

		TriggerEvents();
	}
	else
		TriggerEvent(FD_READ, nErrorCode, TRUE);
}

void CAsyncSslSocketLayer::OnSend(int nErrorCode)
{
	if (m_bUseSSL) {
		if (m_nNetworkError)
			return;

		m_mayTriggerWrite = false;

		//Send data in the send buffer
		while (m_nNetworkSendBufferLen) {
			int numsent = SendNext(m_pNetworkSendBuffer, m_nNetworkSendBufferLen);
			if (numsent == SOCKET_ERROR)
			{
				int nError = GetLastError();
				if (nError != WSAEWOULDBLOCK && nError != WSAENOTCONN)
				{
					m_nNetworkError = nError;
					TriggerEvent(FD_CLOSE, 0, TRUE);
				}
				return;
			}
			else if (!numsent)
			{
				if (GetLayerState() == connected)
					TriggerEvent(FD_CLOSE, nErrorCode, TRUE);
			}
			if (numsent == m_nNetworkSendBufferLen)
				m_nNetworkSendBufferLen = 0;
			else
			{
				memmove(m_pNetworkSendBuffer, m_pNetworkSendBuffer + numsent, m_nNetworkSendBufferLen - numsent);
				m_nNetworkSendBufferLen -= numsent;
			}
		}

		//Send the data waiting in the network bio
		size_t len = pBIO_ctrl_pending(m_nbio);
		char *buffer = new char[len];
		int numread = pBIO_read(m_nbio, buffer, len);
		if (numread <= 0)
			m_mayTriggerWrite = true;
		while (numread > 0)
		{
			int numsent = SendNext(buffer, numread);
			if (!numsent)
			{
				if (GetLayerState() == connected)
					TriggerEvent(FD_CLOSE, nErrorCode, TRUE);
			}
			if (numsent == SOCKET_ERROR || numsent < numread)
			{
				if (numsent == SOCKET_ERROR)
				{
					if (GetLastError() != WSAEWOULDBLOCK && GetLastError() != WSAENOTCONN)
					{
						m_nNetworkError = GetLastError();
						TriggerEvent(FD_CLOSE, 0, TRUE);
						delete [] buffer;
						return;
					}
					else
						numsent = 0;
				}

				// Add all data that was retrieved from the network bio but could not be sent to the send buffer.
				if (m_nNetworkSendBufferMaxLen < (m_nNetworkSendBufferLen + numread - numsent))
				{
					char * tmp = m_pNetworkSendBuffer;
					m_nNetworkSendBufferMaxLen = static_cast<int>((m_nNetworkSendBufferLen + numread - numsent) * 1.5);
					m_pNetworkSendBuffer = new char[m_nNetworkSendBufferMaxLen];
					if (tmp)
					{
						memcpy(m_pNetworkSendBuffer, tmp, m_nNetworkSendBufferLen);
						delete [] tmp;
					}
				}
				ASSERT(m_pNetworkSendBuffer);
				memcpy(m_pNetworkSendBuffer + m_nNetworkSendBufferLen, buffer, numread-numsent);
				m_nNetworkSendBufferLen += numread - numsent;
			}
			if (!numsent)
				break;
			len = pBIO_ctrl_pending(m_nbio);
			if (!len)
			{
				m_mayTriggerWrite = true;
				break;
			}
			delete [] buffer;
			buffer = new char[len];
			numread = pBIO_read(m_nbio, buffer, len);
			if (numread <= 0)
				m_mayTriggerWrite = true;
		}
		delete [] buffer;

		if (m_pRetrySendBuffer)
		{
			int numwrite = pBIO_write(m_sslbio, m_pRetrySendBuffer, m_nRetrySendBufferLen);
			if (numwrite >= 0)
			{
				pBIO_ctrl(m_sslbio, BIO_CTRL_FLUSH, 0, NULL);
				delete [] m_pRetrySendBuffer;
				m_pRetrySendBuffer = 0;
			}
			else if (numwrite == -1)
			{
				if (!pBIO_test_flags(m_sslbio, BIO_FLAGS_SHOULD_RETRY))
				{
					if (PrintLastErrorMsg())
					{
						delete [] m_pRetrySendBuffer;
						m_pRetrySendBuffer = 0;

						SetLastError(WSAECONNABORTED);
						TriggerEvent(FD_CLOSE, 0, TRUE);
						return;
					}
				}
			}
		}

		// No more data available, shutdown or ask
		if (shutDownState != ShutDownState::none) {
			DoShutDown();
		}
		TriggerEvents();
	}
	else
		TriggerEvent(FD_WRITE, nErrorCode, TRUE);
}

int CAsyncSslSocketLayer::Send(const void* lpBuf, int nBufLen, int nFlags)
{
	if (m_bUseSSL)
	{
		if (!lpBuf)
			return 0;
		if (m_bBlocking || m_pRetrySendBuffer)
		{
			m_mayTriggerWriteUp = true;
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}
		if (m_nNetworkError) {
			SetLastError(m_nNetworkError);
			return SOCKET_ERROR;
		}
		if (shutDownState != ShutDownState::none) {
			SetLastError(WSAESHUTDOWN);
			return SOCKET_ERROR;
		}
		if (!m_bSslEstablished) {
			m_mayTriggerWriteUp = true;
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}
		if (!nBufLen)
			return 0;

		if (m_onCloseCalled) {
			TriggerEvent(FD_CLOSE, 0, TRUE);
			return 0;
		}

		int len = pBIO_ctrl_get_write_guarantee(m_sslbio);
		if (nBufLen > len)
			nBufLen = len;
		if (nBufLen <= 0) {
			m_mayTriggerWriteUp = true;
			TriggerEvents();
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}

		m_pRetrySendBuffer = new char[nBufLen];
		m_nRetrySendBufferLen = nBufLen;
		memcpy(m_pRetrySendBuffer, lpBuf, nBufLen);

		int numwrite = pBIO_write(m_sslbio, m_pRetrySendBuffer, m_nRetrySendBufferLen);
		if (numwrite >= 0) {
			pBIO_ctrl(m_sslbio, BIO_CTRL_FLUSH, 0, NULL);
			delete [] m_pRetrySendBuffer;
			m_pRetrySendBuffer = 0;
		}
		else if (numwrite == -1)
		{
			if (pBIO_test_flags(m_sslbio, BIO_FLAGS_SHOULD_RETRY))
			{
				if (GetLayerState() == closed)
					return 0;
				else if (GetLayerState() != connected)
				{
					SetLastError(m_nNetworkError);
					return SOCKET_ERROR;
				}

				TriggerEvents();

				return nBufLen;
			}
			else
			{
				bool fatal = PrintLastErrorMsg();
				if (fatal)
				{
					delete [] m_pRetrySendBuffer;
					m_pRetrySendBuffer = 0;

					SetLastError(WSAECONNABORTED);
				}
				else
					SetLastError(WSAEWOULDBLOCK);
			}
			return SOCKET_ERROR;
		}

		m_mayTriggerWriteUp = true;
		TriggerEvents();

		return numwrite;
	}
	else {
		return SendNext(lpBuf, nBufLen, nFlags);
	}
}

int CAsyncSslSocketLayer::Receive(void* lpBuf, int nBufLen, int nFlags)
{
	if (m_bUseSSL) {
		if (m_bBlocking) {
			m_mayTriggerReadUp = true;
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}
		if (m_nNetworkError) {
			if ((!m_bSslEstablished || pBIO_ctrl(m_sslbio, BIO_CTRL_PENDING, 0, NULL)) && shutDownState == ShutDownState::none) {
				m_mayTriggerReadUp = true;
				TriggerEvents();
				return pBIO_read(m_sslbio, lpBuf, nBufLen);
			}
			WSASetLastError(m_nNetworkError);
			return SOCKET_ERROR;
		}
		if (shutDownState != ShutDownState::none) {
			SetLastError(WSAESHUTDOWN);
			return SOCKET_ERROR;
		}
		if (!nBufLen)
			return 0;
		if (!m_bSslEstablished || !pBIO_ctrl(m_sslbio, BIO_CTRL_PENDING, 0, NULL)) {
			if (GetLayerState() == closed)
				return 0;
			if (m_onCloseCalled) {
				TriggerEvent(FD_CLOSE, 0, TRUE);
				return 0;
			}
			else if (GetLayerState() != connected) {
				SetLastError(m_nNetworkError);
				return SOCKET_ERROR;
			}
			else if (pSSL_get_shutdown(m_ssl)) {
				if (DoShutDown() || GetLastError() == WSAEWOULDBLOCK) {
					TriggerEvent(FD_CLOSE, 0, TRUE);
					return 0;
				}

				m_nNetworkError = WSAECONNABORTED;
				WSASetLastError(WSAECONNABORTED);
				TriggerEvent(FD_CLOSE, WSAECONNABORTED, TRUE);
				return SOCKET_ERROR;
			}
			m_mayTriggerReadUp = true;
			TriggerEvents();
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}
		int numread = pBIO_read(m_sslbio, lpBuf, nBufLen);
		if (!numread) {
			if (pSSL_get_shutdown(m_ssl)) {
				if (DoShutDown() || GetLastError() == WSAEWOULDBLOCK) {
					TriggerEvent(FD_CLOSE, 0, TRUE);
					return 0;
				}

				m_nNetworkError = WSAECONNABORTED;
				WSASetLastError(WSAECONNABORTED);
				TriggerEvent(FD_CLOSE, WSAECONNABORTED, TRUE);
				return SOCKET_ERROR;
			}
			m_mayTriggerReadUp = true;
			TriggerEvents();
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}
		if (numread < 0) {
			if (!pBIO_test_flags(m_sslbio, BIO_FLAGS_SHOULD_RETRY)) {
				bool fatal = PrintLastErrorMsg();
				if (fatal) {
					m_nNetworkError = WSAECONNABORTED;
					WSASetLastError(WSAECONNABORTED);
					TriggerEvent(FD_CLOSE, 0, TRUE);
					return SOCKET_ERROR;
				}
			}

			if (pSSL_get_shutdown(m_ssl)) {
				if (DoShutDown() || GetLastError() == WSAEWOULDBLOCK) {
					TriggerEvent(FD_CLOSE, 0, TRUE);
					return 0;
				}

				m_nNetworkError = WSAECONNABORTED;
				WSASetLastError(WSAECONNABORTED);
				TriggerEvent(FD_CLOSE, WSAECONNABORTED, TRUE);
				return SOCKET_ERROR;
			}

			m_mayTriggerReadUp = true;
			TriggerEvents();
			SetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}

		m_mayTriggerReadUp = true;
		TriggerEvents();
		return numread;
	}
	else
		return ReceiveNext(lpBuf, nBufLen, nFlags);
}

void CAsyncSslSocketLayer::Close()
{
	shutDownState = ShutDownState::none;
	m_onCloseCalled = false;
	ResetSslSession();
	CloseNext();
}

BOOL CAsyncSslSocketLayer::Connect(const SOCKADDR *lpSockAddr, int nSockAddrLen)
{
	BOOL res = ConnectNext(lpSockAddr, nSockAddrLen);
	if (!res)
		if (GetLastError() != WSAEWOULDBLOCK)
			ResetSslSession();
	return res;
}

BOOL CAsyncSslSocketLayer::Connect(LPCTSTR lpszHostAddress, UINT nHostPort)
{
	BOOL res = ConnectNext(lpszHostAddress, nHostPort);
	if (!res)
		if (GetLastError()!=WSAEWOULDBLOCK)
			ResetSslSession();
	return res;
}

namespace {
unsigned char toHexDigit(unsigned char c)
{
	if (c >= 10) {
		return c + 'A' - 10;
	}
	else {
		return c + '0';
	}
}

unsigned char fromHexDigit(TCHAR c)
{
	if (c >= 'a')
		return c - 'a' + 10;
	else if (c >= 'A')
		return c - 'A' + 10;
	else
		return c - '0';
}
}

CStdString CAsyncSslSocketLayer::GenerateDiffieHellmanParameters()
{
	CStdString ret;

	if (m_bSslInitialized) {
		// Set DH parameters
		DH* dh = pDH_new();
		int res = pDH_generate_parameters_ex(dh, 2048, DH_GENERATOR_2, 0);
		if (res == 1) {
			int len = pi2d_DHparams(dh, 0);
			if (len > 0) {
				auto buf = new unsigned char[len];

				// Who designed this API?
				auto tmp = buf;
				if ((pi2d_DHparams(dh, &tmp) == len)) {

					// Convert to hex encoding
					for (int i = 0; i <= len; ++i) {
						ret += toHexDigit(buf[i] >> 4);
						ret += toHexDigit(buf[i] & 0xfu);
					}
				}

				delete [] buf;
			}
		}

		pDH_free(dh);
	}

	return ret;
}

bool CAsyncSslSocketLayer::SetDiffieHellmanParameters(CStdString const& params)
{
	bool ret{};
	if (m_dh) {
		pDH_free(m_dh);
		m_dh = 0;
	}

	if (!(params.GetLength() % 2)) {
		int const len = params.GetLength() / 2;
		auto buf = new unsigned char[len];
		for (int i = 0; i < len; ++i) {
			buf[i] = fromHexDigit(params[i * 2]) << 4;
			buf[i] += fromHexDigit(params[i * 2 + 1]);
		}

		auto * tmp = buf;
		m_dh = pd2i_DHparams(0, const_cast<const unsigned char**>(&tmp), len);
		if (m_dh) {

			int tmp{};
			if (pDH_check(m_dh, &tmp) == 1 && !tmp) {
				ret = true;
			}
		}
		delete [] buf;
	}

	if (!ret && m_dh) {
		pDH_free(m_dh);
		m_dh = 0;
	}

	return ret;
}

int CAsyncSslSocketLayer::InitSSLConnection(bool clientMode, CAsyncSslSocketLayer* primarySocket, bool require_session_reuse)
{
	if (m_bUseSSL)
		return 0;
	int res = InitSSL();
	if (res)
		return res;

	scoped_lock lock(m_mutex);
	m_require_session_reuse = require_session_reuse;
	m_primarySocket = primarySocket;
	if (primarySocket && primarySocket->m_ssl_ctx) {
		if (m_ssl_ctx) {
			ResetSslSession();
			return SSL_FAILURE_INITSSL;
		}

		m_ssl_ctx = primarySocket->m_ssl_ctx;
	}
	else if (!m_ssl_ctx) {
		if (!CreateContext() ) {
			ResetSslSession();
			return SSL_FAILURE_INITSSL;
		}

		if (clientMode) {
			USES_CONVERSION;
			pSSL_CTX_set_verify(m_ssl_ctx.get(), SSL_VERIFY_PEER, verify_callback);
			pSSL_CTX_load_verify_locations(m_ssl_ctx.get(), T2CA(m_CertStorage), 0);
		}
	}

	//Create new SSL session
	if (!(m_ssl = pSSL_new(m_ssl_ctx.get()))) {
		ResetSslSession();
		return SSL_FAILURE_INITSSL;
	}
	pSSL_set_ex_data(m_ssl, 0, this);

	// Disable (3)DES, RC4 and other weak and export ciphers
	// Also disable rarely used SEED and IDEA
	// We do not make use of PSK and SRP so disable them as well for good measure.
	pSSL_set_cipher_list(m_ssl, "DEFAULT:!eNULL:!aNULL:!DES:!3DES:!WEAK:!EXP:!LOW:!MD5:!RC4:!SEED:!IDEA:!PSK:!SRP");

	// Enable Diffie-Hellman
	if (m_dh) {
		pSSL_ctrl(m_ssl, SSL_CTRL_SET_TMP_DH, 0, (char *)m_dh);
	}

	// ECDH
	EC_KEY* ec_key = pEC_KEY_new_by_curve_name(NID_X9_62_prime256v1); // In future, consider using Curve25519 once supported by OpenSSL
    if (!ec_key) {
        ResetSslSession();
		return SSL_FAILURE_INITSSL;
    }
    pSSL_ctrl(m_ssl, SSL_CTRL_SET_TMP_ECDH, 0, (char *)ec_key);
    pEC_KEY_free(ec_key);

	lock.unlock();

	//Create bios
	m_sslbio = pBIO_new(pBIO_f_ssl());
	pBIO_new_bio_pair(&m_ibio, 32768, &m_nbio, 32768);

	if (!m_sslbio || !m_nbio || !m_ibio) {
		ResetSslSession();
		return SSL_FAILURE_INITSSL;
	}

	long options = /*pSSL_ctrl(m_ssl, SSL_CTRL_OPTIONS, 0, NULL)*/0;
	options |= SSL_OP_ALL;
	/*pSSL_ctrl(m_ssl, SSL_CTRL_OPTIONS, options, NULL);*/

	//Init SSL connection
	pSSL_set_session(m_ssl, NULL);
	if (clientMode)
		pSSL_set_connect_state(m_ssl);
	else
		pSSL_set_accept_state(m_ssl);
	pSSL_set_bio(m_ssl, m_ibio, m_ibio);
	pBIO_ctrl(m_sslbio, BIO_C_SET_SSL, BIO_NOCLOSE, m_ssl);

	pSSL_set_info_callback(m_ssl, apps_ssl_info_callback);

	// Initiate handshake
	pBIO_read(m_sslbio, (void *)1, 0);

	// Trigger FD_WRITE so that we can initialize SSL negotiation
	if (GetLayerState() == connected || GetLayerState() == attached) {
		TriggerEvent(FD_READ, 0);
		TriggerEvent(FD_WRITE, 0);
		TriggerEvent(FD_READ, 0, TRUE);
		TriggerEvent(FD_WRITE, 0, TRUE);
	}

	m_bUseSSL = true;

	return 0;
}

void CAsyncSslSocketLayer::ResetSslSession()
{
	delete [] m_pRetrySendBuffer;
	m_pRetrySendBuffer = 0;

	m_bFailureSent = FALSE;
	m_bBlocking = FALSE;
	m_nSslAsyncNotifyId++;
	m_nNetworkError = 0;
	m_bUseSSL = FALSE;
	m_nVerificationResult = 0;
	m_nVerificationDepth = 0;

	m_bSslEstablished = FALSE;
	pBIO_free(m_sslbio);
	if (m_ssl)
		pSSL_set_session(m_ssl,NULL);
	pBIO_free(m_nbio);
	pBIO_free(m_ibio);

	m_nNetworkSendBufferLen = 0;

	m_nbio = 0;
	m_ibio = 0;
	m_sslbio = 0;

	pSSL_free(m_ssl);
	m_ssl = 0;

	simple_lock lock(m_mutex);

	if (m_ssl_ctx.unique()) {
		pSSL_CTX_free(m_ssl_ctx.get());
	}
	m_ssl_ctx.reset();

	delete [] m_pKeyPassword;
	m_pKeyPassword = 0;
}

bool CAsyncSslSocketLayer::IsUsingSSL()
{
	return m_bUseSSL;
}

BOOL CAsyncSslSocketLayer::ShutDown()
{
	BOOL ret = DoShutDown();
	TriggerEvents();
	return ret;
}

BOOL CAsyncSslSocketLayer::DoShutDown()
{
	if (shutDownState == ShutDownState::shutDown) {
		return TRUE;
	}
	shutDownState = ShutDownState::shuttingDown;

	if (m_bUseSSL) {
		if (m_pRetrySendBuffer) {
			WSASetLastError(WSAEWOULDBLOCK);
			return false;
		}

		pSSL_shutdown(m_ssl);

		if (m_nNetworkSendBufferLen) {
			WSASetLastError(WSAEWOULDBLOCK);
			return FALSE;
		}

		// Empty read buffer
		char buffer[1000];
		int numread;
		do {
			numread = pBIO_read(m_sslbio, buffer, 1000);
		} while (numread > 0);

		if (pBIO_ctrl_pending(m_nbio)) {
			WSASetLastError(WSAEWOULDBLOCK);
			return FALSE;
		}
	}
	BOOL res = ShutDownNext();
	if( res ) {
		shutDownState = ShutDownState::shutDown;
		if (m_bUseSSL) {
			DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_INFO, SSL_INFO_SHUTDOWNCOMPLETE);
		}
	}

	return res;
}

namespace {
bool matching_session(SSL* control, SSL* data)
{
	if (!control || !data) {
		return false;
	}

	SSL_SESSION* control_session = pSSL_get_session(control);
	SSL_SESSION* data_session = pSSL_get_session(data);

	if (!control_session || !data_session) {
		return false;
	}

	// In case session tickets are used, the session_id isn't set on the control connection.
	// Compare the master key instead. OpenSSL is such a PITA to work with...
	//if (!control_session->session_id_length) {
	//	if (control_session->master_key_length != data_session->master_key_length ||
	//		memcmp(control_session->master_key, data_session->master_key, control_session->master_key_length))
	//	{
	//		return false;
	//	}
	//}
	//else {
	//	if (control_session->session_id_length != data_session->session_id_length ||
	//		memcmp(control_session->session_id, data_session->session_id, control_session->session_id_length))
	//	{
	//		return false;
	//	}
	//}

	return true;
}
}

void CAsyncSslSocketLayer::apps_ssl_info_callback(const SSL *s, int where, int ret)
{
	auto const pLayer = static_cast<CAsyncSslSocketLayer*>(pSSL_get_ex_data(s, 0));

	// Called while unloading?
	if (!pLayer || !pLayer->m_bUseSSL)
		return;

	char const* str;

	int const w = where & ~SSL_ST_MASK;
	if (w & SSL_ST_CONNECT)
		str = "SSL_connect";
	else if (w & SSL_ST_ACCEPT)
		str = "SSL_accept";
	else
		str = "undefined";

	int const bufsize = 4096;

	if (where & SSL_CB_LOOP) {
#if SSL_VERBOSE_INFO
		char *buffer = new char[bufsize];
		char const* state = pSSL_state_string_long(s);
		_snprintf(buffer, 4096, "%s: %s",
				str,
				state ? state : "unknown state" );
		buffer[bufsize - 1] = 0;
		pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERBOSE_INFO, 0, buffer);
#endif
	}
	else if (where & SSL_CB_ALERT) {
		str=(where & SSL_CB_READ)? "read" : "write";
		const char* desc = pSSL_alert_desc_string_long(ret);

		// Don't send close notify warning
		if (desc && strcmp(desc, "close notify")) {
			char *buffer = new char[bufsize];
			char const* alert = pSSL_alert_type_string_long(ret);
			_snprintf(buffer, bufsize, "SSL3 alert %s: %s: %s",
					str,
					alert ? alert : "unknown alert",
					desc);
			buffer[bufsize - 1] = 0;
			pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERBOSE_WARNING, 0, buffer);
		}
	}
	else if (where & SSL_CB_EXIT) {
		if (ret == 0) {
			char *buffer = new char[bufsize];
			char const* state = pSSL_state_string_long(s);
			_snprintf(buffer, bufsize, "%s: failed in %s",
					str,
					state ? state : "unknown state" );
			buffer[bufsize - 1] = 0;
			pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERBOSE_WARNING, 0, buffer);
			if (!pLayer->m_bFailureSent) {
				pLayer->m_bFailureSent = TRUE;
				pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_FAILURE, pLayer->m_bSslEstablished ? SSL_FAILURE_UNKNOWN : SSL_FAILURE_ESTABLISH);
			}
		}
		else if (ret < 0) {
			int error = pSSL_get_error(s, ret);
			if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
				char *buffer = new char[bufsize];
				char const* state = pSSL_state_string_long(s);
				_snprintf(buffer, bufsize, "%s: error %d in %s",
						str,
						error,
						state ? state : "unknown state");
				buffer[bufsize - 1] = 0;
				pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERBOSE_WARNING, 0, buffer);
				if (!pLayer->m_bFailureSent) {
					pLayer->m_bFailureSent = TRUE;
					pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_FAILURE, pLayer->m_bSslEstablished ? SSL_FAILURE_UNKNOWN : SSL_FAILURE_ESTABLISH);
				}
			}
		}
	}
	else if (where & SSL_CB_HANDSHAKE_DONE) {
		int error = pSSL_get_verify_result(pLayer->m_ssl);
		if (error) {
			pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERIFY_CERT, error);
			pLayer->m_bBlocking = TRUE;
			return;
		}

		if (pLayer->m_require_session_reuse) {
			SSL_SESSION* session = pSSL_get_session(pLayer->m_ssl);
			bool const reused = /*pSSL_ctrl(pLayer->m_ssl, SSL_CTRL_GET_SESSION_REUSED, 0, NULL) != 0*/false;
			if (!reused) {
				if (!pLayer->m_bFailureSent) {
					pLayer->m_bFailureSent = TRUE;
					pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_FAILURE, SSL_FAILURE_NO_SESSIONREUSE);
				}
				pLayer->m_bBlocking = TRUE;
				return;
			}
			else if (pLayer->m_primarySocket) {
				bool const match = matching_session(pLayer->m_primarySocket->m_ssl, pLayer->m_ssl);
				if (!match) {
					if (!pLayer->m_bFailureSent) {
						pLayer->m_bFailureSent = TRUE;
						pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_FAILURE, SSL_FAILURE_NO_SESSIONREUSE);
					}
					pLayer->m_bBlocking = TRUE;
					return;
				}
			}
		}

		pLayer->m_bSslEstablished = TRUE;
		pLayer->PrintSessionInfo();
		pLayer->DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_INFO, SSL_INFO_ESTABLISHED);

		pLayer->TriggerEvents();
	}
}


void CAsyncSslSocketLayer::UnloadSSL()
{
	if (!m_bSslInitialized)
		return;
	ResetSslSession();

	m_bSslInitialized = false;

	simple_lock lock(m_mutex);
	--m_nSslRefCount;
	if (m_nSslRefCount) {
		return;
	}

	DoUnloadLibrary();
}

void CAsyncSslSocketLayer::DoUnloadLibrary()
{
	if (m_dh) {
		safe_call(pDH_free, m_dh);
		m_dh = 0;
	}

	safe_call(pCONF_modules_free);
	safe_call(pENGINE_cleanup);
	safe_call(pEVP_cleanup);
	safe_call(pCONF_modules_unload, 1);
	safe_call(pCRYPTO_cleanup_all_ex_data);
	safe_call(pERR_free_strings);
	
	clear_locking_callback();

	m_sslDll1.clear();
	m_sslDll2.clear();
}

namespace {
void ParseX509Name(X509_NAME * name, t_SslCertData::t_Contact & contact )
{
	if (!name)
		return;

	int count = pX509_NAME_entry_count(name);
	for (int i=0; i < count; i++) {
		X509_NAME_ENTRY * pX509NameEntry = pX509_NAME_get_entry(name, i);
		if (!pX509NameEntry)
			continue;
		ASN1_OBJECT *pObject = pX509_NAME_ENTRY_get_object(pX509NameEntry);
		ASN1_STRING *pString = pX509_NAME_ENTRY_get_data(pX509NameEntry);
		CString str;

		unsigned char *out;
		int len = pASN1_STRING_to_UTF8(&out, pString);
		if (len > 0) {
			// Keep it huge
			LPWSTR unicode = new WCHAR[len * 10];
			memset(unicode, 0, sizeof(WCHAR) * len * 10);
			int unicodeLen = MultiByteToWideChar(CP_UTF8, 0, (const char *)out, len, unicode, len * 10);
			if (unicodeLen > 0)	{
				str = unicode;
			}
			delete [] unicode;
			pCRYPTO_free(out);
		}

		switch(pOBJ_obj2nid(pObject))
		{
		case NID_organizationName:
			_tcsncpy(contact.Organization, str, 255);
			contact.Organization[255] = 0;
			break;
		case NID_organizationalUnitName:
			_tcsncpy(contact.Unit, str, 255);
			contact.Unit[255] = 0;
			break;
		case NID_commonName:
			_tcsncpy(contact.CommonName, str, 255);
			contact.CommonName[255] = 0;
			break;
		case NID_pkcs9_emailAddress:
			_tcsncpy(contact.Mail, str, 255);
			contact.Mail[255] = 0;
			break;
		case NID_countryName:
			_tcsncpy(contact.Country, str, 255);
			contact.Country[255] = 0;
			break;
		case NID_stateOrProvinceName:
			_tcsncpy(contact.StateProvince, str, 255);
			contact.StateProvince[255] = 0;
			break;
		case NID_localityName:
			_tcsncpy(contact.Town, str, 255);
			contact.Town[255] = 0;
			break;
		default:
			if ( !pOBJ_nid2sn(pOBJ_obj2nid(pObject)) )
			{
				TCHAR tmp[20];
				_sntprintf(tmp, sizeof(tmp)/sizeof(TCHAR), _T("%d"), pOBJ_obj2nid(pObject));
				tmp[sizeof(tmp)/sizeof(TCHAR) - 1] = 0;
				int maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), tmp, maxlen);

				maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), _T("="), maxlen);

				maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), str, maxlen);

				maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), _T(";"), maxlen);
			}
			else
			{
				int maxlen = 1024 - _tcslen(contact.Other)-1;

				USES_CONVERSION;
				_tcsncpy(contact.Other+_tcslen(contact.Other), A2CT(pOBJ_nid2sn(pOBJ_obj2nid(pObject))), maxlen);

				maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), _T("="), maxlen);

				maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), str, maxlen);

				maxlen = 1024 - _tcslen(contact.Other)-1;
				_tcsncpy(contact.Other+_tcslen(contact.Other), _T(";"), maxlen);
			}
			break;
		}
	}
}

bool ParseTime(ASN1_UTCTIME *pTime, tm& out)
{
	char *v;
	int gmt = 0;
	int i;
	int y=0, M=0, d=0, h=0, m=0, s=0;

	i = pTime->length;
	v = (char *)pTime->data;

	if (i < 10) {
		return false;
	}
	if (v[i-1] == 'Z') gmt=1;
	for (i=0; i<10; i++) {
		if ((v[i] > '9') || (v[i] < '0')) {
			return false;
		}
	}
	y= (v[0]-'0')*10+(v[1]-'0');
	if (y < 50) y+=100;
	M= (v[2]-'0')*10+(v[3]-'0');
	if ((M > 12) || (M < 1)) {
		return false;
	}
	d= (v[4]-'0')*10+(v[5]-'0');
	h= (v[6]-'0')*10+(v[7]-'0');
	m=  (v[8]-'0')*10+(v[9]-'0');
	if (	(v[10] >= '0') && (v[10] <= '9') &&
		(v[11] >= '0') && (v[11] <= '9'))
		s=  (v[10]-'0')*10+(v[11]-'0');

	out.tm_year = y;
	out.tm_mon = M;
	out.tm_mday = d;
	out.tm_hour = h;
	out.tm_min = m;
	out.tm_sec = s;

	return true;
}
}

BOOL CAsyncSslSocketLayer::GetPeerCertificateData(t_SslCertData &SslCertData)
{
	X509 *pX509=pSSL_get_peer_certificate(m_ssl);
	if (!pX509)
		return FALSE;

	//Reset the contents of SslCertData
	memset(&SslCertData, 0, sizeof(t_SslCertData));

	//Set subject data fields
	ParseX509Name(pX509_get_subject_name(pX509), SslCertData.subject);
	ParseX509Name(pX509_get_issuer_name(pX509), SslCertData.issuer);

	//Validity span
	if (!ParseTime(X509_get_notBefore(pX509), SslCertData.validFrom) ||
		!ParseTime(X509_get_notAfter(pX509), SslCertData.validUntil))
	{
		pX509_free(pX509);
		return FALSE;
	}

	unsigned int length = 20;
	pX509_digest(pX509, pEVP_sha1(), SslCertData.hash, &length);

	SslCertData.priv_data = m_nSslAsyncNotifyId;

	pX509_free(pX509);

	SslCertData.verificationResult = m_nVerificationResult;
	SslCertData.verificationDepth = m_nVerificationDepth;

	return TRUE;
}

void CAsyncSslSocketLayer::SetNotifyReply(int nID, int nCode, int result)
{
	if (!m_bBlocking)
		return;
	if (nID != m_nSslAsyncNotifyId)
		return;
	if (nCode != SSL_VERIFY_CERT)
		return;

	m_bBlocking = FALSE;

	if (!result) {
		m_nNetworkError = WSAECONNABORTED;
		WSASetLastError(WSAECONNABORTED);
		if (!m_bFailureSent) {
			m_bFailureSent = TRUE;
			DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_FAILURE, SSL_FAILURE_CERTREJECTED);
		}
		TriggerEvent(FD_CLOSE, 0, TRUE);
		return;
	}
	m_bSslEstablished = TRUE;
	PrintSessionInfo();
	DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_INFO, SSL_INFO_ESTABLISHED);

	TriggerEvents();
}

void CAsyncSslSocketLayer::PrintSessionInfo()
{
#if SSL_VERBOSE_INFO
	SSL_CIPHER *ciph;
	X509 *cert;

	ciph = pSSL_get_current_cipher(m_ssl);
	char enc[4096] = {0};
	cert=pSSL_get_peer_certificate(m_ssl);

	if (cert != NULL)
	{
		EVP_PKEY *pkey = pX509_get_pubkey(cert);
		if (pkey != NULL)
		{
			if (0)
				;
#ifndef NO_RSA
			else if (pkey->type == EVP_PKEY_RSA && pkey->pkey.rsa != NULL
				&& pkey->pkey.rsa->n != NULL)
				sprintf(enc,	"%d bit RSA", pBN_num_bits(pkey->pkey.rsa->n));
#endif
#ifndef NO_DSA
			else if (pkey->type == EVP_PKEY_DSA && pkey->pkey.dsa != NULL
					&& pkey->pkey.dsa->p != NULL)
				sprintf(enc,	"%d bit DSA", pBN_num_bits(pkey->pkey.dsa->p));
#endif
			pEVP_PKEY_free(pkey);
		}
		pX509_free(cert);
		/* The SSL API does not allow us to look at temporary RSA/DH keys,
		 * otherwise we should print their lengths too */
	}

	char *buffer = new char[4096];
	sprintf(buffer, "Using %s, cipher %s: %s, %s",
			pSSL_get_version(m_ssl),
			pSSL_CIPHER_get_version(ciph),
			pSSL_CIPHER_get_name(ciph),
			enc);
	DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERBOSE_INFO, 0, buffer);
#endif
}

void CAsyncSslSocketLayer::OnConnect(int nErrorCode)
{
	if (m_bUseSSL && nErrorCode)
		TriggerEvent(FD_WRITE, 0);
	TriggerEvent(FD_CONNECT, nErrorCode, TRUE);
}

int CAsyncSslSocketLayer::verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	X509   *err_cert;
	int     err, depth;
	SSL    *ssl;

	err_cert = pX509_STORE_CTX_get_current_cert(ctx);
	err = pX509_STORE_CTX_get_error(ctx);
	depth = pX509_STORE_CTX_get_error_depth(ctx);

	/*
	 * Retrieve the pointer to the SSL of the connection currently treated
	 * and the application specific data stored into the SSL object.
	 */
	ssl = (SSL *)pX509_STORE_CTX_get_ex_data(ctx, pSSL_get_ex_data_X509_STORE_CTX_idx());
	if (!ssl) {
		return 1;
	}

	auto const pLayer = static_cast<CAsyncSslSocketLayer*>(pSSL_get_ex_data(ssl, 0));
	if (!pLayer) {
		return 1;
	}

	/*
	 * Catch a too long certificate chain. The depth limit set using
	 * SSL_CTX_set_verify_depth() is by purpose set to "limit+1" so
	 * that whenever the "depth>verify_depth" condition is met, we
	 * have violated the limit and want to log this error condition.
	 * We must do it here, because the CHAIN_TOO_LONG error would not
	 * be found explicitly; only errors introduced by cutting off the
	 * additional certificates would be logged.
	 */
	if (depth > 10) {//mydata->verify_depth) {
		preverify_ok = 0;
		err = X509_V_ERR_CERT_CHAIN_TOO_LONG;
		pX509_STORE_CTX_set_error(ctx, err);
	}

	if (!preverify_ok) {
		if (!pLayer->m_nVerificationResult) {
			pLayer->m_nVerificationDepth = depth;
			pLayer->m_nVerificationResult = err;
		}
	}
	return 1;
}

BOOL CAsyncSslSocketLayer::SetCertStorage(CString const& file)
{
	m_CertStorage = file;
	return TRUE;
}

void CAsyncSslSocketLayer::OnClose(int nErrorCode)
{
	m_onCloseCalled = true;
	if (m_bUseSSL && pBIO_ctrl && pBIO_ctrl(m_sslbio, BIO_CTRL_PENDING, 0, NULL) > 0)
	{
		TriggerEvents();
	}
	else
		TriggerEvent(FD_CLOSE, nErrorCode, TRUE);
}

int CAsyncSslSocketLayer::GetLastSslError(CString& e)
{
	int err = 0;
	if( pERR_get_error && pERR_error_string ) {
		USES_CONVERSION;

		int err = pERR_get_error();
		char buffer[512];
		pERR_error_string(err, buffer);
		e = A2CT(buffer);
	}

	return err;
}

bool CAsyncSslSocketLayer::PrintLastErrorMsg()
{
	if (!pERR_get_error)
		return false;

	bool fatal = false;
	int err = pERR_get_error();
	while (err)
	{
		// Something about an undefined const function or
		// so, no idea where that comes from. OpenSSL is a mess
		if (err != 336539714)
			fatal = true;
		char *buffer = new char[512];
		pERR_error_string(err, buffer);
		err = pERR_get_error();
		DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, SSL_VERBOSE_WARNING, 0, buffer);
	}

	return fatal;
}

void CAsyncSslSocketLayer::ClearErrors()
{
	int err = pERR_get_error();
	while (err)
		err = pERR_get_error();
}

namespace {
void add_dn(X509_NAME* dn, const char* name, const unsigned char* value, int type = V_ASN1_UTF8STRING)
{
	if (!name || !*name || !value || !*value)
		return;

	pX509_NAME_add_entry_by_txt(dn, name,
		type, value, -1, -1, 0);
}
}

bool CAsyncSslSocketLayer::CreateSslCertificate(LPCTSTR filename, int bits, const unsigned char* country, const unsigned char* state,
			const unsigned char* locality, const unsigned char* organization, const unsigned char* unit, const unsigned char* cname,
			const unsigned char *email, CString& err)
{
	// Certificate valid for a year
	int days = 365;

	CAsyncSslSocketLayer layer(0);
	if (layer.InitSSL()) {
		err = _T("Failed to initialize SSL library");
		return false;
	}

	X509 *x;
	EVP_PKEY *pk;
	RSA *rsa;
	X509_NAME *name = NULL;

	if ((pk = pEVP_PKEY_new()) == NULL) {
		err = _T("Could not create key object");
		return false;
	}

	if ((x = pX509_new()) == NULL) {
		err = _T("Could not create certificate object");
		return false;
	}

	rsa = pRSA_generate_key(bits, RSA_F4, 0/*callback*/, NULL);

	if (!pEVP_PKEY_assign(pk, EVP_PKEY_RSA, (char *)(rsa))) {
		err = _T("Failed to assign rsa key to key object");
		return false;
	}

	rsa = NULL;

	pX509_set_version(x,2);
	pASN1_INTEGER_set(pX509_get_serialNumber(x), 0/*serial*/);
	pX509_gmtime_adj(X509_get_notBefore(x),0);
	pX509_gmtime_adj(X509_get_notAfter(x),(long)60*60*24*days);
	pX509_set_pubkey(x,pk);

	name = pX509_get_subject_name(x);

	/* This function creates and adds the entry, working out the
	 * correct string type and performing checks on its length.
	 * Normally we'd check the return value for errors...
	 */
	add_dn(name, "CN", cname);
	add_dn(name, "C", country, MBSTRING_UTF8);
	add_dn(name, "ST", state);
	add_dn(name, "L", locality);
	add_dn(name, "O", organization);
	add_dn(name, "OU", unit);
	if (email && *email)
		pX509_NAME_add_entry_by_NID(name, NID_pkcs9_emailAddress,
				MBSTRING_UTF8, const_cast<unsigned char*>(email), -1, -1, 0);

	/* Its self signed so set the issuer name to be the same as the
	 * subject.
	 */
	pX509_set_issuer_name(x,name);

	if (!pX509_sign(x, pk, pEVP_sha256())) {
		err = _T("Failed to sign certificate");
		return false;
	}

	// Write key and certificate to file
	// We use a memory bio, since the OpenSSL functions accepting a filepointer
	// do crash for no obvious reason.
	FILE* file = _wfopen(filename, _T("w+"));
	if (!file) {
		err = _T("Failed to open output file");
		return false;
	}

	BIO* bio = pBIO_new(pBIO_s_mem());
	//pPEM_ASN1_write_bio((int (*)())pi2d_PrivateKey, (((pk)->type == EVP_PKEY_DSA)?PEM_STRING_DSA:PEM_STRING_RSA), bio, (char *)pk, NULL, NULL, 0, NULL, NULL);
	//pPEM_ASN1_write_bio((int (*)())pi2d_X509, PEM_STRING_X509, bio, (char *)x, NULL, NULL, 0, NULL, NULL);

	char buffer[1001];
	int len;
	while ((len = pBIO_read(bio, buffer, 1000)) > 0) {
		buffer[len] = 0;
		fprintf(file, buffer);
	}

	fclose(file);

	pX509_free(x);
	pEVP_PKEY_free(pk);

	pBIO_free(bio);

	layer.UnloadSSL();

	return true;
}

bool operator<(tm const& lhs, tm const& rhs) {
	auto l = std::tie(lhs.tm_year, lhs.tm_mon, lhs.tm_mday, lhs.tm_hour, lhs.tm_min, lhs.tm_sec);
	auto r = std::tie(rhs.tm_year, rhs.tm_mon, rhs.tm_mday, rhs.tm_hour, rhs.tm_min, rhs.tm_sec);
	return l < r;
}

int CAsyncSslSocketLayer::LoadCertKeyFile(char const* cert, char const* key, CString* error, bool checkExpired)
{
	ClearErrors();
	if (!cert || !*cert) {
		error->Format(_T("Could not load certificate file: Invalid file name"));
		return SSL_FAILURE_VERIFYCERT;
	}
	else if (pSSL_CTX_use_certificate_chain_file(m_ssl_ctx.get(), cert) <= 0) {
		if (error) {
			CStdString e;
			int err = GetLastSslError(e);
			error->Format(_T("Could not load certificate file: %s (%d)"), e, err );
		}
		return SSL_FAILURE_VERIFYCERT;
	}

	if (!key || !*key) {
		error->Format(_T("Could not load key file: Invalid file name"));
		return SSL_FAILURE_VERIFYCERT;
	}
	else if (pSSL_CTX_use_PrivateKey_file(m_ssl_ctx.get(), key, SSL_FILETYPE_PEM) <= 0) {
		if (error) {
			CStdString e;
			int err = GetLastSslError(e);
			error->Format(_T("Could not load key file: %s (%d)"), e, err );
		}
		return SSL_FAILURE_VERIFYCERT;
	}

	if (!pSSL_CTX_check_private_key(m_ssl_ctx.get())) {
		if (error) {
			CStdString e;
			int err = GetLastSslError(e);
			error->Format(_T("Private key does not match the certificate public key: %s (%d)"), e, err );
		}
		return SSL_FAILURE_VERIFYCERT;
	}

	if (checkExpired) {
		X509* cert = pSSL_CTX_get0_certificate(m_ssl_ctx.get());
		if (cert) {
			// I'd really like to use ASN1_UTCTIME_get here, but it's behind #if 0 without any alternative...
			tm v;
			if (ParseTime(X509_get_notAfter(cert), v)) {
				time_t now = time(0);
				tm* t = gmtime(&now);
				if (t && v < *t) {
					if (error) {
						CStdString e;
						int err = GetLastSslError(e);
						error->Format(_T("The configured TLS certificate is expired"), e, err );
					}
					return SSL_FAILURE_VERIFYCERT;
				}
			}
		}
	}

	return 0;
}

int CAsyncSslSocketLayer::SetCertKeyFile(CString const& cert, CString const& key, CString const& pass, CString* error, bool checkExpired)
{
	int res = InitSSL();
	if (res)
		return res;

	simple_lock lock(m_mutex);
	if (!CreateContext()) {
		return SSL_FAILURE_INITSSL;
	}

	pSSL_CTX_set_default_passwd_cb(m_ssl_ctx.get(), pem_passwd_cb);
	pSSL_CTX_set_default_passwd_cb_userdata(m_ssl_ctx.get(), this);

	delete [] m_pKeyPassword;
	m_pKeyPassword = 0;

	USES_CONVERSION;
	char const* ascii_pass = T2CA(pass);
	if (ascii_pass && *ascii_pass) {
		size_t len = strlen(ascii_pass);
		m_pKeyPassword = new char[len + 1];
		strcpy(m_pKeyPassword, ascii_pass);
	}

	char const* ascii_cert = T2CA(cert);
	char const* ascii_key = T2CA(key);
	res = LoadCertKeyFile(ascii_cert, ascii_key, error, checkExpired);

	pSSL_CTX_set_default_passwd_cb_userdata(m_ssl_ctx.get(), 0);

	return res;
}

int CAsyncSslSocketLayer::SendRaw(const void* lpBuf, int nBufLen)
{
	if (!m_bUseSSL) {
		SetLastError(WSANOTINITIALISED);
		return SOCKET_ERROR;
	}

	if (!lpBuf)
		return 0;

	if (m_nNetworkError) {
		SetLastError(m_nNetworkError);
		return SOCKET_ERROR;
	}
	if (shutDownState != ShutDownState::none) {
		SetLastError(WSAESHUTDOWN);
		return SOCKET_ERROR;
	}
	if (m_nNetworkSendBufferLen) {
		SetLastError(WSAEINPROGRESS);
		return SOCKET_ERROR;
	}
	if (!nBufLen)
		return 0;

	if (m_nNetworkSendBufferMaxLen < nBufLen)
		m_nNetworkSendBufferMaxLen = nBufLen;
	delete [] m_pNetworkSendBuffer;
	m_pNetworkSendBuffer = new char[m_nNetworkSendBufferMaxLen];
	memcpy(m_pNetworkSendBuffer, lpBuf, nBufLen);
	m_nNetworkSendBufferLen = nBufLen;
	TriggerEvent(FD_WRITE, 0);

	return nBufLen;
}

void CAsyncSslSocketLayer::TriggerEvents()
{
	if (pBIO_ctrl_pending(m_nbio) > 0) {
		if (m_mayTriggerWrite) {
			m_mayTriggerWrite = false;
			TriggerEvent(FD_WRITE, 0);
		}
	}
	else if (!m_nNetworkSendBufferLen && m_bSslEstablished && !m_pRetrySendBuffer && pBIO_ctrl_get_write_guarantee(m_sslbio) > 0 && m_mayTriggerWriteUp) {
		m_mayTriggerWriteUp = false;
		TriggerEvent(FD_WRITE, 0, TRUE);
	}
	else if (!m_nNetworkSendBufferLen && !m_bSslEstablished && !m_pRetrySendBuffer && pBIO_ctrl_get_write_guarantee(m_sslbio) > 0) {
		if (!m_bFailureSent && shutDownState == ShutDownState::none && !m_onCloseCalled) {
			// Continue handshake.
			char dummy;
			pBIO_write(m_sslbio, &dummy, 0);
			if (pBIO_ctrl_pending(m_nbio) > 0) {
				if (m_mayTriggerWrite) {
					m_mayTriggerWrite = false;
					TriggerEvent(FD_WRITE, 0);
				}
			}
		}
	}

	if (m_bSslEstablished && pBIO_ctrl_pending(m_sslbio) > 0) {
		if (m_mayTriggerReadUp && !m_bBlocking) {
			m_mayTriggerReadUp = false;
			TriggerEvent(FD_READ, 0, TRUE);
		}
	}
	else if (pBIO_ctrl_get_write_guarantee(m_nbio) > 0 && m_mayTriggerRead) {
		m_mayTriggerRead = false;
		TriggerEvent(FD_READ, 0);
	}

	if (m_onCloseCalled && m_bSslEstablished && pBIO_ctrl_pending(m_sslbio) <= 0)
		TriggerEvent(FD_CLOSE, 0, TRUE);
}

int CAsyncSslSocketLayer::pem_passwd_cb(char *buf, int size, int, void *userdata)
{
	CAsyncSslSocketLayer* pThis = (CAsyncSslSocketLayer*)userdata;

	if (!pThis || !pThis->m_pKeyPassword || size < 1)
		return 0;

	int len = strlen(pThis->m_pKeyPassword);
	if (len >= size)
		len = size - 1;

	memcpy(buf, pThis->m_pKeyPassword, len);
	buf[len] = 0;

	return len;
}

namespace {
void null_deleter(void*)
{
}
}

bool CAsyncSslSocketLayer::CreateContext()
{
	if (m_ssl_ctx)
		return true;

	auto ctx = pSSL_CTX_new(pSSLv23_method());
	if (!ctx) {
		return false;
	}
	m_ssl_ctx = decltype(m_ssl_ctx)(ctx, &null_deleter);

	long options = /*pSSL_CTX_ctrl(m_ssl_ctx.get(), SSL_CTRL_OPTIONS, 0, NULL)*/0;
	options |= SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3; // todo: add option so that users can further tighten requirements
	options |= SSL_OP_SINGLE_DH_USE | SSL_OP_SINGLE_ECDH_USE; // Require a new (EC)DH key with each connection. Of course the OpenSSL documentation only documents the former, not the latter.
	options &= ~(SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION|SSL_OP_LEGACY_SERVER_CONNECT);
	
	// Fall-throughs are intentional
	switch (m_minTlsVersion) {
	default:
	case 2:
		options |= SSL_OP_NO_TLSv1_1;
	case 1:
		options |= SSL_OP_NO_TLSv1;
	case 0:
		break;
	}

    //added by zhangyl
	/*pSSL_CTX_ctrl(m_ssl_ctx.get(), SSL_CTRL_OPTIONS, options, NULL);*/

	pSSL_CTX_ctrl(m_ssl_ctx.get(), SSL_CTRL_SET_SESS_CACHE_MODE, SSL_SESS_CACHE_BOTH|SSL_SESS_CACHE_NO_AUTO_CLEAR, NULL);

	pSSL_CTX_set_timeout(m_ssl_ctx.get(), 2000000000);

	return true;
}

std::string CAsyncSslSocketLayer::SHA512(unsigned char const* buf, size_t len)
{
	std::string ret;

	unsigned char out[SHA512_DIGEST_LENGTH];

	InitSSL();

	if (pSHA512) {
		if (pSHA512(buf, len, out)) {
			// Convert to hex encoding
			for (int i = 0; i < SHA512_DIGEST_LENGTH; ++i) {
				ret += toHexDigit(out[i] >> 4);
				ret += toHexDigit(out[i] & 0xfu);
			}
		}
	}

	return ret;
}