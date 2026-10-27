#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/_base/Network.hpp>

#include <cstring>

namespace Hermes {
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy<ExecutionContext, Data>::ConnectSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;

        Data* m_data;
        Options m_options;

        template<class Receiver>
        struct OperationState {
            Data* m_data;
            Options m_options;
            Receiver m_receiver;
            TransferOperStatus m_status{};
            sockaddr_storage m_addr{};
            socklen_t m_addrLen{};

            static void IoCallback(void* context, LongIoCount /*bytesTransferred*/, bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    DefaultAsyncConnectPolicy::Close(*self->m_data);
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }
                stdexec::set_value(std::move(self->m_receiver));
            }

            void start() & noexcept {
                auto addrRes{ m_data->endpoint.ToSockAddr() };
                if (!addrRes.has_value()) {
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                auto [addr, addr_len, addrFamily]{ *addrRes };
                m_data->socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
                if (m_data->socket < 0) {
                    m_data->socket = macroINVALID_SOCKET;
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                const auto applyOpt{ [&](const int level, const int optName, auto value) {
                    setsockopt(m_data->socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                } };

                if constexpr (Data::Family == AddressFamilyEnum::Inet6)
                    applyOpt(IPPROTO_IPV6, IPV6_V6ONLY, int{ m_options.onlyIpv6 });
                if constexpr (Data::Type == SocketTypeEnum::Stream)
                    if (m_options.tcpNoDelay) applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);
                if (m_options.keepAlive)      applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                if (m_options.recvBufferSize) applyOpt(SOL_SOCKET, SO_RCVBUF, m_options.recvBufferSize);
                if (m_options.sendBufferSize) applyOpt(SOL_SOCKET, SO_SNDBUF, m_options.sendBufferSize);

                const auto setNonBlocking{ [&](SocketFd sock) {
#ifdef _WIN32
                    u_long mode{ 1 };
                    ioctlsocket(sock, FIONBIO, &mode);
#else
                    const int flags{ fcntl(sock, F_GETFL, 0) };
                    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
                } };
                setNonBlocking(m_data->socket);

                auto* context{ m_data->executionContext };
                if (!context || !context->RegisterHandle(reinterpret_cast<SocketHandle>(static_cast<intptr_t>(m_data->socket)))) {
                    DefaultAsyncConnectPolicy::Close(*m_data);
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
                    return;
                }

                std::memcpy(&m_addr, &addr, addr_len);
                m_addrLen         = static_cast<socklen_t>(addr_len);
                m_status          = {};
                m_status.context  = this;
                m_status.callback = IoCallback;

                const IoRequest request{
                    .operation         = IoOperation::Connect,
                    .socket            = m_data->socket,
                    .destination       = reinterpret_cast<const sockaddr*>(&m_addr),
                    .destinationLength = m_addrLen,
                    .status            = &m_status
                };

                if (!context->SubmitIo(request)) {
                    DefaultAsyncConnectPolicy::Close(*m_data);
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::ConnectionFailed);
                }
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, m_options, std::move(r) };
        }
    };

    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy<ExecutionContext, Data>::ShutdownSender {
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
    auto DefaultAsyncConnectPolicy<ExecutionContext, Data>::Connect(Data& data, Options options) noexcept {
        static_assert(stdexec::sender<ConnectSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ConnectSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ConnectSender>, std::variant<ConnectionErrorEnum>>);

        return ConnectSender{ &data, options };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto DefaultAsyncConnectPolicy<ExecutionContext, Data>::Shutdown(Data& data) noexcept {
        static_assert(stdexec::sender<ShutdownSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ShutdownSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ShutdownSender>, std::variant<ConnectionErrorEnum>>);

        return ShutdownSender{ &data };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void DefaultAsyncConnectPolicy<ExecutionContext, Data>::Close(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void DefaultAsyncConnectPolicy<ExecutionContext, Data>::Abort(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            constexpr linger lingerOption{ 1, 0 };
            setsockopt(
                data.socket,
                SOL_SOCKET,
                SO_LINGER,
                reinterpret_cast<const char*>(&lingerOption),
                sizeof(lingerOption)
            );
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }
}

#endif