// ReSharper disable CppMemberFunctionMayBeStatic
#include <Hermes/Socket/_base/TlsSession.hpp>
#include <Hermes/_base/Credentials.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace Hermes::_details {

#ifdef _WIN32
// ============================================================================
//                                   SCHANNEL
// ============================================================================

    struct TlsSession::Impl {
        CredHandle                _credHandle{};
        CtxtHandle                _ctxtHandle{};
        TimeStamp                 _tsExpiry{};
        SecPkgContext_StreamSizes _streamSizes{};

        std::string _host{};
        bool        _isServer{};
        bool        _requestClientCert{};
        bool        _ignoreCertErrors{};
        bool        _mutualAuth{};
        bool        _handshakeComplete{};

#pragma region buffers

        std::array<std::array<std::byte, 0x4000>, 4> _buffers{};
        std::array<SecBuffer, 4> _secBuffers{};


        SecBuffer& TokenBuffer() noexcept { return _secBuffers[0]; }
        SecBuffer& ExtraBuffer() noexcept { return _secBuffers[1]; }

        SecBuffer& OutBuffer() noexcept { return _secBuffers[2]; }
        SecBuffer& MsgBuffer() noexcept { return _secBuffers[3]; }


        SecBufferDesc _inBufferDesc { .ulVersion = macroSECBUFFER_VERSION, .cBuffers = 2, .pBuffers = &TokenBuffer() };
        SecBufferDesc _outBufferDesc{ .ulVersion = macroSECBUFFER_VERSION, .cBuffers = 2, .pBuffers = &OutBuffer()   };

#pragma endregion

        ~Impl() noexcept {
            if (_ctxtHandle.dwLower != 0 || _ctxtHandle.dwUpper != 0)
                ::DeleteSecurityContext(&_ctxtHandle);
        }
    };


    void TlsSession::BeginClient(const Credentials& creds, std::string host,
                                 bool ignoreCertErrors, bool mutualAuth) const noexcept {
        _impl->_credHandle        = creds.GetCredHandle();
        _impl->_host              = std::move(host);
        _impl->_isServer          = false;
        _impl->_ignoreCertErrors  = ignoreCertErrors;
        _impl->_mutualAuth        = mutualAuth;
        _impl->_handshakeComplete = false;
    }

    void TlsSession::BeginServer(const Credentials& creds, bool requestClientCert) const noexcept {
        _impl->_credHandle        = creds.GetCredHandle();
        _impl->_isServer          = true;
        _impl->_requestClientCert = requestClientCert;
        _impl->_handshakeComplete = false;
    }


    bool TlsSession::IsServer()            const noexcept { return _impl->_isServer; }
    bool TlsSession::IsActive()            const noexcept { return _impl->_credHandle.dwLower != 0 || _impl->_credHandle.dwUpper != 0; }
    bool TlsSession::IsHandshakeComplete() const noexcept { return _impl->_handshakeComplete; }
    bool TlsSession::IsRenegotiation()     const noexcept { return _impl->_ctxtHandle.dwLower != 0 || _impl->_ctxtHandle.dwUpper != 0; }


    TlsSession::HandshakeOutcome TlsSession::AdvanceHandshake(
        std::span<std::byte> inBytes, std::span<std::byte> outBuf
    ) const noexcept {
        const bool firstPass{ !IsRenegotiation() };

        _impl->TokenBuffer() = SecBuffer{ _tul(inBytes.size()), _tul(SecurityBufferEnum::Token), inBytes.data() };
        _impl->ExtraBuffer() = SecBuffer{ 0                  , _tul(SecurityBufferEnum::Empty), nullptr        };

        _impl->OutBuffer() = SecBuffer{ _tul(outBuf.size())            , _tul(SecurityBufferEnum::Token), outBuf.data()           };
        _impl->MsgBuffer() = SecBuffer{ _tul(_impl->_buffers[3].size()), _tul(SecurityBufferEnum::Alert), _impl->_buffers[3].data() };

        SECURITY_STATUS status{};

        if (_impl->_isServer) {
            auto _dwSspiFlags{ AcceptSecurityContextFlags::SequenceDetect  |
                               AcceptSecurityContextFlags::ReplayDetect    |
                               AcceptSecurityContextFlags::Confidentiality |
                               AcceptSecurityContextFlags::ExtendedError   |
                               AcceptSecurityContextFlags::Stream };
            if (_impl->_requestClientCert)
                _dwSspiFlags |= AcceptSecurityContextFlags::MutualAuth;

            DWORD _pfContextAttr{};
            status = ::AcceptSecurityContext(
                &_impl->_credHandle,
                firstPass ? nullptr : &_impl->_ctxtHandle,
                &_impl->_inBufferDesc,
                _tul(_dwSspiFlags), 0,
                &_impl->_ctxtHandle,
                &_impl->_outBufferDesc, &_pfContextAttr, &_impl->_tsExpiry
            );
        } else {
            auto _dwSspiFlags{ InitializeSecurityContextFlags::SequenceDetect  |
                               InitializeSecurityContextFlags::ReplayDetect    |
                               InitializeSecurityContextFlags::Confidentiality |
                               InitializeSecurityContextFlags::ExtendedError   |
                               InitializeSecurityContextFlags::Stream };
            if (_impl->_mutualAuth)       _dwSspiFlags |= InitializeSecurityContextFlags::MutualAuth;
            if (_impl->_ignoreCertErrors) _dwSspiFlags |= InitializeSecurityContextFlags::ManualCredValidation;

            DWORD _pfContextAttr{};
            status = ::InitializeSecurityContextA(
                &_impl->_credHandle,
                firstPass ? nullptr : &_impl->_ctxtHandle,
                const_cast<SEC_CHAR*>(_impl->_host.c_str()),
                _tll(_dwSspiFlags), 0, 0,
                firstPass ? nullptr : &_impl->_inBufferDesc, 0,
                &_impl->_ctxtHandle,
                &_impl->_outBufferDesc, &_pfContextAttr, &_impl->_tsExpiry
            );
        }

        std::uint32_t consumed{ _tul(inBytes.size()) };
        if (_impl->ExtraBuffer().cbBuffer > 0 && _impl->ExtraBuffer().BufferType == _tul(SecurityBufferEnum::Extra))
            consumed -= _impl->ExtraBuffer().cbBuffer;

        const auto translated{ static_cast<EncryptStatusEnum>(status) };
        if (translated == EncryptStatusEnum::ErrOk)
            _impl->_handshakeComplete = true;

        return { translated, consumed, _impl->OutBuffer().cbBuffer };
    }


    TlsSession::StreamSizes TlsSession::GetStreamSizes() const noexcept {
        if (_impl->_streamSizes.cbMaximumMessage == 0) {
            ::QueryContextAttributesA(
                const_cast<CtxtHandle*>(&_impl->_ctxtHandle),
                macroSECPKG_ATTR_STREAM_SIZES,
                &_impl->_streamSizes
            );
        }
        return { _impl->_streamSizes.cbHeader, _impl->_streamSizes.cbTrailer, _impl->_streamSizes.cbMaximumMessage };
    }


    TlsSession::EncryptOutcome TlsSession::Encrypt(
        std::span<const std::byte> plain, std::span<std::byte> outBuf
    ) const noexcept {
        const auto& sizes{ _impl->_streamSizes };

        _impl->_secBuffers[0] = SecBuffer{ sizes.cbHeader      , _tul(SecurityBufferEnum::StreamHeader) , outBuf.data() };
        _impl->_secBuffers[1] = SecBuffer{ _tul(plain.size()), _tul(SecurityBufferEnum::Data)         , outBuf.data() + sizes.cbHeader };
        _impl->_secBuffers[2] = SecBuffer{ sizes.cbTrailer     , _tul(SecurityBufferEnum::StreamTrailer), outBuf.data() + sizes.cbHeader + plain.size() };
        _impl->_secBuffers[3] = SecBuffer{ 0                   , _tul(SecurityBufferEnum::Empty)        , nullptr };

        std::memcpy(_impl->_secBuffers[1].pvBuffer, plain.data(), plain.size());

        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, _impl->_secBuffers.data() };
        const SECURITY_STATUS status{ ::EncryptMessage(&_impl->_ctxtHandle, 0, &buffDesc, 0) };

        if (status != _tul(EncryptStatusEnum::ErrOk))
            return { static_cast<EncryptStatusEnum>(status), 0 };

        const std::uint32_t produced{ _impl->_secBuffers[0].cbBuffer + _impl->_secBuffers[1].cbBuffer + _impl->_secBuffers[2].cbBuffer };
        return { EncryptStatusEnum::ErrOk, produced };
    }


    TlsSession::DecryptOutcome TlsSession::Decrypt(std::span<std::byte> inBytes) const noexcept {
        std::span buffs{ _impl->_secBuffers };
        buffs[0] = SecBuffer{ _tul(inBytes.size()), _tul(SecurityBufferEnum::Data), inBytes.data() };
        buffs[1] = buffs[2] = buffs[3] = SecBuffer{ 0, _tul(SecurityBufferEnum::Empty), nullptr };

        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, buffs.data() };
        const SECURITY_STATUS status{ ::DecryptMessage(&_impl->_ctxtHandle, &buffDesc, 0, nullptr) };

        DecryptOutcome out{ static_cast<EncryptStatusEnum>(status), {}, inBytes };

        const auto dataBuffer { std::ranges::find(buffs, _tul(SecurityBufferEnum::Data),  &SecBuffer::BufferType) };
        const auto extraBuffer{ std::ranges::find(buffs, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };

        if (dataBuffer != buffs.end() && dataBuffer->cbBuffer > 0)
            out.data = { static_cast<std::byte*>(dataBuffer->pvBuffer), dataBuffer->cbBuffer }, out.extra = {};
        if (extraBuffer != buffs.end() && extraBuffer->cbBuffer > 0)
            out.extra = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };

        return out;
    }


    std::uint32_t TlsSession::Shutdown(std::span<std::byte> outBuf) const noexcept {
        DWORD dwType{ macroSCHANNEL_SHUTDOWN };

        _impl->OutBuffer() = SecBuffer{ sizeof(dwType), _tul(SecurityBufferEnum::Token), &dwType };
        SecBufferDesc outBufferDesc{ macroSECBUFFER_VERSION, 1, &_impl->OutBuffer() };

        if (::ApplyControlToken(&_impl->_ctxtHandle, &outBufferDesc) != _tul(EncryptStatusEnum::ErrOk))
            return 0;

        _impl->OutBuffer() = SecBuffer{ _tul(outBuf.size()), _tul(SecurityBufferEnum::Token), outBuf.data() };
        outBufferDesc      = SecBufferDesc{ macroSECBUFFER_VERSION, 1, &_impl->OutBuffer() };

        DWORD dwSSPIOutFlags{};
        TimeStamp tsExpiry{};
        SECURITY_STATUS status{};

        if (_impl->_isServer) {
            auto dwSspiFlags{ AcceptSecurityContextFlags::SequenceDetect  |
                              AcceptSecurityContextFlags::ReplayDetect    |
                              AcceptSecurityContextFlags::Confidentiality |
                              AcceptSecurityContextFlags::Stream };
            if (_impl->_requestClientCert)
                dwSspiFlags |= AcceptSecurityContextFlags::MutualAuth;

            status = ::AcceptSecurityContext(
                &_impl->_credHandle, &_impl->_ctxtHandle, nullptr,
                _tul(dwSspiFlags), 0, nullptr, &outBufferDesc, &dwSSPIOutFlags, &tsExpiry
            );
        } else {
            constexpr auto dwSspiFlags{
                InitializeSecurityContextFlags::SequenceDetect  |
                InitializeSecurityContextFlags::ReplayDetect    |
                InitializeSecurityContextFlags::Confidentiality |
                InitializeSecurityContextFlags::Stream
            };
            status = ::InitializeSecurityContextA(
                nullptr, &_impl->_ctxtHandle, nullptr,
                _tl(dwSspiFlags), 0, 0, nullptr, 0,
                nullptr, &outBufferDesc, &dwSSPIOutFlags, &tsExpiry
            );
        }

        if (status == _tul(EncryptStatusEnum::ErrOk))
            return _impl->OutBuffer().cbBuffer;
        return 0;
    }


    void TlsSession::DeleteContext() const noexcept {
        if (_impl->_ctxtHandle.dwLower != 0 || _impl->_ctxtHandle.dwUpper != 0) {
            ::DeleteSecurityContext(&_impl->_ctxtHandle);
            _impl->_ctxtHandle = {};
        }
        _impl->_handshakeComplete = false;
        _impl->_streamSizes       = {};
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
        SSL_CTX* _ctx{ nullptr };

        SSL* _ssl { nullptr };
        BIO* _rBio{ nullptr };
        BIO* _wBio{ nullptr };

        std::string _host{};
        bool        _isServer{};
        bool        _requestClientCert{};
        bool        _ignoreCertErrors{};
        bool        _handshakeComplete{};

        std::vector<std::byte> _decryptScratch{};

        ~Impl() noexcept {
            if (_ssl) SSL_free(_ssl);
        }
    };


    static EncryptStatusEnum S_CertVerifyToStatus(const long verifyResult) noexcept {
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

    static EncryptStatusEnum S_TranslateSslError(const SSL* ssl, const int ret, const bool produced) noexcept {
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
                if (verify != X509_V_OK) return S_CertVerifyToStatus(verify);
                return EncryptStatusEnum::ErrInvalidToken;
            }
            case SSL_ERROR_SYSCALL:
            default                : return EncryptStatusEnum::ErrInvalidToken;
        }
    }


    void TlsSession::BeginClient(const Credentials& creds, std::string host,
                                 bool ignoreCertErrors, bool mutualAuth) const noexcept {
        _impl->_ctx               = static_cast<SSL_CTX*>(creds.GetNativeHandle());
        _impl->_host              = std::move(host);
        _impl->_isServer          = false;
        _impl->_ignoreCertErrors  = ignoreCertErrors;
        _impl->_handshakeComplete = false;

        if (_impl->_ssl) { SSL_free(_impl->_ssl); _impl->_ssl = nullptr; }

        _impl->_ssl  = SSL_new(_impl->_ctx);
        _impl->_rBio = BIO_new(BIO_s_mem());
        _impl->_wBio = BIO_new(BIO_s_mem());
        SSL_set_bio(_impl->_ssl, _impl->_rBio, _impl->_wBio);

        SSL_set_connect_state(_impl->_ssl);
        SSL_set_tlsext_host_name(_impl->_ssl, _impl->_host.c_str());

        if (ignoreCertErrors) {
            SSL_set_verify(_impl->_ssl, SSL_VERIFY_NONE, nullptr);
        } else if (X509_VERIFY_PARAM* p{ SSL_get0_param(_impl->_ssl) }) {
            X509_VERIFY_PARAM_set_hostflags(p, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            X509_VERIFY_PARAM_set1_host(p, _impl->_host.c_str(), _impl->_host.size());
        }

        (void)mutualAuth;
    }

    void TlsSession::BeginServer(const Credentials& creds, bool requestClientCert) const noexcept {
        _impl->_ctx               = static_cast<SSL_CTX*>(creds.GetNativeHandle());
        _impl->_isServer          = true;
        _impl->_requestClientCert = requestClientCert;
        _impl->_handshakeComplete = false;

        if (_impl->_ssl) { SSL_free(_impl->_ssl); _impl->_ssl = nullptr; }

        _impl->_ssl  = SSL_new(_impl->_ctx);
        _impl->_rBio = BIO_new(BIO_s_mem());
        _impl->_wBio = BIO_new(BIO_s_mem());
        SSL_set_bio(_impl->_ssl, _impl->_rBio, _impl->_wBio);

        SSL_set_accept_state(_impl->_ssl);

        if (requestClientCert)
            SSL_set_verify(_impl->_ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    }


    bool TlsSession::IsServer()            const noexcept { return _impl->_isServer; }
    bool TlsSession::IsActive()            const noexcept { return _impl->_ssl != nullptr; }
    bool TlsSession::IsHandshakeComplete() const noexcept { return _impl->_handshakeComplete; }
    bool TlsSession::IsRenegotiation()     const noexcept { return _impl->_ssl != nullptr && _impl->_handshakeComplete; }


    TlsSession::HandshakeOutcome TlsSession::AdvanceHandshake(
        std::span<std::byte> inBytes, std::span<std::byte> outBuf
    ) const noexcept {
        if (!_impl->_ssl) return { EncryptStatusEnum::ErrInvalidHandle, 0, 0 };

        std::uint32_t consumed{ 0 };
        if (!inBytes.empty()) {
            const int written{ BIO_write(_impl->_rBio, inBytes.data(), static_cast<int>(inBytes.size())) };
            if (written > 0) consumed = static_cast<std::uint32_t>(written);
        }

        ERR_clear_error();
        const int ret{ SSL_do_handshake(_impl->_ssl) };

        std::uint32_t produced{ 0 };
        const int pending{ BIO_pending(_impl->_wBio) };
        if (pending > 0 && !outBuf.empty()) {
            const int n{ BIO_read(_impl->_wBio, outBuf.data(),
                                   static_cast<int>(std::min<size_t>(outBuf.size(), static_cast<size_t>(pending)))) };
            if (n > 0) produced = static_cast<std::uint32_t>(n);
        }

        const auto status{ S_TranslateSslError(_impl->_ssl, ret, produced > 0) };
        if (ret == 1) _impl->_handshakeComplete = true;

        return { status, consumed, produced };
    }


    TlsSession::StreamSizes TlsSession::GetStreamSizes() const noexcept {
        return { 64u, 64u, 16384u };
    }


    TlsSession::EncryptOutcome TlsSession::Encrypt(
        std::span<const std::byte> plain, std::span<std::byte> outBuf
    ) const noexcept {
        if (!_impl->_ssl) return { EncryptStatusEnum::ErrInvalidHandle, 0 };

        ERR_clear_error();
        const int written{ SSL_write(_impl->_ssl, plain.data(), static_cast<int>(plain.size())) };
        if (written <= 0) {
            const int err{ SSL_get_error(_impl->_ssl, written) };
            if (err == SSL_ERROR_ZERO_RETURN)
                return { EncryptStatusEnum::InfoContextExpired, 0 };
            return { EncryptStatusEnum::ErrEncryptFailure, 0 };
        }

        const int pending{ BIO_pending(_impl->_wBio) };
        if (pending <= 0)
            return { EncryptStatusEnum::ErrOk, 0 };

        const int n{ BIO_read(_impl->_wBio, outBuf.data(),
                               static_cast<int>(std::min<size_t>(outBuf.size(), static_cast<size_t>(pending)))) };
        if (n <= 0) return { EncryptStatusEnum::ErrEncryptFailure, 0 };

        return { EncryptStatusEnum::ErrOk, static_cast<std::uint32_t>(n) };
    }


    TlsSession::DecryptOutcome TlsSession::Decrypt(std::span<std::byte> inBytes) const noexcept {
        if (!_impl->_ssl) return { EncryptStatusEnum::ErrInvalidHandle, {}, {} };

        if (!inBytes.empty())
            BIO_write(_impl->_rBio, inBytes.data(), static_cast<int>(inBytes.size()));

        _impl->_decryptScratch.assign(inBytes.size(), std::byte{});

        size_t totalRead = 0;
        ERR_clear_error();

        while (totalRead < inBytes.size()) {
            const int n{ SSL_read(_impl->_ssl, _impl->_decryptScratch.data() + totalRead,
                                  static_cast<int>(inBytes.size() - totalRead)) };
            if (n > 0) {
                totalRead += static_cast<size_t>(n);
            } else {
                const int err{ SSL_get_error(_impl->_ssl, n) };
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
            std::memcpy(inBytes.data(), _impl->_decryptScratch.data(), totalRead);
            return { EncryptStatusEnum::ErrOk,
                     std::span{ inBytes.data(), totalRead },
                     {} };
        }

        return { EncryptStatusEnum::ErrDecryptFailure, {}, {} };
    }


    std::uint32_t TlsSession::Shutdown(std::span<std::byte> outBuf) const noexcept {
        if (!_impl->_ssl) return 0;

        ERR_clear_error();
        SSL_shutdown(_impl->_ssl);

        const int pending{ BIO_pending(_impl->_wBio) };
        if (pending <= 0 || outBuf.empty()) return 0;

        const int n{ BIO_read(_impl->_wBio, outBuf.data(),
                              static_cast<int>(std::min<size_t>(outBuf.size(), static_cast<size_t>(pending)))) };
        return n > 0 ? static_cast<std::uint32_t>(n) : 0;
    }


    void TlsSession::DeleteContext() const noexcept {
        if (_impl->_ssl) {
            SSL_free(_impl->_ssl);
            _impl->_ssl  = nullptr;
            _impl->_rBio = nullptr;
            _impl->_wBio = nullptr;
        }
        _impl->_handshakeComplete = false;
    }


#endif


    TlsSession::TlsSession() : _impl(std::make_unique<Impl>()) {}
    TlsSession::~TlsSession() noexcept = default;

    TlsSession::TlsSession(TlsSession&& other) noexcept = default;
    TlsSession& TlsSession::operator=(TlsSession&& other) noexcept = default;

    TlsSession TlsSession::MakeChild() const {
        TlsSession session;
        session._impl->_isServer = _impl->_isServer;
        return session;
    }
}