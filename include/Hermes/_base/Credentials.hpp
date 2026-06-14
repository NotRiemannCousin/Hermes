#pragma once
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <expected>
#include <filesystem>
#include <span>
#include <cstddef>
#include <memory>

namespace Hermes {
    enum class CredentialErrorEnum : unsigned long {
        None = 0,

        FileReadFailed,

        // SSL
        InvalidPassword,          // ERROR_INVALID_PASSWORD
        BadCertificateFormat,     // CRYPT_E_BAD_MSG or ERROR_INVALID_DATA
        CertificateNotFound,      // CRYPT_E_NOT_FOUND
        MissingPrivateKey,        // valid but without CERT_KEY_PROV_INFO_PROP_ID

        // SCHANNEL
        SecurityPackageNotFound,  // SEC_E_SECPKG_NOT_FOUND
        UnknownCredentials,       // SEC_E_UNKNOWN_CREDENTIALS
        NoCredentials,            // SEC_E_NO_CREDENTIALS
        InsufficientMemory,       // SEC_E_INSUFFICIENT_MEMORY
        InternalError,            // SEC_E_INTERNAL_ERROR
        UnsupportedFunction,      // SEC_E_UNSUPPORTED_FUNCTION

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

        SupportedProtocolsFlags GetProtocolFlags() const noexcept;
        CredentialFlags GetCredentialFlags() const noexcept;
        bool IsExpired() const noexcept;
        bool HasPrivateKey() const noexcept;

#ifdef _WIN32
        CredHandle GetCredHandle() const noexcept;
        TimeStamp GetTsExpiry() const noexcept;
#else
        // Returns the OpenSSL SSL_CTX* held by this credential (opaque).
        // The caller MUST NOT free or up-ref it — lifetime is tied to *this.
        void* GetNativeHandle() const noexcept;
#endif

    private:
        Credentials() noexcept;

        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
