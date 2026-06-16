// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/_base/Credentials.hpp>

#include <utility>
#include <fstream>
#include <ranges>
#include <vector>
#include <string>

static std::expected<std::vector<std::byte>, Hermes::CredentialErrorEnum> S_ReadCertFromFile(const std::filesystem::path& path) noexcept {
    std::ifstream file{ path, std::ios::binary | std::ios::ate };

    if (!file.is_open())
        return std::unexpected{ Hermes::CredentialErrorEnum::FileReadFailed };

    const auto fileSize{ file.tellg() };
    if (fileSize <= 0)
        return std::unexpected{ Hermes::CredentialErrorEnum::FileReadFailed };

    std::vector<std::byte> buffer(fileSize);

    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
        return std::unexpected{ Hermes::CredentialErrorEnum::FileReadFailed };

    return buffer;
}


#ifdef _WIN32
#include <Hermes/_base/OsApi/Enums/Windows/EncryptStatusEnum.hpp>

static Hermes::CredentialErrorEnum S_MapSecurityStatus(SECURITY_STATUS status) noexcept {
    using Hermes::EncryptStatusEnum;

    switch (static_cast<EncryptStatusEnum>(status)) {
        case EncryptStatusEnum::ErrOk                 : return Hermes::CredentialErrorEnum::None;
        case EncryptStatusEnum::ErrSecPkgNotFound     : return Hermes::CredentialErrorEnum::SecurityPackageNotFound;
        case EncryptStatusEnum::ErrUnknownCredentials : return Hermes::CredentialErrorEnum::UnknownCredentials;
        case EncryptStatusEnum::ErrNoCredentials      : return Hermes::CredentialErrorEnum::NoCredentials;
        case EncryptStatusEnum::ErrInsufficientMemory : return Hermes::CredentialErrorEnum::InsufficientMemory;
        case EncryptStatusEnum::ErrInternalError      : return Hermes::CredentialErrorEnum::InternalError;
        case EncryptStatusEnum::ErrUnsupportedFunction: return Hermes::CredentialErrorEnum::UnsupportedFunction;
        default                                       : return Hermes::CredentialErrorEnum::Unknown;
    }
}


struct CertificateInfo {
    HCERTSTORE certStore;
    PCCERT_CONTEXT certContext;
    BOOL hasPrivateKey;
};

static std::expected<CertificateInfo, Hermes::CredentialErrorEnum> ImportCertificate(
    const std::span<const std::byte> certBuffer, const wchar_t* password) noexcept {
    using Hermes::CredentialErrorEnum;


    CRYPT_DATA_BLOB blob{
        static_cast<DWORD>(certBuffer.size()),
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(certBuffer.data()))
    };

    HCERTSTORE certStore{ PFXImportCertStore(&blob, password, CRYPT_EXPORTABLE | CRYPT_USER_KEYSET) };
    if (!certStore)
        return std::unexpected{ ::GetLastError() == ERROR_INVALID_PASSWORD
            ? CredentialErrorEnum::InvalidPassword
            : CredentialErrorEnum::BadCertificateFormat };

    PCCERT_CONTEXT certContext{ CertEnumCertificatesInStore(certStore, nullptr) };
    if (!certContext) {
        CertCloseStore(certStore, 0);
        return std::unexpected{ CredentialErrorEnum::CertificateNotFound };
    }

    DWORD cbData{};
    BOOL hasPrivateKey{ CertGetCertificateContextProperty(certContext, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &cbData) };

    return CertificateInfo{ certStore, certContext, hasPrivateKey };
}


namespace Hermes {

    struct Credentials::Impl {
        CredHandle              credHandle{};
        SupportedProtocolsFlags protocolFlags{};
        CredentialFlags         credentialFlags{};
        TimeStamp               tsExpiry{};
        bool                    hasPrivateKey{};

        HCERTSTORE     certStore{ nullptr };
        PCCERT_CONTEXT certContext{ nullptr };

        ~Impl() noexcept {
            if (credHandle.dwLower != 0 || credHandle.dwUpper != 0) {
                ::FreeCredentialsHandle(&credHandle);
                credHandle = {};
            }
            if (certContext) {
                ::CertFreeCertificateContext(certContext);
                certContext = nullptr;
            }
            if (certStore) {
                ::CertCloseStore(certStore, 0);
                certStore = nullptr;
            }
        }
    };

#pragma region FromCertificate

    std::expected<Credentials, CredentialErrorEnum> Credentials::FromCertificate(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, CredentialFlags credFlags, const wchar_t *password, bool useSelfStore) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data){ return FromCertificate(data, sChannelFlags, credFlags, password, useSelfStore); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::FromCertificate(std::span<const std::byte> certBuffer,
                                                                                 SChannelCredFlags sChannelFlags, CredentialFlags credFlags, const wchar_t *password, bool useSelfStore) noexcept {
        const auto s_makeCreds = [=](CertificateInfo certInfo) -> std::expected<Credentials, CredentialErrorEnum> {
            Credentials creds{};
            creds._impl->credentialFlags = credFlags;
            creds._impl->certStore       = certInfo.certStore;
            creds._impl->certContext     = certInfo.certContext;
            creds._impl->hasPrivateKey   = certInfo.hasPrivateKey;

            SCH_CREDENTIALS cred{
                .dwVersion = static_cast<DWORD>(SChCredEnum::SchCredentials),
                .cCreds    = 1,
                .paCred    = &creds._impl->certContext,

                .hRootStore     = useSelfStore ? creds._impl->certStore : nullptr,
                .cTlsParameters = 0,
                .pTlsParameters = nullptr,
            };

            const SECURITY_STATUS status{ ::AcquireCredentialsHandleA(
                nullptr, const_cast<LPSTR>(macroUNISP_NAME.data()), static_cast<unsigned long>(creds._impl->credentialFlags),
                nullptr, &cred, nullptr, nullptr, &creds._impl->credHandle, &creds._impl->tsExpiry
            ) };

            if (status != EncryptStatusEnum::ErrOk) return std::unexpected{ S_MapSecurityStatus(status) }; // sai com ErrAlgorithmMismatch
            return creds;
        };

        return ImportCertificate(certBuffer, password)
                .and_then(s_makeCreds);
    }

#pragma endregion


#pragma region Client Credentials

    std::expected<Credentials, CredentialErrorEnum> Credentials::Client(SChannelCredFlags sChannelFlags) noexcept {
        Credentials creds{};
        creds._impl->credentialFlags = CredentialFlags::Outbound;

        SCH_CREDENTIALS cred{
            .dwVersion = static_cast<DWORD>(SChCredEnum::SchCredentials),

            .hRootStore     = nullptr,
            .dwFlags        = static_cast<DWORD>(sChannelFlags),

            .cTlsParameters = 0,
            .pTlsParameters = nullptr,
        };


        const SECURITY_STATUS status{ ::AcquireCredentialsHandleA(
            nullptr, const_cast<LPSTR>(macroUNISP_NAME.data()), static_cast<unsigned long>(creds._impl->credentialFlags),
            nullptr, &cred, nullptr, nullptr, &creds._impl->credHandle, &creds._impl->tsExpiry
        ) };

        if (status != EncryptStatusEnum::ErrOk) return std::unexpected{ S_MapSecurityStatus(status) };
        return creds;
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Client(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data){ return Client(data, sChannelFlags, password); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Client(const std::span<const std::byte> certBuffer,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return FromCertificate(certBuffer, sChannelFlags, CredentialFlags::Outbound, password);
    }

#pragma endregion


#pragma region Server Credentials

    std::expected<Credentials, CredentialErrorEnum> Credentials::Server(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data) { return Server(data, sChannelFlags, password); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Server(std::span<const std::byte> certBuffer,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return FromCertificate(certBuffer, sChannelFlags, CredentialFlags::Inbound, password, true);
    }
#pragma endregion


#pragma region Both Credentials

    std::expected<Credentials, CredentialErrorEnum> Credentials::Both(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data){ return Both(data, sChannelFlags, password); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Both(std::span<const std::byte> certBuffer,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return FromCertificate(certBuffer, sChannelFlags, CredentialFlags::Both, password);
    }

#pragma endregion


    CredHandle Credentials::GetCredHandle() const noexcept { return _impl->credHandle; }
    TimeStamp  Credentials::GetTsExpiry()  const noexcept { return _impl->tsExpiry;  }

    bool Credentials::IsExpired() const noexcept {
        ULARGE_INTEGER now, exp;

        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);

        now.LowPart = ftNow.dwLowDateTime;
        now.HighPart = ftNow.dwHighDateTime;

        exp.LowPart = _impl->tsExpiry.LowPart;
        exp.HighPart = _impl->tsExpiry.HighPart;

        return now.QuadPart >= exp.QuadPart;
    }

}

#else

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>
#include <openssl/bio.h>
#include <openssl/x509.h>


// Convert a (possibly UTF-32 on Linux) wchar_t string into UTF-8 so it can be
// fed to PKCS12_parse, which expects a NUL-terminated UTF-8 password.
static std::string S_WideToUtf8(const wchar_t* wide) noexcept {
    std::string out;
    if (!wide) return out;
    for (; *wide; ++wide) {
        const auto cp{ static_cast<uint32_t>(*wide) };
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}


// Drain the OpenSSL error queue and map the first error to a CredentialErrorEnum.
static Hermes::CredentialErrorEnum S_MapOpenSSLError() noexcept {
    using Hermes::CredentialErrorEnum;

    auto result{ CredentialErrorEnum::Unknown };
    while (const unsigned long err{ ERR_get_error() }) {
        if (result != CredentialErrorEnum::Unknown) continue; // keep draining
        switch (ERR_GET_REASON(err)) {
            case PKCS12_R_MAC_VERIFY_FAILURE:
            case PKCS12_R_PARSE_ERROR:
                result = CredentialErrorEnum::InvalidPassword;
                break;
            case PKCS12_R_PKCS12_CIPHERFINAL_ERROR:
            case ASN1_R_HEADER_TOO_LONG:
            case ERR_R_NESTED_ASN1_ERROR:
                result = CredentialErrorEnum::BadCertificateFormat;
                break;
            default:
                result = CredentialErrorEnum::Unknown;
        }
    }
    return result;
}


namespace {
    // Bag of OpenSSL objects extracted from a PKCS#12 blob; ownership is moved into
    // the SSL_CTX or freed by the Impl destructor when present.
    struct CertificateInfo {
        X509*           cert;     // leaf certificate
        EVP_PKEY*       pkey;     // private key (may be nullptr if cert-only PFX)
        STACK_OF(X509)* caChain;  // additional CA certificates (may be nullptr)
    };
}

static std::expected<CertificateInfo, Hermes::CredentialErrorEnum> ImportCertificate(
    const std::span<const std::byte> certBuffer, const wchar_t* password) noexcept {
    using Hermes::CredentialErrorEnum;

    BIO* bio{ BIO_new_mem_buf(certBuffer.data(), static_cast<int>(certBuffer.size())) };
    if (!bio) return std::unexpected{ CredentialErrorEnum::InsufficientMemory };

    PKCS12* p12{ d2i_PKCS12_bio(bio, nullptr) };
    BIO_free(bio);
    if (!p12) {
        ERR_clear_error();
        return std::unexpected{ CredentialErrorEnum::BadCertificateFormat };
    }

    const std::string utf8Pass{ S_WideToUtf8(password) };

    EVP_PKEY*       pkey{ nullptr };
    X509*           cert{ nullptr };
    STACK_OF(X509)* ca  { nullptr };

    const int ok{ PKCS12_parse(p12, utf8Pass.c_str(), &pkey, &cert, &ca) };
    PKCS12_free(p12);

    if (!ok) return std::unexpected{ S_MapOpenSSLError() };

    if (!cert) {
        if (pkey) EVP_PKEY_free(pkey);
        if (ca)   sk_X509_pop_free(ca, X509_free);
        return std::unexpected{ CredentialErrorEnum::CertificateNotFound };
    }

    return CertificateInfo{ cert, pkey, ca };
}


namespace Hermes {

    struct Credentials::Impl {
        SSL_CTX*                sslCtx{ nullptr };
        SupportedProtocolsFlags protocolFlags{};
        CredentialFlags         credentialFlags{};
        bool                    hasPrivateKey{};

        // Kept alive for IsExpired() (via X509_get0_notAfter) and chain inspection.
        X509*           certificate{ nullptr };
        EVP_PKEY*       privateKey{ nullptr };
        STACK_OF(X509)* caChain{ nullptr };

        ~Impl() noexcept {
            if (sslCtx)      SSL_CTX_free(sslCtx);
            if (certificate) X509_free(certificate);
            if (privateKey)  EVP_PKEY_free(privateKey);
            if (caChain)     sk_X509_pop_free(caChain, X509_free);
        }
    };


    // Translate the SChannel-shaped flag set into OpenSSL verify mode + trust store loading.
    static CredentialErrorEnum S_ApplyContextFlags(SSL_CTX* ctx, SChannelCredFlags flags, bool isServer) noexcept {
        const auto has{ [flags](SChannelCredFlags f) noexcept {
            return (static_cast<unsigned long>(flags) & static_cast<unsigned long>(f)) != 0;
        } };

        // TLS 1.2 minimum: matches OpenSSL 3.x default policy.
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

        int mode{ SSL_VERIFY_NONE };
        if (has(SChannelCredFlags::ManualCredValidation)) {
            mode = SSL_VERIFY_NONE;
        } else if (has(SChannelCredFlags::AutoCredValidation) || isServer == false) {
            mode = SSL_VERIFY_PEER;
        }
        SSL_CTX_set_verify(ctx, mode, nullptr);

        if (!has(SChannelCredFlags::NoDefaultCreds))
            SSL_CTX_set_default_verify_paths(ctx);

        if (has(SChannelCredFlags::DisableReconnects))
            SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

        if (X509_VERIFY_PARAM* param{ SSL_CTX_get0_param(ctx) }) {
            unsigned long crlFlags{ 0 };
            if (has(SChannelCredFlags::RevocationCheckEndCert)) crlFlags |= X509_V_FLAG_CRL_CHECK;
            if (has(SChannelCredFlags::RevocationCheckChain))   crlFlags |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;
            if (crlFlags) X509_VERIFY_PARAM_set_flags(param, crlFlags);
        }

        return CredentialErrorEnum::None;
    }


#pragma region FromCertificate

    std::expected<Credentials, CredentialErrorEnum> Credentials::FromCertificate(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, CredentialFlags credFlags, const wchar_t *password, bool useSelfStore) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data){ return FromCertificate(data, sChannelFlags, credFlags, password, useSelfStore); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::FromCertificate(std::span<const std::byte> certBuffer,
                                                                                 SChannelCredFlags sChannelFlags, CredentialFlags credFlags, const wchar_t *password, bool useSelfStore) noexcept {
        const auto s_makeCreds = [=](CertificateInfo certInfo) -> std::expected<Credentials, CredentialErrorEnum> {
            Credentials creds{};
            creds._impl->credentialFlags = credFlags;
            creds._impl->certificate     = certInfo.cert;
            creds._impl->privateKey      = certInfo.pkey;
            creds._impl->caChain         = certInfo.caChain;
            creds._impl->hasPrivateKey   = certInfo.pkey != nullptr;

            const SSL_METHOD* method{
                credFlags == CredentialFlags::Inbound  ? TLS_server_method() :
                credFlags == CredentialFlags::Outbound ? TLS_client_method() :
                                                         TLS_method()
            };

            creds._impl->sslCtx = SSL_CTX_new(method);
            if (!creds._impl->sslCtx) return std::unexpected{ CredentialErrorEnum::SecurityPackageNotFound };

            S_ApplyContextFlags(creds._impl->sslCtx, sChannelFlags, credFlags != CredentialFlags::Outbound);

            if (SSL_CTX_use_certificate(creds._impl->sslCtx, certInfo.cert) != 1)
                return std::unexpected{ CredentialErrorEnum::UnknownCredentials };

            if (certInfo.pkey) {
                if (SSL_CTX_use_PrivateKey(creds._impl->sslCtx, certInfo.pkey) != 1
                    || SSL_CTX_check_private_key(creds._impl->sslCtx) != 1)
                    return std::unexpected{ CredentialErrorEnum::UnknownCredentials };
            } else if (credFlags != CredentialFlags::Outbound) {
                return std::unexpected{ CredentialErrorEnum::MissingPrivateKey };
            }

            if (certInfo.caChain) {
                for (int i{ 0 }; i < sk_X509_num(certInfo.caChain); ++i) {
                    X509* ca{ sk_X509_value(certInfo.caChain, i) };
                    if (useSelfStore) {
                        // Treat the bundled CAs as the trust anchor for incoming peers.
                        if (X509_STORE* store{ SSL_CTX_get_cert_store(creds._impl->sslCtx) })
                            X509_STORE_add_cert(store, ca);
                    }
                    if (X509_up_ref(ca) == 1)
                        SSL_CTX_add_extra_chain_cert(creds._impl->sslCtx, ca);
                }
            }

            return creds;
        };

        return ImportCertificate(certBuffer, password)
                .and_then(s_makeCreds);
    }

#pragma endregion


#pragma region Client Credentials

    std::expected<Credentials, CredentialErrorEnum> Credentials::Client(SChannelCredFlags sChannelFlags) noexcept {
        Credentials creds{};
        creds._impl->credentialFlags = CredentialFlags::Outbound;

        creds._impl->sslCtx = SSL_CTX_new(TLS_client_method());
        if (!creds._impl->sslCtx) return std::unexpected{ CredentialErrorEnum::SecurityPackageNotFound };

        S_ApplyContextFlags(creds._impl->sslCtx, sChannelFlags, /*isServer*/ false);
        return creds;
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Client(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data){ return Client(data, sChannelFlags, password); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Client(const std::span<const std::byte> certBuffer,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return FromCertificate(certBuffer, sChannelFlags, CredentialFlags::Outbound, password);
    }

#pragma endregion


#pragma region Server Credentials

    std::expected<Credentials, CredentialErrorEnum> Credentials::Server(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data) { return Server(data, sChannelFlags, password); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Server(std::span<const std::byte> certBuffer,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return FromCertificate(certBuffer, sChannelFlags, CredentialFlags::Inbound, password, true);
    }

#pragma endregion


#pragma region Both Credentials

    std::expected<Credentials, CredentialErrorEnum> Credentials::Both(const std::filesystem::path &certPath,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return S_ReadCertFromFile(certPath)
                .and_then([=](std::vector<std::byte> data){ return Both(data, sChannelFlags, password); });
    }

    std::expected<Credentials, CredentialErrorEnum> Credentials::Both(std::span<const std::byte> certBuffer,
        SChannelCredFlags sChannelFlags, const wchar_t *password) noexcept {
        return FromCertificate(certBuffer, sChannelFlags, CredentialFlags::Both, password);
    }

#pragma endregion


    void* Credentials::GetNativeHandle() const noexcept { return _impl->sslCtx; }

    bool Credentials::IsExpired() const noexcept {
        if (!_impl->certificate) return false;
        const ASN1_TIME* notAfter{ X509_get0_notAfter(_impl->certificate) };
        if (!notAfter) return false;
        return X509_cmp_current_time(notAfter) < 0;
    }

}

#endif


namespace Hermes {

    Credentials::Credentials() noexcept : _impl{ std::make_unique<Impl>() } {}

    Credentials::Credentials(Credentials&& other) noexcept = default;
    Credentials& Credentials::operator=(Credentials&& other) noexcept = default;
    Credentials::~Credentials() noexcept = default;

    SupportedProtocolsFlags Credentials::GetProtocolFlags()   const noexcept { return _impl->protocolFlags;   }
    CredentialFlags         Credentials::GetCredentialFlags() const noexcept { return _impl->credentialFlags; }
    bool                    Credentials::HasPrivateKey()      const noexcept { return _impl->hasPrivateKey;   }

}
