#pragma once
#include <Hermes/_base/Network.hpp>

namespace Hermes {
    template<SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy<Data>::ConnectSender {
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
#ifndef _WIN32
            TransferOperStatus _status{};
            sockaddr_storage _addr{};
            socklen_t _addrLen{};

            static void IoCallback(void* context, size_t /*bytesTransferred*/, bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    DefaultAsyncConnectPolicy::Close(*self->_data);
                    stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }
                stdexec::set_value(std::move(self->_receiver));
            }
#endif

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                auto addrRes{ self._data->endpoint.ToSockAddr() };
                if (!addrRes.has_value()) {
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                auto [addr, addr_len, addrFamily]{ *addrRes };

                self._data->socket = socket(static_cast<int>(addrFamily), static_cast<int>(Data::Type), 0);
                if (self._data->socket < 0) {
                    self._data->socket = macroINVALID_SOCKET;
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                const auto s_applyOpt = [&](const int level, const int optName, auto value) {
                    setsockopt(self._data->socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                };

                if constexpr (Data::Family == AddressFamilyEnum::Inet6)
                    s_applyOpt(IPPROTO_IPV6, IPV6_V6ONLY, int{ self._options.onlyIpv6 });

                if constexpr (Data::Type == SocketTypeEnum::Stream)
                    if (self._options.tcpNoDelay) s_applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);

                if (self._options.keepAlive)      s_applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                if (self._options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, self._options.recvBufferSize);
                if (self._options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, self._options.sendBufferSize);

                const auto s_setNonBlocking = [&](SocketFd sock) {
#ifdef _WIN32
                    u_long mode{ 1 };
                    ioctlsocket(sock, FIONBIO, &mode);
#else
                    const int flags{ fcntl(sock, F_GETFL, 0) };
                    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
                };

                s_setNonBlocking(self._data->socket);

                auto& sched{ self._options.scheduler };
                if (!sched || !sched->RegisterHandle(reinterpret_cast<SocketHandle>(static_cast<intptr_t>(self._data->socket)))) {
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::NoScheduler);
                    return;
                }

#ifdef _WIN32
                const int res{ connect(self._data->socket, reinterpret_cast<sockaddr*>(&addr), addr_len) };
                if (res == 0) {
                    stdexec::set_value(std::move(self._receiver));
                    return;
                }

                const bool inProgress{ WSAGetLastError() == WSAEWOULDBLOCK };

                if (!inProgress) {
                    DefaultAsyncConnectPolicy::Close(*self._data);
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::ConnectionFailed);
                    return;
                }

                const auto s_fallbackWait = [&]() {
                    fd_set writeSet, errSet;
                    FD_ZERO(&writeSet); FD_ZERO(&errSet);
                    FD_SET(self._data->socket, &writeSet); FD_SET(self._data->socket, &errSet);

                    const int selectRes{ select(static_cast<int>(self._data->socket) + 1, nullptr, &writeSet, &errSet, nullptr) };
                    if (selectRes > 0) {
                        int soError{ 0 };
                        socklen_t len{ sizeof(soError) };
                        getsockopt(self._data->socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);

                        if (soError == 0) {
                            stdexec::set_value(std::move(self._receiver));
                            return;
                        }
                    }

                    DefaultAsyncConnectPolicy::Close(*self._data);
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::ConnectionFailed);
                };

                s_fallbackWait();
#else
                std::memcpy(&self._addr, &addr, addr_len);
                self._addrLen = static_cast<socklen_t>(addr_len);
                self._status = {};
                self._status.context = &self;
                self._status.callback = IoCallback;

                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(self._data->socket));
                if (!loop) {
                    DefaultAsyncConnectPolicy::Close(*self._data);
                    stdexec::set_error(std::move(self._receiver), ConnectionErrorEnum::NoScheduler);
                    return;
                }

                loop->SubmitIo([&](struct io_uring_sqe* sqe) {
                    io_uring_prep_connect(sqe, static_cast<int>(self._data->socket), reinterpret_cast<sockaddr*>(&self._addr), self._addrLen);
                    io_uring_sqe_set_data(sqe, &self._status);
                });
#endif
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const ConnectSender& self, Receiver r) {
            return { self._data, self._options, std::move(r) };
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