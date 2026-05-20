#pragma once
#include <Hermes/_base/Network.hpp>
#include <print>
#include <MSWSock.h>

namespace Hermes {

    template<SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy<Data>::AcceptSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum)
        >;

        Data* _listenData;
        Data* _clientData;
        AcceptOptions _options;

        template<class Receiver>
        struct OperationState {
            Data* _listenData;
            Data* _clientData;
            AcceptOptions _options;
            Receiver _receiver;
            TransferOperStatus _status{};
            std::byte _buffer[2 * (sizeof(sockaddr_storage) + 16)]{};

            OperationState(Data* listenData, Data* clientData, AcceptOptions options, Receiver receiver) noexcept :
                _listenData{ listenData }, _clientData{ clientData }, _options{ options }, _receiver{ std::move(receiver) } {}

            static void IoCallback(void* context, DWORD bytesTransferred, bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);

                if (!success) {
                    DefaultAsyncAcceptPolicy::Close(*self->_clientData);
                    stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }

                try {
                    // 1. Update the accept context so setsockopt/getsockname work
                    if (setsockopt(self->_clientData->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        reinterpret_cast<char*>(&self->_listenData->socket), sizeof(self->_listenData->socket)) == macroSOCKET_ERROR) {
                        DefaultAsyncAcceptPolicy::Close(*self->_clientData);
                        stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::Unknown);
                        return;
                    }

                    // 2. Register the new socket with the IOCP scheduler
                    if (self->_options.scheduler) {
                        self->_options.scheduler->RegisterHandle(reinterpret_cast<HANDLE>(self->_clientData->socket));
                    }

                    // 3. Apply socket options
                    const auto s_applyOpt = [&](const int level, const int optName, auto value) {
                        setsockopt(self->_clientData->socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                    };

                    if constexpr (Data::Type == SocketTypeEnum::Stream) {
                        if (self->_options.tcpNoDelay) s_applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);
                    }

                    if (self->_options.keepAlive)      s_applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                    if (self->_options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, self._options.recvBufferSize);
                    if (self->_options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, self._options.sendBufferSize);

                    // 4. Extract remote address
                    LPFN_GETACCEPTEXSOCKADDRS getAcceptExSockaddrs{ nullptr };
                    GUID guidGetAcceptExSockaddrs{ WSAID_GETACCEPTEXSOCKADDRS };
                    DWORD bytes{};

                    if (WSAIoctl(self->_listenData->socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
                        &getAcceptExSockaddrs, sizeof(getAcceptExSockaddrs),
                        &bytes, nullptr, nullptr) == macroSOCKET_ERROR) {
                        DefaultAsyncAcceptPolicy::Close(*self->_clientData);
                        stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::Unknown);
                        return;
                    }

                    sockaddr* localAddr{ nullptr };
                    sockaddr* remoteAddr{ nullptr };
                    int localLen{};
                    int remoteLen{};

                    getAcceptExSockaddrs(self->_buffer, 0,
                        sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                        &localAddr, &localLen, &remoteAddr, &remoteLen);

                    if (remoteAddr) {
                        auto endpointRes{ Data::EndpointType::FromSockAddr(
                            SocketInfoAddr{ *reinterpret_cast<sockaddr_storage*>(remoteAddr), static_cast<size_t>(remoteLen), AddressFamilyEnum{ remoteAddr->sa_family } }) };

                        if (endpointRes) {
                            self->_clientData->endpoint = std::move(*endpointRes);
                        } else {
                            DefaultAsyncAcceptPolicy::Close(*self->_clientData);
                            stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::InvalidEndpoint);
                            return;
                        }
                    }

                    stdexec::set_value(std::move(self->_receiver));

                } catch (const std::exception& e) {
                    std::println(stderr, "Exception in Accept Success: {}", e.what());
                    DefaultAsyncAcceptPolicy::Close(*self->_clientData);
                    stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::Unknown);
                } catch (...) {
                    std::println(stderr, "Unknown exception in Accept Success");
                    DefaultAsyncAcceptPolicy::Close(*self->_clientData);
                    stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::Unknown);
                }
            }

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                // Pre-create the client socket
                self._clientData->socket = socket(static_cast<int>(Data::Family), static_cast<int>(Data::Type), 0);
                if (self._clientData->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                LPFN_ACCEPTEX acceptEx{ nullptr };
                GUID guidAcceptEx{ WSAID_ACCEPTEX };
                DWORD bytes{};

                // Load AcceptEx
                if (WSAIoctl(self._listenData->socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidAcceptEx, sizeof(guidAcceptEx),
                    &acceptEx, sizeof(acceptEx),
                    &bytes, nullptr, nullptr) == macroSOCKET_ERROR) {
                    DefaultAsyncAcceptPolicy::Close(*self._clientData);
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                self._status = {};
                self._status.context = &self;
                self._status.callback = IoCallback;

                DWORD bytesReceived{};
                const BOOL success = acceptEx(self._listenData->socket, self._clientData->socket,
                    self._buffer, 0, // No receive data, just accept
                    sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                    &bytesReceived, &self._status);

                if (!success && WSAGetLastError() != WSA_IO_PENDING) {
                    DefaultAsyncAcceptPolicy::Close(*self._clientData);
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::ConnectionFailed);
                }
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const AcceptSender& self, Receiver r) {
            return { self._listenData, self._clientData, self._options, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    ConnectionResultOper DefaultAsyncAcceptPolicy<Data>::Listen(Data& data, int backlog, ListenOptions options) noexcept {
        return DefaultAcceptPolicy<EndpointType, Type, Family>::Listen(data, backlog, options);
    }

    template<SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<Data>::AsyncAccept(Data& listenData, Data& clientData, AcceptOptions options) noexcept {
        static_assert(stdexec::sender<AcceptSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<AcceptSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<AcceptSender>, std::variant<ConnectionErrorEnum>>);

        return AcceptSender{ &listenData, &clientData, options };
    }


    template<SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy<Data>::ShutdownSender {
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
                if (self._data->socket == macroINVALID_SOCKET) {
                    stdexec::set_value(std::move(self._receiver));
                    return;
                }

                if (shutdown(self._data->socket, static_cast<int>(SocketShutdownEnum::Send)) == macroSOCKET_ERROR) {
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
    auto DefaultAsyncAcceptPolicy<Data>::AsyncShutdown(Data& data) noexcept {
        static_assert(stdexec::sender<ShutdownSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ShutdownSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ShutdownSender>, std::variant<ConnectionErrorEnum>>);

        return ShutdownSender{ &data };
    }



    template<SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<Data>::Close(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Close(data);
    }

    template<SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<Data>::Abort(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Abort(data);
    }
}