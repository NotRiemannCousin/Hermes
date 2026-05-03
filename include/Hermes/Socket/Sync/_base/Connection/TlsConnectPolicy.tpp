#pragma once
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/Macros.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Connect(Data& data, Options options) noexcept {
        const auto s_makeHandshake = [&](std::monostate) {
            data.handshakeCallback = [options](Data& d) {
                return TlsConnectPolicy::ClientHandshake(d, options);
            };

            return TlsConnectPolicy::ClientHandshake(data, std::move(options));
        };

        return DefaultConnectPolicy<Data>::Connect(data, options)
                .and_then(s_makeHandshake);
    }

// I hate TLS so much

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::ClientHandshake(Data& data, Options options) {
#pragma region fast-fail and lambdas

        if (data.credentials == nullptr) data.credentials = &Network::GetClientCredentials();
        if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();

        if (data.host.size() >= 255)
            return std::unexpected{ ConnectionErrorEnum::HandshakeFailed };

        static constexpr auto s_hasData = [](const SecBuffer buffer) {
            return buffer.cbBuffer > 0;
        };

        const auto s_clearAndExit = [&](ConnectionErrorEnum error = ConnectionErrorEnum::HandshakeFailed){
            DeleteSecurityContext(&data.ctxtHandle);
            data.ctxtHandle = {};

            Close(data);
            return std::unexpected{ error };
        };


        const auto s_setTimeout = [&](const int value) {
            setsockopt(data.socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
            setsockopt(data.socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
        };

#pragma endregion


#pragma region buffers

        std::array<std::array<std::byte, 0x4000>, 4> buffers{};
        std::array<SecBuffer, 4> secBuffers{};

        auto& tokenBuffer{ secBuffers[0] }; // input:  data received from client
        auto& extraBuffer{ secBuffers[1] }; // input:  leftover from previous iteration

        auto& outBuffer{ secBuffers[2] }; // output: token to send to client
        auto& msgBuffer{ secBuffers[3] }; // output: alert


        SecBufferDesc inBufferDesc{ SecBufferDesc{ macroSECBUFFER_VERSION, 2, &tokenBuffer } }; // token, extra
        SecBufferDesc outBufferDesc{ SecBufferDesc{ macroSECBUFFER_VERSION, 2, &outBuffer } }; // out, msg
        const PSecBufferDesc pInBufferDesc{ &inBufferDesc };

#pragma endregion


#pragma region settings and lifecycle

        CredHandle credHandle{ data.credentials->GetCredHandle() };
        TimeStamp tsExpiry{};

        auto dwSspiFlags{
            InitializeSecurityContextFlags::SequenceDetect |
            InitializeSecurityContextFlags::ReplayDetect   |
            InitializeSecurityContextFlags::Confidentiality |
            InitializeSecurityContextFlags::ExtendedError  |
            InitializeSecurityContextFlags::Stream };

        if (options.requestMutualAuth)
            dwSspiFlags |= InitializeSecurityContextFlags::MutualAuth;

        if (options.ignoreCertificateErrors)
            dwSspiFlags |= InitializeSecurityContextFlags::ManualCredValidation;


        DWORD pfContextAttr{};
        SECURITY_STATUS status{};

        const bool isRenegotiation{ data.ctxtHandle.dwLower != 0 || data.ctxtHandle.dwUpper != 0 };
        bool firstPass{ !isRenegotiation };

        DWORD receivedBytes{ data.pendingData };
        data.pendingData = 0;


        if (options.handshakeTimeout.count() != 0)
            s_setTimeout(options.handshakeTimeout.count());

#pragma endregion

        do {
            tokenBuffer = SecBuffer{ receivedBytes, _tul(SecurityBufferEnum::Token), data.state->decryptedData.data() };
            extraBuffer = SecBuffer{ 0            , _tul(SecurityBufferEnum::Empty), nullptr };

            outBuffer = SecBuffer{ _tul(buffers[2].size()), _tul(SecurityBufferEnum::Token), buffers[2].data() };
            msgBuffer = SecBuffer{ _tul(buffers[3].size()), _tul(SecurityBufferEnum::Alert), buffers[3].data() };

            status = InitializeSecurityContextA(
                &credHandle,
                firstPass ? nullptr : &data.ctxtHandle,
                const_cast<SEC_CHAR*>(data.host.c_str()),
                _tll(dwSspiFlags), 0, 0,
                firstPass ? nullptr : pInBufferDesc, 0,
                &data.ctxtHandle,
                &outBufferDesc, &pfContextAttr, &tsExpiry
            );

            firstPass = false;

            if (s_hasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::Extra) {
                std::memmove(data.state->decryptedData.data(),
                            data.state->decryptedData.data() + receivedBytes - extraBuffer.cbBuffer,
                            extraBuffer.cbBuffer); // reset buffer
                receivedBytes = extraBuffer.cbBuffer;
            }

            // All values listed in:
            // https://learn.microsoft.com/en-us/windows/win32/secauthn/initializesecuritycontext--schannel
            switch (static_cast<EncryptStatusEnum>(status)) {
                // ----------------------------------------------------------------------
                //                  Happy Path
                // ----------------------------------------------------------------------

                case EncryptStatusEnum::InfoCompleteAndContinue:
                case EncryptStatusEnum::InfoCompleteNeeded:

#pragma region Complete Auth
                    CompleteAuthToken(&data.ctxtHandle, &outBufferDesc);

                    if (status == EncryptStatusEnum::InfoCompleteAndContinue)
                        continue;
#pragma endregion

                case EncryptStatusEnum::InfoContinueNeeded:

#pragma region Send More Data

                    if (status != EncryptStatusEnum::InfoCompleteNeeded && s_hasData(outBuffer)) {
                        const int sent{ send(data.socket,
                                       static_cast<const char*>(outBuffer.pvBuffer),
                                       outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer)
                            return s_clearAndExit(ConnectionErrorEnum::HandshakeFailed);
                    }

                    if (extraBuffer.BufferType != SecurityBufferEnum::Extra)
                        receivedBytes = 0;

                    if (receivedBytes > 0) {
                        continue;
                    }

#pragma endregion

                case EncryptStatusEnum::ErrIncompleteMessage:

#pragma region Receive More Data
                    {
                        const int received{ recv(data.socket,
                                           reinterpret_cast<char*>(data.state->decryptedData.data() + receivedBytes),
                                           data.state->decryptedData.size() - receivedBytes, 0) };
                        receivedBytes += received;

                        if (received <= 0)
                            return s_clearAndExit();

                        if (status == EncryptStatusEnum::ErrIncompleteMessage)
                            continue;
                    }

#pragma endregion
                    break;
                case EncryptStatusEnum::ErrOk: {
                    if (s_hasData(outBuffer)) {
                        int sent{ send(data.socket,
                                       static_cast<const char*>(outBuffer.pvBuffer),
                                       outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer)
                            return s_clearAndExit();
                    }


                    status = QueryContextAttributesA(&data.ctxtHandle,
                                                    macroSECPKG_ATTR_STREAM_SIZES,
                                                    &data.contextStreamSizes);

                    if (status != EncryptStatusEnum::ErrOk)
                        return s_clearAndExit();

                    data.isHandshakeComplete = true;
                    data.isServer            = false;

                    s_setTimeout({});


                    std::size_t extraBufferSize{
                        s_hasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::Extra
                            ? extraBuffer.cbBuffer
                            : 0
                    };

                    data.state->decryptedExtraSpan = std::span<std::byte>{
                        data.state->decryptedData.data(),
                        extraBufferSize
                    };

                    return {};
                }

                case EncryptStatusEnum::ErrIncompleteCredentials:
                    return s_clearAndExit(ConnectionErrorEnum::CertificateError);

                // ----------------------------------------------------------------------
                //                  Errors
                // ----------------------------------------------------------------------

                case EncryptStatusEnum::ErrInsufficientMemory:
                case EncryptStatusEnum::ErrInvalidHandle:
                case EncryptStatusEnum::ErrInvalidToken:
                    return s_clearAndExit();
                case EncryptStatusEnum::ErrLogonDenied:
                case EncryptStatusEnum::ErrNoCredentials:
                case EncryptStatusEnum::ErrUntrustedRoot:
                case EncryptStatusEnum::ErrNoAuthenticatingAuthority:

                    return s_clearAndExit(ConnectionErrorEnum::CertificateError);

                case EncryptStatusEnum::ErrInternalError:
                case EncryptStatusEnum::ErrWrongPrincipal:
                case EncryptStatusEnum::ErrTargetUnknown:
                case EncryptStatusEnum::ErrUnsupportedFunction:
                case EncryptStatusEnum::ErrApplicationProtocolMismatch:
                default:
                    return s_clearAndExit(ConnectionErrorEnum::Unknown);
            }


        } while (status == EncryptStatusEnum::InfoContinueNeeded ||
                 status == EncryptStatusEnum::ErrIncompleteMessage);

        return s_clearAndExit();
    }

    template<SocketDataConcept Data>
    void TlsConnectPolicy<Data>::Close(Data& data) {
        static constexpr auto s_hasData = [](const SecBuffer buffer) {
            return buffer.cbBuffer > 0;
        };

        if (data.socket == macroINVALID_SOCKET) return;

        if (data.isHandshakeComplete) {
            DWORD dwType{ macroSCHANNEL_SHUTDOWN };

            SecBuffer outBuffer{
                sizeof(dwType),
                _tul(SecurityBufferEnum::Token),
                &dwType
            };

            SecBufferDesc outBufferDesc{
                macroSECBUFFER_VERSION,
                1,
                &outBuffer
            };

            SECURITY_STATUS status = ApplyControlToken(&data.ctxtHandle, &outBufferDesc);

            if (status == EncryptStatusEnum::ErrOk) {
                outBuffer     = SecBuffer{ 0, _tul(SecurityBufferEnum::Token), nullptr };
                outBufferDesc = SecBufferDesc{ macroSECBUFFER_VERSION, 1, &outBuffer };

                constexpr auto dwSspiFlags{
                    InitializeSecurityContextFlags::SequenceDetect  |
                    InitializeSecurityContextFlags::ReplayDetect    |
                    InitializeSecurityContextFlags::Confidentiality |
                    InitializeSecurityContextFlags::Stream
                };

                DWORD dwSSPIOutFlags{};
                TimeStamp tsExpiry{};

                status = InitializeSecurityContextA(
                    nullptr,
                    &data.ctxtHandle,
                    nullptr,
                    _tl(dwSspiFlags),
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    &outBufferDesc,
                    &dwSSPIOutFlags,
                    &tsExpiry
                );

                if (status == EncryptStatusEnum::ErrOk && s_hasData(outBuffer))
                    send(data.socket, static_cast<const char*>(outBuffer.pvBuffer),
                        outBuffer.cbBuffer, 0);
            }

            DeleteSecurityContext(&data.ctxtHandle);
            data.ctxtHandle = {};
            data.isHandshakeComplete = false;
        }

        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }

    template<SocketDataConcept Data>
    void TlsConnectPolicy<Data>::Abort(Data &data) {
        constexpr linger lingerOption{ 1, 0 };

        setsockopt(
            data.socket,
            SOL_SOCKET,
            SO_LINGER,
            reinterpret_cast<const char*>(&lingerOption),
            sizeof(lingerOption)
        );

        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }
}
