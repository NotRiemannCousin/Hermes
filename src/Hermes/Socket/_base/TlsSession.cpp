// ReSharper disable CppMemberFunctionMayBeStatic
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_TLS

#include <Hermes/Socket/_base/TlsSession.hpp>
#include <Hermes/_base/Credentials.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace Hermes::details_ {

#ifdef _WIN32
// ============================================================================
//                                   SCHANNEL
// ============================================================================

    struct TlsSession::Impl {
        CredHandle                m_credHandle{};
        CtxtHandle                m_ctxtHandle{};
        TimeStamp                 m_tsExpiry{};
        SecPkgContext_StreamSizes m_streamSizes{};

        std::string m_host{};
        bool        m_isServer{};
        bool        m_requestClientCert{};
        bool        m_ignoreCertErrors{};
        bool        m_mutualAuth{};
        bool        m_handshakeComplete{};

#pragma region buffers

        std::array<std::array<std::byte, 0x4000>, 4> m_buffers{};
        std::array<SecBuffer, 4> m_secBuffers{};


        SecBuffer& TokenBuffer() noexcept { return m_secBuffers[0]; }
        SecBuffer& ExtraBuffer() noexcept { return m_secBuffers[1]; }

        SecBuffer& OutBuffer() noexcept { return m_secBuffers[2]; }
        SecBuffer& MsgBuffer() noexcept { return m_secBuffers[3]; }


        SecBufferDesc m_inBufferDesc { .ulVersion = macroSECBUFFER_VERSION, .cBuffers = 2, .pBuffers = &TokenBuffer() };
        SecBufferDesc m_outBufferDesc{ .ulVersion = macroSECBUFFER_VERSION, .cBuffers = 2, .pBuffers = &OutBuffer()   };

#pragma endregion

        ~Impl() noexcept {
            if (m_ctxtHandle.dwLower != 0 || m_ctxtHandle.dwUpper != 0)
                ::DeleteSecurityContext(&m_ctxtHandle);
        }
    };


    void TlsSession::BeginClient(const Credentials& creds, std::string host,
                                 bool ignoreCertErrors, bool mutualAuth) const noexcept {
        m_impl->m_credHandle        = creds.GetCredHandle();
        m_impl->m_host              = std::move(host);
        m_impl->m_isServer          = false;
        m_impl->m_ignoreCertErrors  = ignoreCertErrors;
        m_impl->m_mutualAuth        = mutualAuth;
        m_impl->m_handshakeComplete = false;
    }

    void TlsSession::BeginServer(const Credentials& creds, bool requestClientCert) const noexcept {
        m_impl->m_credHandle        = creds.GetCredHandle();
        m_impl->m_isServer          = true;
        m_impl->m_requestClientCert = requestClientCert;
        m_impl->m_handshakeComplete = false;
    }


    bool TlsSession::IsServer()            const noexcept { return m_impl->m_isServer; }
    bool TlsSession::IsActive()            const noexcept { return m_impl->m_credHandle.dwLower != 0 || m_impl->m_credHandle.dwUpper != 0; }
    bool TlsSession::IsHandshakeComplete() const noexcept { return m_impl->m_handshakeComplete; }
    bool TlsSession::IsRenegotiation()     const noexcept { return m_impl->m_ctxtHandle.dwLower != 0 || m_impl->m_ctxtHandle.dwUpper != 0; }


    TlsSession::HandshakeOutcome TlsSession::AdvanceHandshake(
        std::span<std::byte> inBytes, std::span<std::byte> outBuf
    ) const noexcept {
        const bool firstPass{ !IsRenegotiation() };

        m_impl->TokenBuffer() = SecBuffer{ tul(inBytes.size()), tul(SecurityBufferEnum::Token), inBytes.data() };
        m_impl->ExtraBuffer() = SecBuffer{ 0                  , tul(SecurityBufferEnum::Empty), nullptr        };

        m_impl->OutBuffer() = SecBuffer{ tul(outBuf.size())            , tul(SecurityBufferEnum::Token), outBuf.data()           };
        m_impl->MsgBuffer() = SecBuffer{ tul(m_impl->m_buffers[3].size()), tul(SecurityBufferEnum::Alert), m_impl->m_buffers[3].data() };

        SECURITY_STATUS status{};

        if (m_impl->m_isServer) {
            auto m_dwSspiFlags{ AcceptSecurityContextFlags::SequenceDetect  |
                               AcceptSecurityContextFlags::ReplayDetect    |
                               AcceptSecurityContextFlags::Confidentiality |
                               AcceptSecurityContextFlags::ExtendedError   |
                               AcceptSecurityContextFlags::Stream };
            if (m_impl->m_requestClientCert)
                m_dwSspiFlags |= AcceptSecurityContextFlags::MutualAuth;

            DWORD m_pfContextAttr{};
            status = ::AcceptSecurityContext(
                &m_impl->m_credHandle,
                firstPass ? nullptr : &m_impl->m_ctxtHandle,
                &m_impl->m_inBufferDesc,
                tul(m_dwSspiFlags), 0,
                &m_impl->m_ctxtHandle,
                &m_impl->m_outBufferDesc, &m_pfContextAttr, &m_impl->m_tsExpiry
            );
        } else {
            auto m_dwSspiFlags{ InitializeSecurityContextFlags::SequenceDetect  |
                               InitializeSecurityContextFlags::ReplayDetect    |
                               InitializeSecurityContextFlags::Confidentiality |
                               InitializeSecurityContextFlags::ExtendedError   |
                               InitializeSecurityContextFlags::Stream };
            if (m_impl->m_mutualAuth)       m_dwSspiFlags |= InitializeSecurityContextFlags::MutualAuth;
            if (m_impl->m_ignoreCertErrors) m_dwSspiFlags |= InitializeSecurityContextFlags::ManualCredValidation;

            DWORD m_pfContextAttr{};
            status = ::InitializeSecurityContextA(
                &m_impl->m_credHandle,
                firstPass ? nullptr : &m_impl->m_ctxtHandle,
                const_cast<SEC_CHAR*>(m_impl->m_host.c_str()),
                tll(m_dwSspiFlags), 0, 0,
                firstPass ? nullptr : &m_impl->m_inBufferDesc, 0,
                &m_impl->m_ctxtHandle,
                &m_impl->m_outBufferDesc, &m_pfContextAttr, &m_impl->m_tsExpiry
            );
        }

        std::uint32_t consumed{ tul(inBytes.size()) };
        if (m_impl->ExtraBuffer().cbBuffer > 0 && m_impl->ExtraBuffer().BufferType == tul(SecurityBufferEnum::Extra))
            consumed -= m_impl->ExtraBuffer().cbBuffer;

        const auto translated{ static_cast<EncryptStatusEnum>(status) };
        if (translated == EncryptStatusEnum::ErrOk)
            m_impl->m_handshakeComplete = true;

        return { translated, consumed, m_impl->OutBuffer().cbBuffer };
    }


    TlsSession::StreamSizes TlsSession::GetStreamSizes() const noexcept {
        if (m_impl->m_streamSizes.cbMaximumMessage == 0) {
            ::QueryContextAttributesA(
                const_cast<CtxtHandle*>(&m_impl->m_ctxtHandle),
                macroSECPKG_ATTR_STREAM_SIZES,
                &m_impl->m_streamSizes
            );
        }
        return { m_impl->m_streamSizes.cbHeader, m_impl->m_streamSizes.cbTrailer, m_impl->m_streamSizes.cbMaximumMessage };
    }


    TlsSession::EncryptOutcome TlsSession::Encrypt(
        std::span<const std::byte> plain, std::span<std::byte> outBuf
    ) const noexcept {
        const auto& sizes{ m_impl->m_streamSizes };

        m_impl->m_secBuffers[0] = SecBuffer{ sizes.cbHeader      , tul(SecurityBufferEnum::StreamHeader) , outBuf.data() };
        m_impl->m_secBuffers[1] = SecBuffer{ tul(plain.size()), tul(SecurityBufferEnum::Data)         , outBuf.data() + sizes.cbHeader };
        m_impl->m_secBuffers[2] = SecBuffer{ sizes.cbTrailer     , tul(SecurityBufferEnum::StreamTrailer), outBuf.data() + sizes.cbHeader + plain.size() };
        m_impl->m_secBuffers[3] = SecBuffer{ 0                   , tul(SecurityBufferEnum::Empty)        , nullptr };

        std::memcpy(m_impl->m_secBuffers[1].pvBuffer, plain.data(), plain.size());

        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, m_impl->m_secBuffers.data() };
        const SECURITY_STATUS status{ ::EncryptMessage(&m_impl->m_ctxtHandle, 0, &buffDesc, 0) };

        if (status != tul(EncryptStatusEnum::ErrOk))
            return { static_cast<EncryptStatusEnum>(status), 0 };

        const std::uint32_t produced{ m_impl->m_secBuffers[0].cbBuffer + m_impl->m_secBuffers[1].cbBuffer + m_impl->m_secBuffers[2].cbBuffer };
        return { EncryptStatusEnum::ErrOk, produced };
    }


    TlsSession::DecryptOutcome TlsSession::Decrypt(std::span<std::byte> inBytes) const noexcept {
        std::span buffs{ m_impl->m_secBuffers };
        buffs[0] = SecBuffer{ tul(inBytes.size()), tul(SecurityBufferEnum::Data), inBytes.data() };
        buffs[1] = buffs[2] = buffs[3] = SecBuffer{ 0, tul(SecurityBufferEnum::Empty), nullptr };

        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, buffs.data() };
        const SECURITY_STATUS status{ ::DecryptMessage(&m_impl->m_ctxtHandle, &buffDesc, 0, nullptr) };

        DecryptOutcome out{ static_cast<EncryptStatusEnum>(status), {}, inBytes };

        const auto dataBuffer { std::ranges::find(buffs, tul(SecurityBufferEnum::Data),  &SecBuffer::BufferType) };
        const auto extraBuffer{ std::ranges::find(buffs, tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };

        if (dataBuffer != buffs.end() && dataBuffer->cbBuffer > 0)
            out.data = { static_cast<std::byte*>(dataBuffer->pvBuffer), dataBuffer->cbBuffer }, out.extra = {};
        if (extraBuffer != buffs.end() && extraBuffer->cbBuffer > 0)
            out.extra = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };

        return out;
    }


    std::uint32_t TlsSession::Shutdown(std::span<std::byte> outBuf) const noexcept {
        DWORD dwType{ macroSCHANNEL_SHUTDOWN };

        m_impl->OutBuffer() = SecBuffer{ sizeof(dwType), tul(SecurityBufferEnum::Token), &dwType };
        SecBufferDesc outBufferDesc{ macroSECBUFFER_VERSION, 1, &m_impl->OutBuffer() };

        if (::ApplyControlToken(&m_impl->m_ctxtHandle, &outBufferDesc) != tul(EncryptStatusEnum::ErrOk))
            return 0;

        m_impl->OutBuffer() = SecBuffer{ tul(outBuf.size()), tul(SecurityBufferEnum::Token), outBuf.data() };
        outBufferDesc      = SecBufferDesc{ macroSECBUFFER_VERSION, 1, &m_impl->OutBuffer() };

        DWORD dwSSPIOutFlags{};
        TimeStamp tsExpiry{};
        SECURITY_STATUS status{};

        if (m_impl->m_isServer) {
            auto dwSspiFlags{ AcceptSecurityContextFlags::SequenceDetect  |
                              AcceptSecurityContextFlags::ReplayDetect    |
                              AcceptSecurityContextFlags::Confidentiality |
                              AcceptSecurityContextFlags::Stream };
            if (m_impl->m_requestClientCert)
                dwSspiFlags |= AcceptSecurityContextFlags::MutualAuth;

            status = ::AcceptSecurityContext(
                &m_impl->m_credHandle, &m_impl->m_ctxtHandle, nullptr,
                tul(dwSspiFlags), 0, nullptr, &outBufferDesc, &dwSSPIOutFlags, &tsExpiry
            );
        } else {
            constexpr auto dwSspiFlags{
                InitializeSecurityContextFlags::SequenceDetect  |
                InitializeSecurityContextFlags::ReplayDetect    |
                InitializeSecurityContextFlags::Confidentiality |
                InitializeSecurityContextFlags::Stream
            };
            status = ::InitializeSecurityContextA(
                nullptr, &m_impl->m_ctxtHandle, nullptr,
                tl(dwSspiFlags), 0, 0, nullptr, 0,
                nullptr, &outBufferDesc, &dwSSPIOutFlags, &tsExpiry
            );
        }

        if (status == tul(EncryptStatusEnum::ErrOk))
            return m_impl->OutBuffer().cbBuffer;
        return 0;
    }


    void TlsSession::DeleteContext() const noexcept {
        if (m_impl->m_ctxtHandle.dwLower != 0 || m_impl->m_ctxtHandle.dwUpper != 0) {
            ::DeleteSecurityContext(&m_impl->m_ctxtHandle);
            m_impl->m_ctxtHandle = {};
        }
        m_impl->m_handshakeComplete = false;
        m_impl->m_streamSizes       = {};
    }


#else
// ============================================================================
//                                  OPENSSL
// ============================================================================

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>


    struct TlsSession::Impl {
        SSL_CTX* m_ctx{ nullptr };

        SSL* m_ssl { nullptr };
        BIO* m_rBio{ nullptr };
        BIO* m_wBio{ nullptr };

        std::string m_host{};
        bool        m_isServer{};
        bool        m_requestClientCert{};
        bool        m_ignoreCertErrors{};
        bool        m_handshakeComplete{};

        std::vector<std::byte> m_decryptScratch{};

        ~Impl() noexcept {
            if (m_ssl) SSL_free(m_ssl);
        }
    };


    static EncryptStatusEnum CertVerifyToStatus(const long verifyResult) noexcept {
        switch (verifyResult) {
            case X509_V_OK                                   : return EncryptStatusEnum::ErrOk;
            case X509_V_ERR_CERT_HAS_EXPIRED                 : return EncryptStatusEnum::ErrCertExpired;
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT      :
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN        :
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT        :
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY: return EncryptStatusEnum::ErrUntrustedRoot;
            case X509_V_ERR_UNABLE_TO_GET_CRL                :
            case X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER         : return EncryptStatusEnum::ErrNoAuthenticatingAuthority;
            default                                          : return EncryptStatusEnum::ErrCertUnknown;
        }
    }

    static EncryptStatusEnum TranslateSslError(const SSL* ssl, const int ret, const bool produced) noexcept {
        if (ret > 0)
            return EncryptStatusEnum::ErrOk;

        switch (SSL_get_error(ssl, ret)) {
            case SSL_ERROR_WANT_READ:
                return produced ? EncryptStatusEnum::InfoContinueNeeded
                                : EncryptStatusEnum::ErrIncompleteMessage;
            case SSL_ERROR_WANT_WRITE : return EncryptStatusEnum::InfoContinueNeeded;
            case SSL_ERROR_ZERO_RETURN: return EncryptStatusEnum::InfoContextExpired;
            case SSL_ERROR_SSL: {
                const long verify{ SSL_get_verify_result(ssl) };
                if (verify != X509_V_OK) return CertVerifyToStatus(verify);
                return EncryptStatusEnum::ErrInvalidToken;
            }
            case SSL_ERROR_SYSCALL:
            default                : return EncryptStatusEnum::ErrInvalidToken;
        }
    }


    void TlsSession::BeginClient(const Credentials& creds, std::string host,
                                 bool ignoreCertErrors, bool mutualAuth) const noexcept {
        m_impl->m_ctx               = static_cast<SSL_CTX*>(creds.GetNativeHandle());
        m_impl->m_host              = std::move(host);
        m_impl->m_isServer          = false;
        m_impl->m_ignoreCertErrors  = ignoreCertErrors;
        m_impl->m_handshakeComplete = false;

        if (m_impl->m_ssl) { SSL_free(m_impl->m_ssl); m_impl->m_ssl = nullptr; }

        m_impl->m_ssl  = SSL_new(m_impl->m_ctx);
        m_impl->m_rBio = BIO_new(BIO_s_mem());
        m_impl->m_wBio = BIO_new(BIO_s_mem());
        SSL_set_bio(m_impl->m_ssl, m_impl->m_rBio, m_impl->m_wBio);

        SSL_set_connect_state(m_impl->m_ssl);
        SSL_set_tlsext_host_name(m_impl->m_ssl, m_impl->m_host.c_str());

        if (ignoreCertErrors) {
            SSL_set_verify(m_impl->m_ssl, SSL_VERIFY_NONE, nullptr);
        } else if (X509_VERIFY_PARAM* p{ SSL_get0_param(m_impl->m_ssl) }) {
            X509_VERIFY_PARAM_set_hostflags(p, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            X509_VERIFY_PARAM_set1_host(p, m_impl->m_host.c_str(), m_impl->m_host.size());
        }

        (void)mutualAuth;
    }

    void TlsSession::BeginServer(const Credentials& creds, bool requestClientCert) const noexcept {
        m_impl->m_ctx               = static_cast<SSL_CTX*>(creds.GetNativeHandle());
        m_impl->m_isServer          = true;
        m_impl->m_requestClientCert = requestClientCert;
        m_impl->m_handshakeComplete = false;

        if (m_impl->m_ssl) { SSL_free(m_impl->m_ssl); m_impl->m_ssl = nullptr; }

        m_impl->m_ssl  = SSL_new(m_impl->m_ctx);
        m_impl->m_rBio = BIO_new(BIO_s_mem());
        m_impl->m_wBio = BIO_new(BIO_s_mem());
        SSL_set_bio(m_impl->m_ssl, m_impl->m_rBio, m_impl->m_wBio);

        SSL_set_accept_state(m_impl->m_ssl);

        if (requestClientCert)
            SSL_set_verify(m_impl->m_ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    }


    bool TlsSession::IsServer()            const noexcept { return m_impl->m_isServer; }
    bool TlsSession::IsActive()            const noexcept { return m_impl->m_ssl != nullptr; }
    bool TlsSession::IsHandshakeComplete() const noexcept { return m_impl->m_handshakeComplete; }
    bool TlsSession::IsRenegotiation()     const noexcept { return m_impl->m_ssl != nullptr && m_impl->m_handshakeComplete; }


    TlsSession::HandshakeOutcome TlsSession::AdvanceHandshake(
        std::span<std::byte> inBytes, std::span<std::byte> outBuf
    ) const noexcept {
        if (!m_impl->m_ssl) return { EncryptStatusEnum::ErrInvalidHandle, 0, 0 };

        std::uint32_t consumed{ 0 };
        if (!inBytes.empty()) {
            const int written{ BIO_write(m_impl->m_rBio, inBytes.data(), static_cast<int>(inBytes.size())) };
            if (written > 0) consumed = static_cast<std::uint32_t>(written);
        }

        ERR_clear_error();
        const int ret{ SSL_do_handshake(m_impl->m_ssl) };

        std::uint32_t produced{ 0 };
        const int pending{ BIO_pending(m_impl->m_wBio) };
        if (pending > 0 && !outBuf.empty()) {
            const int n{ BIO_read(m_impl->m_wBio, outBuf.data(),
                                   static_cast<int>(std::min<size_t>(outBuf.size(), static_cast<size_t>(pending)))) };
            if (n > 0) produced = static_cast<std::uint32_t>(n);
        }

        const auto status{ TranslateSslError(m_impl->m_ssl, ret, produced > 0) };
        if (ret == 1) m_impl->m_handshakeComplete = true;

        return { status, consumed, produced };
    }


    TlsSession::StreamSizes TlsSession::GetStreamSizes() const noexcept {
        return { 64u, 64u, 16384u };
    }


    TlsSession::EncryptOutcome TlsSession::Encrypt(
        std::span<const std::byte> plain, std::span<std::byte> outBuf
    ) const noexcept {
        if (!m_impl->m_ssl) return { EncryptStatusEnum::ErrInvalidHandle, 0 };

        ERR_clear_error();
        const int written{ SSL_write(m_impl->m_ssl, plain.data(), static_cast<int>(plain.size())) };
        if (written <= 0) {
            const int err{ SSL_get_error(m_impl->m_ssl, written) };
            if (err == SSL_ERROR_ZERO_RETURN)
                return { EncryptStatusEnum::InfoContextExpired, 0 };
            return { EncryptStatusEnum::ErrEncryptFailure, 0 };
        }

        const int pending{ BIO_pending(m_impl->m_wBio) };
        if (pending <= 0)
            return { EncryptStatusEnum::ErrOk, 0 };

        const int n{ BIO_read(m_impl->m_wBio, outBuf.data(),
                               static_cast<int>(std::min<size_t>(outBuf.size(), static_cast<size_t>(pending)))) };
        if (n <= 0) return { EncryptStatusEnum::ErrEncryptFailure, 0 };

        return { EncryptStatusEnum::ErrOk, static_cast<std::uint32_t>(n) };
    }


    TlsSession::DecryptOutcome TlsSession::Decrypt(std::span<std::byte> inBytes) const noexcept {
        if (!m_impl->m_ssl) return { EncryptStatusEnum::ErrInvalidHandle, {}, {} };

        if (!inBytes.empty())
            BIO_write(m_impl->m_rBio, inBytes.data(), static_cast<int>(inBytes.size()));

        constexpr size_t k_maxPlaintext{ 0x10000 };
        m_impl->m_decryptScratch.resize(k_maxPlaintext);

        size_t totalRead{};
        ERR_clear_error();

        while (totalRead < k_maxPlaintext) {
            const int n{ SSL_read(m_impl->m_ssl, m_impl->m_decryptScratch.data() + totalRead,
                                  static_cast<int>(k_maxPlaintext - totalRead)) };
            if (n > 0) {
                totalRead += static_cast<size_t>(n);
            } else {
                const int err{ SSL_get_error(m_impl->m_ssl, n) };
                if (totalRead > 0 && err == SSL_ERROR_WANT_READ) {
                    break;
                }
                if (totalRead == 0) {
                    switch (err) {
                        case SSL_ERROR_WANT_READ  : return { EncryptStatusEnum::ErrIncompleteMessage, {}, {} };
                        case SSL_ERROR_ZERO_RETURN: return { EncryptStatusEnum::InfoContextExpired  , {}, {} };
                        default                   : return { EncryptStatusEnum::ErrDecryptFailure   , {}, {} };
                    }
                }
                break;
            }
        }

        if (totalRead > 0) {
            std::memcpy(inBytes.data(), m_impl->m_decryptScratch.data(), totalRead);
            return { EncryptStatusEnum::ErrOk,
                     std::span{ inBytes.data(), totalRead },
                     {} };
        }

        return { EncryptStatusEnum::ErrDecryptFailure, {}, {} };
    }


    std::uint32_t TlsSession::Shutdown(std::span<std::byte> outBuf) const noexcept {
        if (!m_impl->m_ssl) return 0;

        ERR_clear_error();
        SSL_shutdown(m_impl->m_ssl);

        const int pending{ BIO_pending(m_impl->m_wBio) };
        if (pending <= 0 || outBuf.empty()) return 0;

        const int n{ BIO_read(m_impl->m_wBio, outBuf.data(),
                              static_cast<int>(std::min<size_t>(outBuf.size(), static_cast<size_t>(pending)))) };
        return n > 0 ? static_cast<std::uint32_t>(n) : 0;
    }


    void TlsSession::DeleteContext() const noexcept {
        if (m_impl->m_ssl) {
            SSL_free(m_impl->m_ssl);
            m_impl->m_ssl  = nullptr;
            m_impl->m_rBio = nullptr;
            m_impl->m_wBio = nullptr;
        }
        m_impl->m_handshakeComplete = false;
    }


#endif


    TlsSession::TlsSession() : m_impl(std::make_unique<Impl>()) {}
    TlsSession::~TlsSession() noexcept = default;

    TlsSession::TlsSession(TlsSession&& other) noexcept = default;
    TlsSession& TlsSession::operator=(TlsSession&& other) noexcept = default;

    TlsSession TlsSession::MakeChild() const {
        TlsSession session;
        session.m_impl->m_isServer = m_impl->m_isServer;
        return session;
    }
}

#endif