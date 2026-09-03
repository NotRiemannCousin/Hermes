#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

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
    DefaultAsyncAcceptPolicy::Close(self REF m_serverData);                            \
    stdexec::set_error(std::move(self REF m_receiver), ConnectionErrorEnum::ERROR);    \
    return;                                                                           \
}
#define CHECK_ERR(COND, ERROR)                                                        \
if (COND) {                                                                           \
    stdexec::set_error(std::move(self REF m_receiver), ConnectionErrorEnum::ERROR);    \
    return;                                                                           \
}


namespace Hermes {

    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy<ExecutionContext, Data>::AcceptSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(Data),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data* m_listenData{};
        Data m_serverData{};
        AcceptOptions m_options{};

        template<class Receiver>
        struct OperationState {
            Data* m_listenData;
            Data m_serverData;
            AcceptOptions m_options;
            Receiver m_receiver;
            TransferOperStatus m_status{};
            std::byte m_buffer[2 * (sizeof(sockaddr_storage) + 16)]{};
#ifndef _WIN32
            socklen_t m_addrLen{ sizeof(sockaddr_storage) };
#endif

            OperationState(Data* listenData, Data&& serverData, AcceptOptions options, Receiver receiver) :
                m_listenData{ listenData }, m_serverData{ std::move(serverData) },
                m_options{ options }, m_receiver{ std::move(receiver) } {}

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {

                auto* self{ static_cast<OperationState*>(context) };

                SAFE_CHECK_ERR(!success, ConnectionFailed);

                try {
#ifdef _WIN32
                    auto acceptCtx{ setsockopt(self->m_serverData.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        reinterpret_cast<char*>(&self->m_listenData->socket), sizeof(self->m_listenData->socket)) };
                    SAFE_CHECK_ERR(acceptCtx == macroSOCKET_ERROR, Unknown);
#else
                    self->m_serverData.socket = static_cast<SocketFd>(static_cast<int>(bytesTransferred));
                    SAFE_CHECK_ERR(self->m_serverData.socket < 0, ConnectionFailed);
#endif

                    auto& sched{ self->m_options.scheduler };
                    SAFE_CHECK_ERR(!sched
                        || !sched->RegisterHandle(reinterpret_cast<SocketHandle>(self->m_serverData.socket)), NoScheduler);
#pragma region Options

                    const auto applyOpt{ [&](const int level, const int optName, auto value) {
                        setsockopt(self->m_serverData.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                    } };

                    if constexpr (Data::Type == SocketTypeEnum::Stream) {
                        if (self->m_options.tcpNoDelay) applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);
                    }

                    if (self->m_options.keepAlive)      applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                    if (self->m_options.recvBufferSize) applyOpt(SOL_SOCKET, SO_RCVBUF, self->m_options.recvBufferSize);
                    if (self->m_options.sendBufferSize) applyOpt(SOL_SOCKET, SO_SNDBUF, self->m_options.sendBufferSize);
#pragma endregion

#ifdef _WIN32
                    const auto& extensions = listenerExtensions.at(self->m_listenData->socket);
                    sockaddr* localAddr{ nullptr };
                    sockaddr* remoteAddr{ nullptr };
                    int localLen{};
                    int remoteLen{};
                    extensions.lpfnGetAcceptExSockaddrs(self->m_buffer, 0,
                        sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                        &localAddr, &localLen, &remoteAddr, &remoteLen);
#else
                    sockaddr* remoteAddr{ reinterpret_cast<sockaddr*>(self->m_buffer) };
                    int remoteLen{ static_cast<int>(self->m_addrLen) };
#endif
                    if (remoteAddr) {
                        auto endpointRes{ Data::EndpointType::FromSockAddr(
                            SocketInfoAddr{ *reinterpret_cast<sockaddr_storage*>(remoteAddr), static_cast<size_t>(remoteLen), AddressFamilyEnum{ remoteAddr->sa_family } }) };
                        SAFE_CHECK_ERR(!endpointRes, InvalidEndpoint);

                        self->m_serverData.endpoint = std::move(*endpointRes);
                    }

                    stdexec::set_value(std::move(self->m_receiver), std::move(self->m_serverData));
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
            void start() & noexcept
            {
                auto& self{ *this };

#ifdef _WIN32
                self.m_serverData.socket = socket(static_cast<int>(Data::Family), static_cast<int>(Data::Type), 0);

                CHECK_ERR(self.m_serverData.socket == macroINVALID_SOCKET, Unknown);

                const auto& extensions = listenerExtensions.at(self.m_listenData->socket);

                self.m_status = {};
                self.m_status.context = &self;
                self.m_status.callback = IoCallback;

                DWORD bytesReceived{};
                const BOOL success = extensions.lpfnAcceptEx(self.m_listenData->socket, self.m_serverData.socket,
                    self.m_buffer, 0,
                    sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                    &bytesReceived, &self.m_status);
                SAFE_CHECK_ERR(!success && WSAGetLastError() != WSA_IO_PENDING, ConnectionFailed);
#else
                self.m_status = {};
                self.m_status.context = &self;
                self.m_status.callback = IoCallback;

                auto* scheduler = self.m_options.scheduler;
                SAFE_CHECK_ERR(!scheduler, NoScheduler);
                scheduler->SubmitIo([&](struct io_uring_sqe* sqe) {
                    self.m_addrLen = sizeof(sockaddr_storage);
                    io_uring_prep_accept(sqe, static_cast<int>(self.m_listenData->socket),
                        reinterpret_cast<sockaddr*>(self.m_buffer), &self.m_addrLen, 0);
                    io_uring_sqe_set_data(sqe, &self.m_status);

                });
#endif
            }
        };
        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) && {
            return { m_listenData, std::move(m_serverData), m_options, std::move(r) };
        }
    };

    template<class ExecutionContext, SocketDataConcept Data>
    ConnectionResultOper DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Listen(Data& data, int backlog, ListenOptions options) {
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
        std::lock_guard lock(listenerExtensionsMutex);
        listenerExtensions[data.socket] = extensions;
#endif

        return listenerPolicy;
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Accept(Data& listenData, Data&& serverData, AcceptOptions options) {
        static_assert(stdexec::sender<AcceptSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<AcceptSender>, std::variant<std::tuple<Data>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<AcceptSender>, std::variant<ConnectionErrorEnum>>);

        return AcceptSender{ &listenData, std::move(serverData), options };
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
                auto& self{ *this };

                if (self.m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_value(std::move(self.m_receiver));
                    return;
                }

                CHECK_ERR(shutdown(self.m_data->socket, static_cast<int>(SocketShutdownEnum::Send)) == macroSOCKET_ERROR, SendFailed);
                stdexec::set_value(std::move(self.m_receiver));
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
#ifdef _WIN32
        std::lock_guard lock(listenerExtensionsMutex);
        listenerExtensions.erase(data.socket);
#else
#endif
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void DefaultAsyncAcceptPolicy<ExecutionContext, Data>::Abort(Data& data) noexcept {
        DefaultAcceptPolicy<EndpointType, Type, Family>::Abort(data);
#ifdef _WIN32
        std::lock_guard lock(listenerExtensionsMutex);
        listenerExtensions.erase(data.socket);
#else
#endif
    }
}

#pragma pop_macro("SAFE_CHECK_ERR")
#pragma pop_macro("SAFE_CHECK_ERR_H")
#pragma pop_macro("CHECK_ERR")
#pragma pop_macro("CHECK_ERROR_H")

#endif