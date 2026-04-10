#pragma once
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/Macros.hpp>

// I hate TLS so much (server edition)

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAcceptPolicy<Data>::Listen(Data& data, const int backlog) noexcept {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes)
            return std::unexpected{ ConnectionErrorEnum::InvalidEndpoint };

        auto [addr, addr_len, addrFamily]{ *addrRes };

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

        if (bind(data.socket, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(addr_len)) == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::AddressInUse };
        }

        if (listen(data.socket, backlog) == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::ListenFailed };
        }

        return {};
    }


    template<SocketDataConcept Data>
    ConnectionResultOper TlsAcceptPolicy<Data>::Accept(Data& data, Data& outData) noexcept {
        sockaddr_storage clientAddr{};
        int clientAddrLen{ sizeof(clientAddr) };

        outData.socket = accept(data.socket,
            reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

        if (outData.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

        auto endpointRes{ Data::EndpointType::FromSockAddr(
            SocketInfoAddr{ clientAddr, static_cast<size_t>(clientAddrLen), Data::Family }) };

        if (!endpointRes) {
            closesocket(outData.socket);
            outData.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::InvalidEndpoint };
        }

        outData.endpoint = std::move(*endpointRes);

        data.handshakeCallback = &TlsAcceptPolicy::ServerHandshake;
        return ServerHandshake(outData);
    }

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAcceptPolicy<Data>::ServerHandshake(Data& data) noexcept {
#pragma region fast-fail and lambdas

        if (data.credentials == nullptr) return std::unexpected{ ConnectionErrorEnum::CertificateError };
        if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();


        static constexpr auto s_hasData = [](const SecBuffer buffer) {
            return buffer.cbBuffer > 0;
        };

        const auto s_clearAndExit = [&](ConnectionErrorEnum error = ConnectionErrorEnum::HandshakeFailed) {
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

#pragma endregion


#pragma region settings and lifecycle

        CredHandle credHandle{ data.credentials->GetCredHandle() };
        TimeStamp tsExpiry{};

        constexpr auto dwSspiFlags{
            AcceptSecurityContextFlags::SequenceDetect |
            AcceptSecurityContextFlags::ReplayDetect |
            AcceptSecurityContextFlags::Confidentiality |
            AcceptSecurityContextFlags::ExtendedError |
            AcceptSecurityContextFlags::Stream };


        DWORD pfContextAttr{};
        SECURITY_STATUS status{};

        const bool isRenegotiation{ data.ctxtHandle.dwLower != 0 || data.ctxtHandle.dwUpper != 0 };
        bool firstPass{ !isRenegotiation };;

        DWORD receivedBytes{};

        if (data.pendingData > 0) {
            receivedBytes = data.pendingData;
            data.pendingData = 0;
        } else {
            const int received{ recv(data.socket,
                reinterpret_cast<char*>(data.state->decryptedData.data()),
                static_cast<int>(data.state->decryptedData.size()), 0) };

            if (received <= 0) return s_clearAndExit();
            receivedBytes = static_cast<DWORD>(received);
        }

#pragma endregion

        do {
            tokenBuffer   = SecBuffer{ receivedBytes, _tul(SecurityBufferEnum::Token), data.state->decryptedData.data() };
            extraBuffer   = SecBuffer{ 0            , _tul(SecurityBufferEnum::Empty), nullptr };

            outBuffer     = SecBuffer{ _tul(buffers[2].size()), _tul(SecurityBufferEnum::Token), buffers[2].data() };
            msgBuffer     = SecBuffer{ _tul(buffers[3].size()), _tul(SecurityBufferEnum::Alert), buffers[3].data() };

            status = AcceptSecurityContext(
                &credHandle,
                firstPass ? nullptr : &data.ctxtHandle,
                &inBufferDesc,
                _tul(dwSspiFlags),
                0,
                &data.ctxtHandle,
                &outBufferDesc, &pfContextAttr, &tsExpiry
            );

            firstPass = false;

            if (s_hasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::Extra) {
                std::memmove(data.state->decryptedData.data(),
                             data.state->decryptedData.data() + receivedBytes - extraBuffer.cbBuffer,
                             extraBuffer.cbBuffer); // reset buffer
                receivedBytes = extraBuffer.cbBuffer;
            } else {
                receivedBytes = 0;
            }

            // All values listed in:
            // https://learn.microsoft.com/en-us/windows/win32/secauthn/initializesecuritycontext--schannel
            switch (static_cast<EncryptStatusEnum>(status)) {
                // ----------------------------------------------------------------------
                //                  Happy Path
                // ----------------------------------------------------------------------

                case EncryptStatusEnum::InfoContinueNeeded:
                    if (s_hasData(outBuffer)) {
                        const int sent{ send(data.socket,
                            static_cast<const char*>(outBuffer.pvBuffer),
                            outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR || sent != static_cast<int>(outBuffer.cbBuffer))
                            return s_clearAndExit();
                    }

                case EncryptStatusEnum::ErrIncompleteMessage: {
                    const int received{ recv(data.socket,
                        reinterpret_cast<char*>(data.state->decryptedData.data() + receivedBytes),
                        static_cast<int>(data.state->decryptedData.size() - receivedBytes), 0) };

                    if (received <= 0) return s_clearAndExit();
                    receivedBytes += static_cast<DWORD>(received);

                    if (status == EncryptStatusEnum::ErrIncompleteMessage)
                        continue;
                    break;
                }

                case EncryptStatusEnum::ErrOk: {
                    if (s_hasData(outBuffer)) {
                        const int sent{ send(data.socket,
                            static_cast<const char*>(outBuffer.pvBuffer),
                            outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR)
                            return s_clearAndExit();
                    }

                    if (s_hasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::Extra) {
                        std::memmove(data.state->decryptedData.data(),
                                     data.state->decryptedData.data() + receivedBytes - extraBuffer.cbBuffer,
                                     extraBuffer.cbBuffer);
                        data.state->decryptedExtraSpan = {
                            data.state->decryptedData.data(),
                            extraBuffer.cbBuffer
                        };
                    }

                    status = QueryContextAttributesA(&data.ctxtHandle,
                        macroSECPKG_ATTR_STREAM_SIZES, &data.contextStreamSizes);

                    if (status != EncryptStatusEnum::ErrOk)
                        return s_clearAndExit();

                    data.isHandshakeComplete = true;
                    data.isServer            = true;

                    return {};
                }

                case EncryptStatusEnum::ErrIncompleteCredentials:
                    return s_clearAndExit(ConnectionErrorEnum::CertificateError);

                case EncryptStatusEnum::ErrInsufficientMemory:
                case EncryptStatusEnum::ErrInvalidHandle:
                case EncryptStatusEnum::ErrInvalidToken:
                    return s_clearAndExit();

                case EncryptStatusEnum::ErrLogonDenied:
                case EncryptStatusEnum::ErrCertUnknown:
                case EncryptStatusEnum::ErrCertExpired:
                case EncryptStatusEnum::ErrNoCredentials:
                case EncryptStatusEnum::ErrUntrustedRoot:
                case EncryptStatusEnum::ErrNoAuthenticatingAuthority:
                    return s_clearAndExit(ConnectionErrorEnum::CertificateError);

                case EncryptStatusEnum::ErrInternalError:
                case EncryptStatusEnum::ErrWrongPrincipal:
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
    void TlsAcceptPolicy<Data>::Close(Data& data) noexcept {
        static constexpr auto s_hasData = [](const SecBuffer buffer) {
            return buffer.cbBuffer > 0;
        };

        if (data.isHandshakeComplete) {
            DWORD dwType{ macroSCHANNEL_SHUTDOWN };

            SecBuffer outBuffer{
                sizeof(dwType),
                _tul(SecurityBufferEnum::Token),
                &dwType
            };
            SecBufferDesc outBufferDesc{ macroSECBUFFER_VERSION, 1, &outBuffer };

            const SECURITY_STATUS applyStatus{ ApplyControlToken(&data.ctxtHandle, &outBufferDesc) };

            if (applyStatus == EncryptStatusEnum::ErrOk) {
                outBuffer     = SecBuffer{ 0, _tul(SecurityBufferEnum::Token), nullptr };
                outBufferDesc = SecBufferDesc{ macroSECBUFFER_VERSION, 1, &outBuffer };

                constexpr auto dwSspiFlags{
                    AcceptSecurityContextFlags::SequenceDetect |
                    AcceptSecurityContextFlags::ReplayDetect |
                    AcceptSecurityContextFlags::Confidentiality |
                    AcceptSecurityContextFlags::Stream
                };

                DWORD dwSSPIOutFlags{};
                CredHandle credHandle{ data.credentials->GetCredHandle() };
                TimeStamp tsExpiry{};

                // Server side uses AcceptSecurityContext even for the shutdown token.
                // https://learn.microsoft.com/en-us/windows/win32/secauthn/shutting-down-an-schannel-connection
                AcceptSecurityContext(
                    &credHandle,
                    &data.ctxtHandle,
                    nullptr,
                    _tul(dwSspiFlags),
                    0,
                    nullptr,
                    &outBufferDesc,
                    &dwSSPIOutFlags,
                    &tsExpiry
                );

                if (s_hasData(outBuffer))
                    send(data.socket,
                         static_cast<const char*>(outBuffer.pvBuffer),
                         outBuffer.cbBuffer, 0);
            }

            DeleteSecurityContext(&data.ctxtHandle);
            data.ctxtHandle          = {};
            data.isHandshakeComplete = false;
        }

        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }

    template<SocketDataConcept Data>
    void TlsAcceptPolicy<Data>::Abort(Data &data) noexcept {
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
