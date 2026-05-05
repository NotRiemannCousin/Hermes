#pragma once
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.tpp>
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/Macros.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    struct TlsAsyncConnectPolicy<Data>::HandshakeSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;

        Data* _data;
        Options _options;

        template<class Receiver>
        struct OperationState {
            Data* _data;
            Options _options;
            Receiver _receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                Data& data = *self._data;
                const Options& options = self._options;

#pragma region fast-fail and lambdas

                if (data.credentials == nullptr) data.credentials = &Network::GetClientCredentials();
                if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();

                if (data.host.size() >= 255) {
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::HandshakeFailed);
                    return;
                }

                static constexpr auto s_hasData = [](const SecBuffer buffer) {
                    return buffer.cbBuffer > 0;
                };

                const auto s_clearAndExit = [&](ConnectionErrorEnum error = ConnectionErrorEnum::HandshakeFailed) {
                    DeleteSecurityContext(&data.ctxtHandle);
                    data.ctxtHandle = {};

                    TlsAsyncConnectPolicy::Close(data);
                    stdexec::set_error(std::move(self._receiver), error);
                };

                // HACK: It's blocking while there is no event loop scheduler.
                const auto s_asyncRecv = [&](char* buf, int len) {
                    fd_set fd; FD_ZERO(&fd); FD_SET(data.socket, &fd);
                    select(static_cast<int>(data.socket) + 1, &fd, nullptr, nullptr, nullptr);
                    return recv(data.socket, buf, len, 0);
                };

                const auto s_asyncSend = [&](const char* buf, int len) {
                    fd_set fd; FD_ZERO(&fd); FD_SET(data.socket, &fd);
                    select(static_cast<int>(data.socket) + 1, nullptr, &fd, nullptr, nullptr);
                    return send(data.socket, buf, len, 0);
                };

#pragma endregion

#pragma region buffers

                std::array<std::array<std::byte, 0x4000>, 4> buffers{};
                std::array<SecBuffer, 4> secBuffers{};

                auto& tokenBuffer{ secBuffers[0] };
                auto& extraBuffer{ secBuffers[1] };

                auto& outBuffer{ secBuffers[2] };
                auto& msgBuffer{ secBuffers[3] };


                SecBufferDesc inBufferDesc{ SecBufferDesc{ macroSECBUFFER_VERSION, 2, &tokenBuffer } };
                SecBufferDesc outBufferDesc{ SecBufferDesc{ macroSECBUFFER_VERSION, 2, &outBuffer } };
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
                                    extraBuffer.cbBuffer);
                        receivedBytes = extraBuffer.cbBuffer;
                    }

                    switch (static_cast<EncryptStatusEnum>(status)) {
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
                                const int sent{ s_asyncSend(static_cast<const char*>(outBuffer.pvBuffer), outBuffer.cbBuffer) };

                                if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer) {
                                    s_clearAndExit(ConnectionErrorEnum::HandshakeFailed);
                                    return;
                                }
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
                                const int received{ s_asyncRecv(
                                                   reinterpret_cast<char*>(data.state->decryptedData.data() + receivedBytes),
                                                   static_cast<int>(data.state->decryptedData.size() - receivedBytes)) };
                                receivedBytes += received;

                                if (received <= 0) {
                                    s_clearAndExit();
                                    return;
                                }

                                if (status == EncryptStatusEnum::ErrIncompleteMessage)
                                    continue;
                            }

#pragma endregion
                            break;
                        case EncryptStatusEnum::ErrOk: {
                            if (s_hasData(outBuffer)) {
                                int sent{ s_asyncSend(static_cast<const char*>(outBuffer.pvBuffer), outBuffer.cbBuffer) };

                                if (sent == macroSOCKET_ERROR || sent != outBuffer.cbBuffer) {
                                    s_clearAndExit();
                                    return;
                                }
                            }


                            status = QueryContextAttributesA(&data.ctxtHandle,
                                                            macroSECPKG_ATTR_STREAM_SIZES,
                                                            &data.contextStreamSizes);

                            if (status != EncryptStatusEnum::ErrOk) {
                                s_clearAndExit();
                                return;
                            }

                            data.isHandshakeComplete = true;
                            data.isServer            = false;

                            std::size_t extraBufferSize{
                                s_hasData(extraBuffer) && extraBuffer.BufferType == SecurityBufferEnum::Extra
                                    ? extraBuffer.cbBuffer
                                    : 0
                            };

                            data.state->decryptedExtraSpan = std::span<std::byte>{
                                data.state->decryptedData.data(),
                                extraBufferSize
                            };

                            stdexec::set_value(std::move(self._receiver));
                            return;
                        }

                        case EncryptStatusEnum::ErrIncompleteCredentials:
                            s_clearAndExit(ConnectionErrorEnum::CertificateError);
                            return;

                        case EncryptStatusEnum::ErrInsufficientMemory:
                        case EncryptStatusEnum::ErrInvalidHandle:
                        case EncryptStatusEnum::ErrInvalidToken:
                            s_clearAndExit();
                            return;
                        case EncryptStatusEnum::ErrLogonDenied:
                        case EncryptStatusEnum::ErrNoCredentials:
                        case EncryptStatusEnum::ErrUntrustedRoot:
                        case EncryptStatusEnum::ErrNoAuthenticatingAuthority:
                            s_clearAndExit(ConnectionErrorEnum::CertificateError);
                            return;

                        case EncryptStatusEnum::ErrInternalError:
                        case EncryptStatusEnum::ErrWrongPrincipal:
                        case EncryptStatusEnum::ErrTargetUnknown:
                        case EncryptStatusEnum::ErrUnsupportedFunction:
                        case EncryptStatusEnum::ErrApplicationProtocolMismatch:
                        default:
                            s_clearAndExit(ConnectionErrorEnum::Unknown);
                            return;
                    }

                } while (status == EncryptStatusEnum::InfoContinueNeeded ||
                         status == EncryptStatusEnum::ErrIncompleteMessage);

                s_clearAndExit();
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const HandshakeSender& self, Receiver r) {
            return { self._data, self._options, std::move(r) };
        }
    };


    template<SocketDataConcept Data>
    struct TlsAsyncConnectPolicy<Data>::ShutdownSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;

        Data* _data;

        template<class Receiver>
        struct OperationState {
            Data* _data;
            Receiver _receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                Data& data = *self._data;

                if (data.socket == macroINVALID_SOCKET) {
                    stdexec::set_value(std::move(self._receiver));
                    return;
                }

                if (data.isHandshakeComplete) {
                    DWORD dwType{ macroSCHANNEL_SHUTDOWN };

                    SecBuffer outBuffer{ sizeof(dwType), _tul(SecurityBufferEnum::Token), &dwType };
                    SecBufferDesc outBufferDesc{ macroSECBUFFER_VERSION, 1, &outBuffer };

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
                            nullptr, &data.ctxtHandle, nullptr, _tl(dwSspiFlags),
                            0, 0, nullptr, 0, nullptr,
                            &outBufferDesc, &dwSSPIOutFlags, &tsExpiry
                        );

                        if (status == EncryptStatusEnum::ErrOk && outBuffer.cbBuffer > 0) {
                            fd_set fd; FD_ZERO(&fd); FD_SET(data.socket, &fd);
                            select(static_cast<int>(data.socket) + 1, nullptr, &fd, nullptr, nullptr);
                            send(data.socket, static_cast<const char*>(outBuffer.pvBuffer), outBuffer.cbBuffer, 0);
                        }
                    }

                    DeleteSecurityContext(&data.ctxtHandle);
                    data.ctxtHandle = {};
                    data.isHandshakeComplete = false;
                }

                if (shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send)) == macroSOCKET_ERROR) {
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::SendFailed);
                    return;
                }

                stdexec::set_value(std::move(self._receiver));
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const ShutdownSender& self, Receiver r) {
            return { self._data, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    auto TlsAsyncConnectPolicy<Data>::AsyncConnect(Data& data, Options options) noexcept {
        return DefaultAsyncConnectPolicy<Data>::AsyncConnect(data, options)
             | stdexec::let_value([&data, options]() mutable {
                   data.handshakeCallback = [options](Data& d) {
                       // TODO: implement renegotiation
                       // 1. Criamos a Option Síncrona correspondente.
                       typename TlsConnectPolicy<Data>::Options syncOptions{};

                       // 2. Mapeamos os campos específicos do TLS.
                       syncOptions.ignoreCertificateErrors = options.ignoreCertificateErrors;
                       syncOptions.requestMutualAuth       = options.requestMutualAuth;

                       // (Opcional) Se houver campos herdados da classe base (DefaultConnectPolicy),
                       // mapeie-os aqui também. Por exemplo:
                       // syncOptions.onlyIpv6 = options.onlyIpv6;
                       // syncOptions.connectionTimeout = options.connectionTimeout;

                       // 3. Chamamos o Handshake bloqueante com o tipo correto.
                       return TlsConnectPolicy<Data>::ClientHandshake(d, std::move(syncOptions));
                   };

                   return HandshakeSender{ &data, options };
               });
    }

    template<SocketDataConcept Data>
    auto TlsAsyncConnectPolicy<Data>::AsyncShutdown(Data& data) noexcept {
        return ShutdownSender{ &data };
    }

    template<SocketDataConcept Data>
    void TlsAsyncConnectPolicy<Data>::Close(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }

    template<SocketDataConcept Data>
    void TlsAsyncConnectPolicy<Data>::Abort(Data &data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            constexpr linger lingerOption{ 1, 0 };
            setsockopt(data.socket, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lingerOption), sizeof(lingerOption));
            closesocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }
}