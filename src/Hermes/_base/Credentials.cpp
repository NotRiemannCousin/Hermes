#include <Hermes/_base/Credentials.hpp>

#include <utility>
#include <fstream>
#include <functional>
#include <iostream>
#include <ranges>
#include <vector>

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
static Hermes::CredentialErrorEnum S_MapSecurityStatus(SECURITY_STATUS status) noexcept {
    switch (static_cast<EncryptStatusEnum>(status)) {
        case EncryptStatusEnum::ErrOk                 : return Hermes::CredentialErrorEnum::None;
        case EncryptStatusEnum::ErrSecpkgNotFound     : return Hermes::CredentialErrorEnum::SecurityPackageNotFound;
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

    Credentials::Credentials(Credentials&& other) noexcept 
        : _credHandle(std::exchange(other._credHandle, {})),
          _protocolFlags(std::exchange(other._protocolFlags, {})),
          _credentialFlags(std::exchange(other._credentialFlags, {})),
          _tsExpiry(std::exchange(other._tsExpiry, {})),
          _hasPrivateKey(std::exchange(other._hasPrivateKey, false)),
          _certStore(std::exchange(other._certStore, nullptr)),
          _certContext(std::exchange(other._certContext, nullptr)) {}

    Credentials& Credentials::operator=(Credentials&& other) noexcept {
        if (this != &other) {
            this->~Credentials();
            _credHandle = std::exchange(other._credHandle, {});
            _protocolFlags = std::exchange(other._protocolFlags, {});
            _credentialFlags = std::exchange(other._credentialFlags, {});
            _tsExpiry = std::exchange(other._tsExpiry, {});
            _hasPrivateKey = std::exchange(other._hasPrivateKey, false);
            _certStore = std::exchange(other._certStore, nullptr);
            _certContext = std::exchange(other._certContext, nullptr);
        }
        return *this;
    }

    Credentials::~Credentials() noexcept {
        if (_credHandle.dwLower != 0 || _credHandle.dwUpper != 0) {
            ::FreeCredentialsHandle(&_credHandle);
            _credHandle = {};
        }
        if (_certContext) {
            ::CertFreeCertificateContext(_certContext);
            _certContext = nullptr;
        }
        if (_certStore) {
            ::CertCloseStore(_certStore, 0);
            _certStore = nullptr;
        }
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
            creds._credentialFlags = credFlags;
            creds._certStore       = certInfo.certStore;
            creds._certContext     = certInfo.certContext;
            creds._hasPrivateKey   = certInfo.hasPrivateKey;

            SCH_CREDENTIALS cred{
                .dwVersion = static_cast<DWORD>(SChCredEnum::SchCredentials),
                .cCreds    = 1,
                .paCred    = &creds._certContext,

                .hRootStore     = useSelfStore ? creds._certStore : nullptr,
                .cTlsParameters = 0,
                .pTlsParameters = nullptr,
            };

            const SECURITY_STATUS status{ ::AcquireCredentialsHandleA(
                nullptr, const_cast<LPSTR>(macroUNISP_NAME.data()), static_cast<unsigned long>(creds._credentialFlags),
                nullptr, &cred, nullptr, nullptr, &creds._credHandle, &creds._tsExpiry
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
        creds._credentialFlags = CredentialFlags::Outbound;

        SCH_CREDENTIALS cred{
            .dwVersion = static_cast<DWORD>(SChCredEnum::SchCredentials),

            .hRootStore     = nullptr,
            .dwFlags        = static_cast<DWORD>(sChannelFlags),

            .cTlsParameters = 0,
            .pTlsParameters = nullptr,
        };


        const SECURITY_STATUS status{ ::AcquireCredentialsHandleA(
            nullptr, const_cast<LPSTR>(macroUNISP_NAME.data()), static_cast<unsigned long>(creds._credentialFlags),
            nullptr, &cred, nullptr, nullptr, &creds._credHandle, &creds._tsExpiry
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


    CredHandle Credentials::GetCredHandle() const noexcept { return _credHandle; }
    SupportedProtocolsFlags Credentials::GetProtocolFlags() const noexcept { return _protocolFlags; }
    CredentialFlags Credentials::GetCredentialFlags() const noexcept { return _credentialFlags; }
    TimeStamp Credentials::GetTsExpiry() const noexcept { return _tsExpiry; }

    bool Credentials::IsExpired() const noexcept {
        ULARGE_INTEGER now, exp;

        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);

        now.LowPart = ftNow.dwLowDateTime;
        now.HighPart = ftNow.dwHighDateTime;

        exp.LowPart = _tsExpiry.LowPart;
        exp.HighPart = _tsExpiry.HighPart;

        return now.QuadPart >= exp.QuadPart;
    }

    bool Credentials::HasPrivateKey() const noexcept { return _hasPrivateKey; }

}