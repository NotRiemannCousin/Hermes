#pragma once
#include <iostream>
#include <Hermes/_base/Network.hpp>

namespace Hermes {
    static constexpr bool HasData(const SecBuffer buffer) {
        return buffer.cbBuffer > 0;
    }


    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Connect(Data& data) {
        auto addrRes{ data.endpoint.ToSockAddr() };
        if (!addrRes.has_value())
            return std::unexpected{ ConnectionErrorEnum::UNKNOWN };

        auto [addr, addr_len, addrFamily] = *addrRes;

        data.socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
        if (data.socket == macroINVALID_SOCKET)
            return std::unexpected{ ConnectionErrorEnum::CONNECTION_FAILED };

        const int result{ connect(data.socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };
        if (result == macroSOCKET_ERROR) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::CONNECTION_FAILED };
        }

        if (!Network::IsInitialized()) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
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
            InitializeSecurityContextFlags::SEQUENCE_DETECT |
            InitializeSecurityContextFlags::REPLAY_DETECT   |
            InitializeSecurityContextFlags::CONFIDENTIALITY |
            InitializeSecurityContextFlags::EXTENDED_ERROR  |
            InitializeSecurityContextFlags::STREAM };

        DWORD pfContextAttr{};
        if (data.host.size() >= 255)
            return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };

        bool firstPass{ true };
        DWORD receivedBytes{};
        SECURITY_STATUS status{};

        const auto ClearAndExit = [&](ConnectionErrorEnum error = ConnectionErrorEnum::HANDSHAKE_FAILED){
            DeleteSecurityContext(&data.ctxtHandle);
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ error };
        };

        inBufferDesc  = SecBufferDesc{ macroSECBUFFER_VERSION, 2, &tokenBuffer }; // token, extra
        outBufferDesc = SecBufferDesc{ macroSECBUFFER_VERSION, 2, &outBuffer };   // out, msg
        PSecBufferDesc pInBufferDesc{ &inBufferDesc };

        do {
            tokenBuffer = SecBuffer{ receivedBytes, _tul(SecurityBufferEnum::TOKEN), data.decryptedData.data() };
            extraBuffer = SecBuffer{ 0,             _tul(SecurityBufferEnum::EMPTY), nullptr };

            outBuffer = SecBuffer{ _tul(buffers[2].size()), _tul(SecurityBufferEnum::TOKEN), buffers[2].data() };
            msgBuffer = SecBuffer{ _tul(buffers[3].size()), _tul(SecurityBufferEnum::ALERT), buffers[3].data() };

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

            if (HasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::EXTRA) {
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

                case EncryptStatusEnum::INFO_COMPLETE_AND_CONTINUE:
                case EncryptStatusEnum::INFO_COMPLETE_NEEDED:

#pragma region Complete Auth
                    CompleteAuthToken(&data.ctxtHandle, &outBufferDesc);

                    if (status == EncryptStatusEnum::INFO_COMPLETE_AND_CONTINUE)
                        continue;
#pragma endregion

                case EncryptStatusEnum::INFO_CONTINUE_NEEDED:

#pragma region Send More Data

                    if (status != EncryptStatusEnum::INFO_COMPLETE_NEEDED && HasData(outBuffer)) {
                        int sent{ send(data.socket,
                                       static_cast<const char*>(outBuffer.pvBuffer),
                                       outBuffer.cbBuffer, 0) };

                        if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer)
                            return ClearAndExit(ConnectionErrorEnum::HANDSHAKE_FAILED);
                    }

                    if (extraBuffer.BufferType != SecurityBufferEnum::EXTRA)
                        receivedBytes = 0;

#pragma endregion

                case EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE:

#pragma region Receive More Data
                    {
                        const int received{ recv(data.socket,
                                           reinterpret_cast<char*>(data.decryptedData.data() + receivedBytes),
                                           data.decryptedData.size() - receivedBytes, 0) };
                        receivedBytes += received;

                        if (received <= 0)
                            return ClearAndExit();

                        if (status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE)
                            continue;
                    }

#pragma endregion
                    break;
                case EncryptStatusEnum::ERR_OK: {
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

                    if (status != EncryptStatusEnum::ERR_OK)
                        return ClearAndExit();

                    data.isHandshakeComplete = true;
                    data.isServer = false;

                    return {};
                }

                case EncryptStatusEnum::ERR_INCOMPLETE_CREDENTIALS:
                    return ClearAndExit(ConnectionErrorEnum::CERTIFICATE_ERROR);

                // ----------------------------------------------------------------------
                //                  Errors
                // ----------------------------------------------------------------------

                case EncryptStatusEnum::ERR_INSUFFICIENT_MEMORY:
                case EncryptStatusEnum::ERR_INVALID_HANDLE:
                case EncryptStatusEnum::ERR_INVALID_TOKEN:
                    printf("Token inválido!");
                    return ClearAndExit();
                case EncryptStatusEnum::ERR_LOGON_DENIED:
                case EncryptStatusEnum::ERR_NO_CREDENTIALS:
                case EncryptStatusEnum::ERR_NO_AUTHENTICATING_AUTHORITY:

                    return ClearAndExit(ConnectionErrorEnum::CERTIFICATE_ERROR);

                case EncryptStatusEnum::ERR_INTERNAL_ERROR:
                case EncryptStatusEnum::ERR_WRONG_PRINCIPAL:
                case EncryptStatusEnum::ERR_TARGET_UNKNOWN:
                case EncryptStatusEnum::ERR_UNSUPPORTED_FUNCTION:
                case EncryptStatusEnum::ERR_APPLICATION_PROTOCOL_MISMATCH:
                default:
                    return ClearAndExit(ConnectionErrorEnum::UNKNOWN);
            }


        } while (status == EncryptStatusEnum::INFO_CONTINUE_NEEDED ||
                 status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE);

        return ClearAndExit();
    }

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Close(Data& data) {
        if (data.socket == macroINVALID_SOCKET)
            return {};

        if (data.isHandshakeComplete) {
            DWORD dwType = SCHANNEL_SHUTDOWN;

            SecBuffer outBuffer{
                sizeof(dwType),
                _tul(SecurityBufferEnum::TOKEN),
                outBuffer.pvBuffer = &dwType
            };

            SecBufferDesc outBufferDesc{
                macroSECBUFFER_VERSION,
                1,
                &outBuffer
            };

            SECURITY_STATUS status = ApplyControlToken(&data.ctxtHandle, &outBufferDesc);

            if (status == EncryptStatusEnum::ERR_OK) {
                outBuffer = SecBuffer{
                    0,
                    _tul(SecurityBufferEnum::TOKEN),
                    nullptr
                };

                outBufferDesc = SecBufferDesc{
                    macroSECBUFFER_VERSION,
                    1,
                    &outBuffer
                };

                constexpr auto dwSSPIFlags =
                    InitializeSecurityContextFlags::SEQUENCE_DETECT |
                    InitializeSecurityContextFlags::REPLAY_DETECT   |
                    InitializeSecurityContextFlags::CONFIDENTIALITY |
                    InitializeSecurityContextFlags::STREAM;

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

                if (status == EncryptStatusEnum::ERR_OK && HasData(outBuffer))
                    send(data.socket, static_cast<const char*>(outBuffer.pvBuffer),
                        outBuffer.cbBuffer, 0);
            }

            DeleteSecurityContext(&data.ctxtHandle);
            data.ctxtHandle = {};
            data.isHandshakeComplete = false;
        }

        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::BOTH));
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;

        return {};
    }
}