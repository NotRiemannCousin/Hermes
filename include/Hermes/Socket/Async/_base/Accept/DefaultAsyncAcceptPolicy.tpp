#pragma once
#include <list>
#include <Hermes/_base/Network.hpp>
#include <print>
#ifdef _WIN32
#include <MSWSock.h>
#else
#include <netinet/tcp.h>
#endif


#pragma push_macro("SAFE_CHECK_ERR")
#pragma push_macro("SAFE_CHECK_ERR_H")
#pragma push_macro("CHECK_ERR")
#pragma push_macro("CHECK_ERROR_H")
#pragma push_macro("REF")

#undef SUCCESS_WITH
#undef SUCCESS_WITH_H
#undef CHECK_ERR
#undef CHECK_ERROR_H
#undef REF


#define REF ->
#define SAFE_CHECK_ERR(COND, ERROR)                                                   \
if (COND) {                                                                           \
    DefaultAsyncAcceptPolicy::Close(self REF _serverData);                            \
    stdexec::set_error(std::move(self REF _receiver), ConnectionErrorEnum::ERROR);    \
    return;                                                                           \
}
#define CHECK_ERR(COND, ERROR)                                                        \
if (COND) {                                                                           \
    stdexec::set_error(std::move(self REF _receiver), ConnectionErrorEnum::ERROR);    \
    return;                                                                           \
}


namespace Hermes {

    template<SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy<Data>::AcceptSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(Data),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data* _listenData{};
        Data _serverData{};
        AcceptOptions _options{};

        template<class Receiver>
        struct OperationState {
            Data* _listenData;
            Data _serverData;
            AcceptOptions _options;
            Receiver _receiver;
            TransferOperStatus _status{};
            std::byte _buffer[2 * (sizeof(sockaddr_storage) + 16)]{};
#ifndef _WIN32
            socklen_t _addrLen{ sizeof(sockaddr_storage) };
#endif

            OperationState(Data* listenData, Data&& serverData, AcceptOptions options, Receiver receiver) :
                _listenData{ listenData }, _serverData{ std::move(serverData) },
                _options{ options }, _receiver{ std::move(receiver) } {}

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {

                auto* self{ static_cast<OperationState*>(context) };

                SAFE_CHECK_ERR(!success, ConnectionFailed);

                try {
#ifdef _WIN32
                    auto acceptCtx{ setsockopt(self->_serverData.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        reinterpret_cast<char*>(&self->_listenData->socket), sizeof(self->_listenData->socket)) };
                    SAFE_CHECK_ERR(acceptCtx == macroSOCKET_ERROR, Unknown);
#else
                    self->_serverData.socket = static_cast<SocketFd>(static_cast<int>(bytesTransferred));
                    SAFE_CHECK_ERR(self->_serverData.socket < 0, ConnectionFailed);
#endif

                    auto& sched{ self->_options.scheduler };
                    SAFE_CHECK_ERR(!sched
                        || !sched->RegisterHandle(reinterpret_cast<SocketHandle>(self->_serverData.socket)), NoScheduler);
#pragma region Options

                    const auto s_applyOpt = [&](const int level, const int optName, auto value) {
                        setsockopt(self->_serverData.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                    };

                    if constexpr (Data::Type == SocketTypeEnum::Stream) {
                        if (self->_options.tcpNoDelay) s_applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);
                    }

                    if (self->_options.keepAlive)      s_applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                    if (self->_options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, self->_options.recvBufferSize);
                    if (self->_options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, self->_options.sendBufferSize);
#pragma endregion

#ifdef _WIN32
                    const auto& extensions = s_listenerExtensions.at(self->_listenData->socket);
                    sockaddr* localAddr{ nullptr };
                    sockaddr* remoteAddr{ nullptr };
                    int localLen{};
                    int remoteLen{};
                    extensions.lpfnGetAcceptExSockaddrs(self->_buffer, 0,
                        sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                        &localAddr, &localLen, &remoteAddr, &remoteLen);
#else
                    sockaddr* remoteAddr{ reinterpret_cast<sockaddr*>(self->_buffer) };
                    int remoteLen{ static_cast<int>(self->_addrLen) };
#endif
                    if (remoteAddr) {
                        auto endpointRes{ Data::EndpointType::FromSockAddr(
                            SocketInfoAddr{ *reinterpret_cast<sockaddr_storage*>(remoteAddr), static_cast<size_t>(remoteLen), AddressFamilyEnum{ remoteAddr->sa_family } }) };
                        SAFE_CHECK_ERR(!endpointRes, InvalidEndpoint);

                        self->_serverData.endpoint = std::move(*endpointRes);
                    }

                    stdexec::set_value(std::move(self->_receiver), std::move(self->_serverData));
                } catch (const std::exception& e) {
                    std::println(stderr, "Exception in Accept Success: {}", e.what());
                    SAFE_CHECK_ERR(true, Unknown);
                } catch (...) {
                    std::println(stderr, "Unknown exception in Accept Success");
                    SAFE_CHECK_ERR(true, Unknown);
                }
            }

#undef REF
#define REF .
            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
#ifdef _WIN32
                self._serverData.socket = socket(static_cast<int>(Data::Family), static_cast<int>(Data::Type), 0);
                CHECK_ERR(self._serverData.socket == macroINVALID_SOCKET, Unknown);

                const auto& extensions = s_listenerExtensions.at(self._listenData->socket);

                self._status = {};
                self._status.context = &self;
                self._status.callback = IoCallback;

                DWORD bytesReceived{};
                const BOOL success = extensions.lpfnAcceptEx(self._listenData->socket, self._serverData.socket,
                    self._buffer, 0,
                    sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                    &bytesReceived, &self._status);
                SAFE_CHECK_ERR(!success && WSAGetLastError() != WSA_IO_PENDING, ConnectionFailed);
#else
                self._status = {};
                self._status.context = &self;
                self._status.callback = IoCallback;

                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(self._listenData->socket));
                SAFE_CHECK_ERR(!loop, NoScheduler);
                loop->SubmitIo([&](struct io_uring_sqe* sqe) {
                    self._addrLen = sizeof(sockaddr_storage);
                    io_uring_prep_accept(sqe, static_cast<int>(self._listenData->socket),
                        reinterpret_cast<sockaddr*>(self._buffer), &self._addrLen, 0);
                    io_uring_sqe_set_data(sqe, &self._status);

                });
#endif
            }
        };
        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, AcceptSender&& self, Receiver r) {
            return { self._listenData, std::move(self._serverData), self._options, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    ConnectionResultOper DefaultAsyncAcceptPolicy<Data>::Listen(Data& data, int backlog, ListenOptions options) {
        auto listenerPolicy{ DefaultAcceptPolicy<EndpointType, Type, Family>::Listen(data, backlog, options) };
        if (!listenerPolicy)
            return listenerPolicy;

        auto sched{ options.scheduler };
        if (!sched || !sched->RegisterHandle(reinterpret_cast<SocketHandle>(data.socket)))
            return std::unexpected{ ConnectionErrorEnum::NoScheduler };
#ifdef _WIN32
        ListenerExtensions extensions;
        DWORD bytes;

        GUID guidAcceptEx = WSAID_ACCEPTEX;
        if (WSAIoctl(data.socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx), &extensions.lpfnAcceptEx, sizeof(extensions.lpfnAcceptEx), &bytes, nullptr, nullptr) != 0)
            return std::unexpected{ ConnectionErrorEnum::Unknown };
        GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        if (WSAIoctl(data.socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs), &extensions.lpfnGetAcceptExSockaddrs, sizeof(extensions.lpfnGetAcceptExSockaddrs), &bytes, nullptr, nullptr) != 0)
            return std::unexpected{ ConnectionErrorEnum::Unknown };
        std::lock_guard lock(s_listenerExtensionsMutex);
        s_listenerExtensions[data.socket] = extensions;
#endif

        return listenerPolicy;
    }

    template<SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<Data>::Accept(Data& listenData, Data&& serverData, AcceptOptions options) {
        static_assert(stdexec::sender<AcceptSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<AcceptSender>, std::variant<std::tuple<Data>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<AcceptSender>, std::variant<ConnectionErrorEnum>>);

        return AcceptSender{ &listenData, std::move(serverData), options };
    }

    template<SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<Data>::Accept(Data& listenData, AcceptOptions options) {
        return Accept(listenData, listenData.MakeChild(), options);
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

                CHECK_ERR(shutdown(self._data->socket, static_cast<int>(SocketShutdownEnum::Send)) == macroSOCKET_ERROR, SendFailed);
                stdexec::set_value(std::move(self._receiver));
            }
        };
        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const ShutdownSender& self, Receiver r) {
            return { self._data, std::move(r) };
        }
    };


    template<SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ShutdownSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ShutdownSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ShutdownSender>, std::variant<ConnectionErrorEnum>>);

        return ShutdownSender{ &data };
    }



    template<SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<Data>::Close(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Close(data);
#ifdef _WIN32
        std::lock_guard lock(s_listenerExtensionsMutex);
        s_listenerExtensions.erase(data.socket);
#else
        FastIoLoop::UnregisterSocketLoop(static_cast<int>(data.socket));
#endif
    }

    template<SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<Data>::Abort(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Abort(data);
#ifdef _WIN32
        std::lock_guard lock(s_listenerExtensionsMutex);
        s_listenerExtensions.erase(data.socket);
#else
        FastIoLoop::UnregisterSocketLoop(static_cast<int>(data.socket));
#endif
    }
}

#pragma pop_macro("SAFE_CHECK_ERR")
#pragma pop_macro("SAFE_CHECK_ERR_H")
#pragma pop_macro("CHECK_ERR")
#pragma pop_macro("CHECK_ERROR_H")