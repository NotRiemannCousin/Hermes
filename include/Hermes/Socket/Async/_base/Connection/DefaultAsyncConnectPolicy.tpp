#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/_base/Network.hpp>

#include <cstring>

namespace Hermes {
    template<SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy<Data>::ConnectSender {
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
#ifndef _WIN32
            TransferOperStatus m_status{};
            sockaddr_storage m_addr{};
            socklen_t m_addrLen{};

            static void IoCallback(void* context, size_t /*bytesTransferred*/, bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    DefaultAsyncConnectPolicy::Close(*self->m_data);
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }
                stdexec::set_value(std::move(self->m_receiver));
            }
#endif

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

                auto& sched{ m_options.scheduler };
                if (!sched || !sched->RegisterHandle(reinterpret_cast<SocketHandle>(static_cast<intptr_t>(m_data->socket)))) {
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
                    return;
                }

#ifdef _WIN32
                const int res{ ::connect(m_data->socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };
                if (res == 0) {
                    stdexec::set_value(std::move(m_receiver));
                    return;
                }

                const bool inProgress{ WSAGetLastError() == WSAEWOULDBLOCK };
                if (!inProgress) {
                    DefaultAsyncConnectPolicy::Close(*m_data);
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }

                const auto fallbackWait{ [&]() {
                    fd_set writeSet, errSet;
                    FD_ZERO(&writeSet); FD_ZERO(&errSet);
                    FD_SET(m_data->socket, &writeSet); FD_SET(m_data->socket, &errSet);

                    const int selectRes{ select(static_cast<int>(m_data->socket) + 1, nullptr, &writeSet, &errSet, nullptr) };
                    if (selectRes > 0) {
                        int soError{ 0 };
                        socklen_t len{ sizeof(soError) };
                        getsockopt(m_data->socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);

                        if (soError == 0) {
                            stdexec::set_value(std::move(m_receiver));
                            return;
                        }
                    }

                    DefaultAsyncConnectPolicy::Close(*m_data);
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::ConnectionFailed);
                } };

                fallbackWait();
#else
                std::memcpy(&m_addr, &addr, addr_len);
                m_addrLen = static_cast<socklen_t>(addr_len);
                m_status = {};
                m_status.context = this;
                m_status.callback = IoCallback;

                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                if (!loop) {
                    DefaultAsyncConnectPolicy::Close(*m_data);
                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
                    return;
                }

                loop->SubmitIo([&](struct io_uring_sqe* sqe) {
                    io_uring_prep_connect(sqe, static_cast<int>(m_data->socket), reinterpret_cast<sockaddr*>(&m_addr), m_addrLen);
                    io_uring_sqe_set_data(sqe, &m_status);
                });
#endif
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, m_options, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy<Data>::ShutdownSender {
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


    template<SocketDataConcept Data>
    auto DefaultAsyncConnectPolicy<Data>::Connect(Data& data, Options options) noexcept {
        static_assert(stdexec::sender<ConnectSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ConnectSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ConnectSender>, std::variant<ConnectionErrorEnum>>);

        return ConnectSender{ &data, options };
    }

    template<SocketDataConcept Data>
    auto DefaultAsyncConnectPolicy<Data>::Shutdown(Data& data) noexcept {
        static_assert(stdexec::sender<ShutdownSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ShutdownSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ShutdownSender>, std::variant<ConnectionErrorEnum>>);

        return ShutdownSender{ &data };
    }

    template<SocketDataConcept Data>
    void DefaultAsyncConnectPolicy<Data>::Close(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            CloseSocket(data.socket);
#ifndef _WIN32
            FastIoLoop::UnregisterSocketLoop(static_cast<int>(data.socket));
#endif
            data.socket = macroINVALID_SOCKET;
        }
    }

    template<SocketDataConcept Data>
    void DefaultAsyncConnectPolicy<Data>::Abort(Data& data) noexcept {
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
#ifndef _WIN32
            FastIoLoop::UnregisterSocketLoop(static_cast<int>(data.socket));
#endif
            data.socket = macroINVALID_SOCKET;
        }
    }
}

#endif