#pragma once
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/Macros.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Connect(Data& data) {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::Unknown };

        auto [addr, addr_len, addrFamily]{ *addrRes };

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

        if constexpr (Data::Family == AddressFamilyEnum::Inet6) {
            int opt{}; // TODO: FUTURE: Implement a proper way to set options inside of data
            setsockopt(data.socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&opt), sizeof(opt));
        }

        // ReSharper disable once CppTooWideScopeInitStatement
        const int result{ connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };
        if (result == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };
        }

        data.handshakeCallback = &TlsConnectPolicy::ClientHandshake;
        return TlsConnectPolicy::ClientHandshake(data);
    }

// I hate TLS so much

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::ClientHandshake(Data& data) {
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
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ error };
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

        constexpr auto dwSspiFlags{
            InitializeSecurityContextFlags::SequenceDetect |
            InitializeSecurityContextFlags::ReplayDetect   |
            InitializeSecurityContextFlags::Confidentiality |
            InitializeSecurityContextFlags::ExtendedError  |
            InitializeSecurityContextFlags::Stream };


        DWORD pfContextAttr{};
        SECURITY_STATUS status{};

        const bool isRenegotiation{ data.ctxtHandle.dwLower != 0 || data.ctxtHandle.dwUpper != 0 };
        bool firstPass{ !isRenegotiation };

        DWORD receivedBytes{ data.pendingData };
        data.pendingData = 0;

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
                outBuffer.pvBuffer = &dwType
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

                constexpr auto dwSSPIFlags{
                    InitializeSecurityContextFlags::SequenceDetect |
                    InitializeSecurityContextFlags::ReplayDetect   |
                    InitializeSecurityContextFlags::Confidentiality |
                    InitializeSecurityContextFlags::Stream
                };

                DWORD dwSSPIOutFlags{};
                TimeStamp tsExpiry{};

                status = InitializeSecurityContextA(
                    nullptr,
                    &data.ctxtHandle,
                    nullptr,
                    _tl(dwSSPIFlags),
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
