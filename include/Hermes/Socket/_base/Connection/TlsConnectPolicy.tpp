#pragma once
#include <Hermes/_base/Network.hpp>

namespace Hermes {
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
    std::span secBuffers{ data.secBuffers };
    std::span outBuffers{ secBuffers.subspan(0, 2) };
    std::span inBuffers { secBuffers.subspan(2, 2) };

    constexpr auto dwSspiFlags{
        InitializeSecurityContextFlags::SEQUENCE_DETECT |
        InitializeSecurityContextFlags::REPLAY_DETECT   |
        InitializeSecurityContextFlags::CONFIDENTIALITY |
        InitializeSecurityContextFlags::EXTENDED_ERROR  |
        InitializeSecurityContextFlags::STREAM };

    DWORD pfContextAttr{};
    const std::wstring sla{ L"api.discogs.com" };

    bool firstPass{ true };
    DWORD receivedBytes{};
    SECURITY_STATUS status{};

    do {
        PSecBufferDesc pInBufferDesc = nullptr;
        if (!firstPass && receivedBytes > 0) {
            inBuffers[0] = SecBuffer{
                receivedBytes,
                _tul(SecurityBufferEnum::TOKEN),
                data.decryptedData.data()
            };
            inBuffers[1] = SecBuffer{
                0,
                _tul(SecurityBufferEnum::EMPTY),
                nullptr
            };

            inBufferDesc = SecBufferDesc{
                macroSECBUFFER_VERSION,
                2,
                &inBuffers[0]
            };
            pInBufferDesc = &inBufferDesc;
        }

        outBuffers[0] = SecBuffer{
            _tul(buffers[0].size()),
            _tul(SecurityBufferEnum::TOKEN),
            buffers[0].data()
        };
        outBufferDesc = SecBufferDesc{
            macroSECBUFFER_VERSION,
            1,
            &outBuffers[0]
        };

        status = InitializeSecurityContextW(
            const_cast<PCredHandle>(&credHandle),
            firstPass ? nullptr : &data.ctxtHandle,
            const_cast<SEC_WCHAR*>(sla.c_str()),
            _tll(dwSspiFlags),
            0, 0,
            pInBufferDesc,
            0,
            firstPass ? &data.ctxtHandle : nullptr,
            &outBufferDesc,
            &pfContextAttr,
            &tsExpiry
        );

        firstPass = false;

        if (status == _tl(EncryptStatusEnum::ERR_OK)) {
            if (outBuffers[0].cbBuffer > 0 && outBuffers[0].pvBuffer != nullptr) {
                int sent = send(data.socket,
                               static_cast<const char*>(outBuffers[0].pvBuffer),
                               outBuffers[0].cbBuffer, 0);

                if (sent == macroSOCKET_ERROR || sent != _tl(outBuffers[0].cbBuffer)) {
                    DeleteSecurityContext(&data.ctxtHandle);
                    closesocket(data.socket);
                    data.socket = macroINVALID_SOCKET;
                    return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
                }
            }


            status = QueryContextAttributesW(&data.ctxtHandle,
                                            SECPKG_ATTR_STREAM_SIZES,
                                            &data.contextStreamSizes);

            if (status != EncryptStatusEnum::ERR_OK) {
                DeleteSecurityContext(&data.ctxtHandle);
                closesocket(data.socket);
                data.socket = macroINVALID_SOCKET;
                return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
            }

            data.isHandshakeComplete = true;
            data.isServer = false;

            return {};
        }


        if (status != EncryptStatusEnum::INFO_CONTINUE_NEEDED &&
            status != EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE) {
            DeleteSecurityContext(&data.ctxtHandle);
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
        }

        if (outBuffers[0].cbBuffer > 0 && outBuffers[0].pvBuffer != nullptr) {
            int sent = send(data.socket,
                           static_cast<const char*>(outBuffers[0].pvBuffer),
                           outBuffers[0].cbBuffer, 0);

            if (sent == macroSOCKET_ERROR || sent != _tl(outBuffers[0].cbBuffer)) {
                DeleteSecurityContext(&data.ctxtHandle);
                closesocket(data.socket);
                data.socket = macroINVALID_SOCKET;
                return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
            }
        }

        if (status == EncryptStatusEnum::INFO_CONTINUE_NEEDED ||
            status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE) {

            const auto offset{ status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE ? receivedBytes : 0ULL };
            int received = recv(data.socket,
                               reinterpret_cast<char*>(data.decryptedData.data() + offset),
                               _tl(data.decryptedData.size() - offset), 0);

            if (received <= 0) {
                DeleteSecurityContext(&data.ctxtHandle);
                closesocket(data.socket);
                data.socket = macroINVALID_SOCKET;
                return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
            }

            if (status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE) {
                receivedBytes += received;
            } else {
                receivedBytes = received;
            }

            if (pInBufferDesc && inBuffers[1].BufferType == SecurityBufferEnum::EXTRA) {
                std::memmove(data.decryptedData.data(),
                            data.decryptedData.data() + (receivedBytes - inBuffers[1].cbBuffer),
                            inBuffers[1].cbBuffer);
                receivedBytes = inBuffers[1].cbBuffer;
            }
        }

    } while (status == EncryptStatusEnum::INFO_CONTINUE_NEEDED ||
             status == EncryptStatusEnum::ERR_INCOMPLETE_MESSAGE);

    DeleteSecurityContext(&data.ctxtHandle);
    closesocket(data.socket);
    data.socket = macroINVALID_SOCKET;
    return std::unexpected{ ConnectionErrorEnum::HANDSHAKE_FAILED };
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

                status = InitializeSecurityContextW(
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

                if (status == EncryptStatusEnum::ERR_OK && outBuffer.cbBuffer > 0 && outBuffer.pvBuffer != nullptr)
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