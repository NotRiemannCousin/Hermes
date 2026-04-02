#pragma once
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/Macros.hpp>

namespace Hermes {
    static constexpr bool HasData(const SecBuffer buffer) {
        return buffer.cbBuffer > 0;
    }


    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Connect(Data& data) {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::Unknown };

        auto [addr, addr_len, addrFamily] = *addrRes;

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };

        // ReSharper disable once CppTooWideScopeInitStatement
        const int result{ connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };
        if (result == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::ConnectionFailed };
        }

        if (!Network::IsInitialized()) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
        }

        return TlsConnectPolicy::ClientHandshake(data);
    }

// I hate TLS so much

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::ClientHandshake(Data& data) {
        const CredHandle& credHandle{ Network::GetCredHandle() };
        TimeStamp tsExpiry{ Network::GetExpiry() };

        SecBufferDesc outBufferDesc{};
        SecBufferDesc inBufferDesc{};

        auto& buffers{ data.buffers };
        auto& tokenBuffer{ data.secBuffers[0] };
        auto& extraBuffer{ data.secBuffers[1] };

        auto& outBuffer{ data.secBuffers[2] };
        auto& msgBuffer{ data.secBuffers[3] };

        constexpr auto dwSspiFlags{
            InitializeSecurityContextFlags::SequenceDetect |
            InitializeSecurityContextFlags::ReplayDetect   |
            InitializeSecurityContextFlags::Confidentiality |
            InitializeSecurityContextFlags::ExtendedError  |
            InitializeSecurityContextFlags::Stream };

        DWORD pfContextAttr{};
        if (data.host.size() >= 255)
            return std::unexpected{ ConnectionErrorEnum::HandshakeFailed };

        bool firstPass{ true };
        DWORD receivedBytes{};
        SECURITY_STATUS status{};

        const auto ClearAndExit = [&](ConnectionErrorEnum error = ConnectionErrorEnum::HandshakeFailed){
            DeleteSecurityContext(&data.ctxtHandle);
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ error };
        };

        inBufferDesc  = SecBufferDesc{ macroSECBUFFER_VERSION, 2, &tokenBuffer }; // token, extra
        outBufferDesc = SecBufferDesc{ macroSECBUFFER_VERSION, 2, &outBuffer   }; // out, msg
        const PSecBufferDesc pInBufferDesc{ &inBufferDesc };

        do {
            tokenBuffer = SecBuffer{ receivedBytes, _tul(SecurityBufferEnum::Token), data.decryptedData.data() };
            extraBuffer = SecBuffer{ 0,             _tul(SecurityBufferEnum::Empty), nullptr };

            outBuffer = SecBuffer{ _tul(buffers[2].size()), _tul(SecurityBufferEnum::Token), buffers[2].data() };
            msgBuffer = SecBuffer{ _tul(buffers[3].size()), _tul(SecurityBufferEnum::Alert), buffers[3].data() };

            status = InitializeSecurityContextA(
                const_cast<PCredHandle>(&credHandle),
                firstPass ? nullptr : &data.ctxtHandle,
                const_cast<SEC_CHAR*>(data.host.c_str()),
                _tll(dwSspiFlags), 0, 0,
                firstPass ? nullptr : pInBufferDesc, 0,
                firstPass ? &data.ctxtHandle : nullptr,
                &outBufferDesc, &pfContextAttr, &tsExpiry
            );

            firstPass = false;

            if (HasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::Extra) {
                std::memmove(data.decryptedData.data(),
                            data.decryptedData.data() + receivedBytes - extraBuffer.cbBuffer,
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

                    if (status != EncryptStatusEnum::InfoCompleteNeeded && HasData(outBuffer)) {
                        int sent{ send(data.socket,
                                       static_cast<const char*>(outBuffer.pvBuffer),
                                       outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer)
                            return ClearAndExit(ConnectionErrorEnum::HandshakeFailed);
                    }

                    if (extraBuffer.BufferType != SecurityBufferEnum::Extra)
                        receivedBytes = 0;

#pragma endregion

                case EncryptStatusEnum::ErrIncompleteMessage:

#pragma region Receive More Data
                    {
                        const int received{ recv(data.socket,
                                           reinterpret_cast<char*>(data.decryptedData.data() + receivedBytes),
                                           data.decryptedData.size() - receivedBytes, 0) };
                        receivedBytes += received;

                        if (received <= 0)
                            return ClearAndExit();

                        if (status == EncryptStatusEnum::ErrIncompleteMessage)
                            continue;
                    }

#pragma endregion
                    break;
                case EncryptStatusEnum::ErrOk: {
                    if (HasData(outBuffer)) {
                        int sent{ send(data.socket,
                                       static_cast<const char*>(outBuffer.pvBuffer),
                                       outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer)
                            return ClearAndExit();
                    }


                    status = QueryContextAttributesA(&data.ctxtHandle,
                                                    SECPKG_ATTR_STREAM_SIZES,
                                                    &data.contextStreamSizes);

                    if (status != EncryptStatusEnum::ErrOk)
                        return ClearAndExit();

                    data.isHandshakeComplete = true;
                    data.isServer = false;

                    return {};
                }

                case EncryptStatusEnum::ErrIncompleteCredentials:
                    return ClearAndExit(ConnectionErrorEnum::CertificateError);

                // ----------------------------------------------------------------------
                //                  Errors
                // ----------------------------------------------------------------------

                case EncryptStatusEnum::ErrInsufficientMemory:
                case EncryptStatusEnum::ErrInvalidHandle:
                case EncryptStatusEnum::ErrInvalidToken:
                    return ClearAndExit();
                case EncryptStatusEnum::ErrLogonDenied:
                case EncryptStatusEnum::ErrNoCredentials:
                case EncryptStatusEnum::ErrNoAuthenticatingAuthority:

                    return ClearAndExit(ConnectionErrorEnum::CertificateError);

                case EncryptStatusEnum::ErrInternalError:
                case EncryptStatusEnum::ErrWrongPrincipal:
                case EncryptStatusEnum::ErrTargetUnknown:
                case EncryptStatusEnum::ErrUnsupportedFunction:
                case EncryptStatusEnum::ErrApplicationProtocolMismatch:
                default:
                    return ClearAndExit(ConnectionErrorEnum::Unknown);
            }


        } while (status == EncryptStatusEnum::InfoContinueNeeded ||
                 status == EncryptStatusEnum::ErrIncompleteMessage);

        return ClearAndExit();
    }

    template<SocketDataConcept Data>
    void TlsConnectPolicy<Data>::Close(Data& data) {
        if (data.socket == macroINVALID_SOCKET) return;

        if (data.isHandshakeComplete) {
            DWORD dwType = SCHANNEL_SHUTDOWN;

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
                outBuffer = SecBuffer{
                    0,
                    _tul(SecurityBufferEnum::Token),
                    nullptr
                };

                outBufferDesc = SecBufferDesc{
                    macroSECBUFFER_VERSION,
                    1,
                    &outBuffer
                };

                constexpr auto dwSSPIFlags =
                    InitializeSecurityContextFlags::SequenceDetect |
                    InitializeSecurityContextFlags::ReplayDetect   |
                    InitializeSecurityContextFlags::Confidentiality |
                    InitializeSecurityContextFlags::Stream;

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

                if (status == EncryptStatusEnum::ErrOk && HasData(outBuffer))
                    send(data.socket, static_cast<const char*>(outBuffer.pvBuffer),
                        outBuffer.cbBuffer, 0);
            }

            DeleteSecurityContext(&data.ctxtHandle);
            data.ctxtHandle = {};
            data.isHandshakeComplete = false;
        }

        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Both));
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
    }
}