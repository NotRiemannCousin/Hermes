#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/_base/Network.hpp>
#include <print>
#ifdef _WIN32
#include <MSWSock.h>
#else
#include <netinet/tcp.h>
#endif

namespace Hermes {
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy<ExecutionContext, Data>::AcceptSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(Data),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        DefaultAsyncAcceptPolicy* m_policy{};
        Data* m_listenData{};
        Data m_serverData{};
        AcceptOptions m_options{};

        template<class Receiver>
        struct OperationState {
            DefaultAsyncAcceptPolicy* m_policy;
            Data* m_listenData;
            Data m_serverData;
            AcceptOptions m_options;
            Receiver m_receiver;
            TransferOperStatus m_status{};
            std::byte m_buffer[2 * (sizeof(sockaddr_storage) + 16)]{};
            socklen_t m_addrLen{ sizeof(sockaddr_storage) };

            OperationState(DefaultAsyncAcceptPolicy* policy, Data* listenData, Data&& serverData, AcceptOptions options, Receiver receiver) :
                m_policy{ policy }, m_listenData{ listenData }, m_serverData{ std::move(serverData) },
                m_options{ options }, m_receiver{ std::move(receiver) } {}

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };

                if (!success) {
                    self->m_policy->Close(self->m_serverData);
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }

                try {
#ifdef _WIN32
                    if (setsockopt(self->m_serverData.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        reinterpret_cast<char*>(&self->m_listenData->socket), sizeof(self->m_listenData->socket)) == macroSOCKET_ERROR) {
                        self->m_policy->Close(self->m_serverData);
                        stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::Unknown);
                        return;
                    }
#else
                    self->m_serverData.socket = static_cast<SocketFd>(static_cast<int>(bytesTransferred));
                    if (self->m_serverData.socket < 0) {
                        stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::ConnectionFailed);
                        return;
                    }
#endif

                    auto* context{ self->m_serverData.executionContext };
                    if (!context || !context->RegisterHandle(reinterpret_cast<SocketHandle>(self->m_serverData.socket))) {
                        self->m_policy->Close(self->m_serverData);
                        stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::NoScheduler);
                        return;
                    }

                    const auto applyOpt{ [&](const int level, const int optName, auto value) {
                        setsockopt(self->m_serverData.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                    } };

                    if constexpr (Data::Type == SocketTypeEnum::Stream) {
                        if (self->m_options.tcpNoDelay) applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);
                    }

                    if (self->m_options.keepAlive)      applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                    if (self->m_options.recvBufferSize) applyOpt(SOL_SOCKET, SO_RCVBUF, self->m_options.recvBufferSize);
                    if (self->m_options.sendBufferSize) applyOpt(SOL_SOCKET, SO_SNDBUF, self->m_options.sendBufferSize);

#ifdef _WIN32
                    sockaddr* localAddr{ nullptr };
                    sockaddr* remoteAddr{ nullptr };
                    int localLen{};
                    int remoteLen{};
                    self->m_policy->extensions.lpfnGetAcceptExSockaddrs(self->m_buffer, 0,
                        sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                        &localAddr, &localLen, &remoteAddr, &remoteLen);
#else
                    sockaddr* remoteAddr{ reinterpret_cast<sockaddr*>(self->m_buffer) };
                    int remoteLen{ static_cast<int>(self->m_addrLen) };
#endif

                    if (remoteAddr) {
                        auto endpointRes{ Data::EndpointType::FromSockAddr(
                            SocketInfoAddr{ *reinterpret_cast<sockaddr_storage*>(remoteAddr), static_cast<size_t>(remoteLen), AddressFamilyEnum{ remoteAddr->sa_family } }) };

                        if (!endpointRes) {
                            self->m_policy->Close(self->m_serverData);
                            stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::InvalidEndpoint);
                            return;
                        }

                        self->m_serverData.endpoint = std::move(*endpointRes);
                    }

                    stdexec::set_value(std::move(self->m_receiver), std::move(self->m_serverData));
                } catch (const std::exception& e) {
                    std::println(stderr, "Exception in Accept Success: {}", e.what());
                    self->m_policy->Close(self->m_serverData);
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::Unknown);
                } catch (...) {
                    std::println(stderr, "Unknown exception in Accept Success");
                    self->m_policy->Close(self->m_serverData);
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::Unknown);
                }
            }

            void start() & noexcept {
                auto* context{ m_listenData->executionContext };
                if (!context) {
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
                    return;
                }

#ifdef _WIN32
                m_serverData.socket = socket(static_cast<int>(Data::Family), static_cast<int>(Data::Type), 0);
                if (m_serverData.socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }
#endif

                m_status          = {};
                m_status.context  = this;
                m_status.callback = IoCallback;

                const IoRequest request{
                    .operation           = IoOperation::Accept,
                    .socket              = m_listenData->socket,
                    .buffer              = m_buffer,
                    .acceptAddress       = reinterpret_cast<sockaddr*>(m_buffer),
                    .acceptAddressLength = &m_addrLen,
                    .acceptedSocket      = m_serverData.socket,
                    .acceptExtensions    = &m_policy->extensions,
                    .status              = &m_status
                };

                if (!context->SubmitIo(request)) {
                    m_policy->Close(m_serverData);
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::ConnectionFailed);
                }
            }
        };
        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) && {
            return { m_policy, m_listenData, std::move(m_serverData), m_options, std::move(r) };
        }
    };

    template<class ExecutionContext, SocketDataConcept Data>
    ConnectionResultOper DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Listen(Data& data, int backlog, ListenOptions options) {
        auto listenerPolicy{ DefaultAcceptPolicy<EndpointType, Type, Family>::Listen(data, backlog, options) };
        if (!listenerPolicy)
            return listenerPolicy;

        auto* context{ data.executionContext };
        if (!context || !context->RegisterHandle(reinterpret_cast<SocketHandle>(data.socket))) {
            Close(data);
            return std::unexpected{ ConnectionErrorEnum::NoScheduler };
        }

        if (auto loaded{ LoadAcceptExtensions(data) }; !loaded) {
            Close(data);
            return loaded;
        }

        return listenerPolicy;
    }

    template<class ExecutionContext, SocketDataConcept Data>
    ConnectionResultOper DefaultAsyncAcceptPolicy<ExecutionContext, Data>::LoadAcceptExtensions(Data& data) noexcept {
#ifdef _WIN32
        extensions = {};
        DWORD bytes{};

        GUID guidAcceptEx = WSAID_ACCEPTEX;
        if (WSAIoctl(data.socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx),
            &extensions.lpfnAcceptEx, sizeof(extensions.lpfnAcceptEx), &bytes, nullptr, nullptr) != 0)
            return std::unexpected{ ConnectionErrorEnum::Unknown };

        GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        if (WSAIoctl(data.socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
            &extensions.lpfnGetAcceptExSockaddrs, sizeof(extensions.lpfnGetAcceptExSockaddrs), &bytes, nullptr, nullptr) != 0)
            return std::unexpected{ ConnectionErrorEnum::Unknown };
#endif
        return {};
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Accept(Data& listenData, Data&& serverData, AcceptOptions options) {
        static_assert(stdexec::sender<AcceptSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<AcceptSender>, std::variant<std::tuple<Data>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<AcceptSender>, std::variant<ConnectionErrorEnum>>);

        return AcceptSender{ this, &listenData, std::move(serverData), options };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Accept(Data& listenData, AcceptOptions options) {
        return Accept(listenData, listenData.MakeChild(), options);
    }

    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy<ExecutionContext, Data>::ShutdownSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data* m_data;

        template<class Receiver>
        struct OperationState {
            Data* m_data;
            Receiver m_receiver;

            void start() & noexcept {
                if (m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_value(std::move(m_receiver));
                    return;
                }

                if (shutdown(m_data->socket, static_cast<int>(SocketShutdownEnum::Send)) == macroSOCKET_ERROR) {
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::SendFailed);
                    return;
                }

                stdexec::set_value(std::move(m_receiver));
            }
        };
        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, std::move(r) };
        }
    };

    template<class ExecutionContext, SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ShutdownSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ShutdownSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ShutdownSender>, std::variant<ConnectionErrorEnum>>);

        return ShutdownSender{ &data };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Close(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Close(data);
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Abort(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Abort(data);
    }
}

#endif
