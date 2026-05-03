#pragma once
#include <Hermes/_base/WinApi/WinApi.hpp>
#include <expected>
#include <filesystem>
#include <span>
#include <cstddef>

namespace Hermes {
    enum class CredentialErrorEnum : unsigned long {
        None = 0,

        FileReadFailed,

        InvalidPassword,          // ERROR_INVALID_PASSWORD
        BadCertificateFormat,     // CRYPT_E_BAD_MSG ou ERROR_INVALID_DATA
        CertificateNotFound,      // CRYPT_E_NOT_FOUND
        MissingPrivateKey,        // Retorno válido, mas cert sem propriedade CERT_KEY_PROV_INFO_PROP_ID

        // Falhas do Schannel (AcquireCredentialsHandle)
        SecurityPackageNotFound,  // SEC_E_SECPKG_NOT_FOUND (Schannel não carregado/inexistente)
        UnknownCredentials,       // SEC_E_UNKNOWN_CREDENTIALS (Geralmente falta de permissão na chave privada)
        NoCredentials,            // SEC_E_NO_CREDENTIALS (A credencial passada não tem privilégios/não serve)
        InsufficientMemory,       // SEC_E_INSUFFICIENT_MEMORY
        InternalError,            // SEC_E_INTERNAL_ERROR (Falha no LSA ou subsistema de crypto do Windows)
        UnsupportedFunction,      // SEC_E_UNSUPPORTED_FUNCTION (Ex: pedindo protocolo desativado no Registry)

        Unknown
    };

    struct Credentials {
        Credentials(const Credentials&) = delete;
        Credentials& operator=(const Credentials&) = delete;

        Credentials(Credentials&& other) noexcept;
        Credentials& operator=(Credentials&& other) noexcept;
        ~Credentials() noexcept;


        static std::expected<Credentials, CredentialErrorEnum> FromCertificate(const std::filesystem::path& certPath,
                SChannelCredFlags sChannelFlags, CredentialFlags credFlags, const wchar_t* password = L"", bool useSelfStore = false) noexcept;
        static std::expected<Credentials, CredentialErrorEnum> FromCertificate(std::span<const std::byte> certBuffer,
                SChannelCredFlags sChannelFlags, CredentialFlags credFlags, const wchar_t* password = L"", bool useSelfStore = false) noexcept;

        static std::expected<Credentials, CredentialErrorEnum> Client(SChannelCredFlags sChannelFlags =
                SChannelCredFlags::AutoCredValidation) noexcept;
        static std::expected<Credentials, CredentialErrorEnum> Client(const std::filesystem::path& certPath,
                SChannelCredFlags sChannelFlags = SChannelCredFlags::AutoCredValidation, const wchar_t* password = L"") noexcept;
        static std::expected<Credentials, CredentialErrorEnum> Client(std::span<const std::byte> certBuffer,
                SChannelCredFlags sChannelFlags = SChannelCredFlags::AutoCredValidation, const wchar_t* password = L"") noexcept;

        static std::expected<Credentials, CredentialErrorEnum> Server(const std::filesystem::path& certPath,
                SChannelCredFlags sChannelFlags = {}, const wchar_t* password = L"") noexcept;
        static std::expected<Credentials, CredentialErrorEnum> Server(std::span<const std::byte> certBuffer,
                SChannelCredFlags sChannelFlags = {}, const wchar_t* password = L"") noexcept;

        // TODO: FUTURE: static std::expected<Credentials, CredentialErrorEnum> Server(std::wstring_view certThumbprint) noexcept;

        static std::expected<Credentials, CredentialErrorEnum> Both(const std::filesystem::path& certPath,
                SChannelCredFlags sChannelFlags = {}, const wchar_t* password = L"") noexcept;
        static std::expected<Credentials, CredentialErrorEnum> Both(std::span<const std::byte> certBuffer,
                SChannelCredFlags sChannelFlags = {}, const wchar_t* password = L"") noexcept;

        CredHandle GetCredHandle() const noexcept;
        SupportedProtocolsFlags GetProtocolFlags() const noexcept;
        CredentialFlags GetCredentialFlags() const noexcept;
        TimeStamp GetTsExpiry() const noexcept;
        bool IsExpired() const noexcept;
        bool HasPrivateKey() const noexcept;

    private:
        Credentials() noexcept = default;

        CredHandle _credHandle{};
        SupportedProtocolsFlags _protocolFlags{};
        CredentialFlags _credentialFlags{};
        TimeStamp _tsExpiry{};
        bool _hasPrivateKey{};

        HCERTSTORE _certStore{ nullptr };
        PCCERT_CONTEXT _certContext{ nullptr };
    };
}