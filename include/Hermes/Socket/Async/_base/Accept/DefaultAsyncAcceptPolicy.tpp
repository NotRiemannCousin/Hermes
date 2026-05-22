#pragma once
#include <Hermes/_base/Network.hpp>
#include <print>
#include <MSWSock.h>


#pragma push_macro("SAFE_CHECK_ERR")
#pragma push_macro("SAFE_CHECK_ERR_H")
#pragma push_macro("CHECK_ERR")
#pragma push_macro("CHECK_ERROR_H")

#undef SUCCESS_WITH
#undef SUCCESS_WITH_H
#undef CHECK_ERR
#undef CHECK_ERROR_H


#define REF ->
#define SAFE_CHECK_ERR(COND, ERROR)                                                   \
if (COND) {                                                                           \
    DefaultAsyncAcceptPolicy::Close(self REF _clientData);                           \
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
            stdexec::set_error_t(ConnectionErrorEnum)
        >;

        Data* _listenData;
        AcceptOptions _options;

        template<class Receiver>
        struct OperationState {
            Data* _listenData;
            Data _clientData;
            AcceptOptions _options;
            Receiver _receiver;
            TransferOperStatus _status{};
            std::byte _buffer[2 * (sizeof(sockaddr_storage) + 16)]{};

            OperationState(Data* listenData, AcceptOptions options, Receiver receiver) :
                _listenData{ listenData }, _options{ options }, _receiver{ std::move(receiver) }, _clientData{ listenData->MakeChild() } {}

            static void IoCallback(void* context, DWORD _, bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };

                SAFE_CHECK_ERR(!success, ConnectionFailed);

                try {
                    auto acceptCtx{ setsockopt(self->_clientData.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        reinterpret_cast<char*>(&self->_listenData->socket), sizeof(self->_listenData->socket)) };

                    SAFE_CHECK_ERR(acceptCtx == macroSOCKET_ERROR, Unknown);


                    auto& sched{ self->_options.scheduler };
                    SAFE_CHECK_ERR(!sched
                        || !sched->RegisterHandle(reinterpret_cast<HANDLE>(self->_clientData.socket)), NoScheduler);

#pragma region Options

                    const auto s_applyOpt = [&](const int level, const int optName, auto value) {
                        setsockopt(self->_clientData.socket, level, optName, reinterpret_cast<const char*>(&value), sizeof(value));
                    };

                    if constexpr (Data::Type == SocketTypeEnum::Stream) {
                        if (self->_options.tcpNoDelay) s_applyOpt(IPPROTO_TCP, TCP_NODELAY, 1);
                    }

                    if (self->_options.keepAlive)      s_applyOpt(SOL_SOCKET, SO_KEEPALIVE, 1);
                    if (self->_options.recvBufferSize) s_applyOpt(SOL_SOCKET, SO_RCVBUF, self->_options.recvBufferSize);
                    if (self->_options.sendBufferSize) s_applyOpt(SOL_SOCKET, SO_SNDBUF, self->_options.sendBufferSize);

#pragma endregion

                    LPFN_GETACCEPTEXSOCKADDRS getAcceptExSockaddrs{ nullptr };
                    auto guidGetAcceptExSockaddrs{ GUID(WSAID_GETACCEPTEXSOCKADDRS) };
                    DWORD bytes{};

                    const auto funcPointerErr{ WSAIoctl(self->_listenData->socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
                        &getAcceptExSockaddrs, sizeof(getAcceptExSockaddrs),
                        &bytes, nullptr, nullptr) };

                    SAFE_CHECK_ERR(funcPointerErr == macroSOCKET_ERROR, Unknown);

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

                        SAFE_CHECK_ERR(!endpointRes, InvalidEndpoint);

                        self->_clientData.endpoint = std::move(*endpointRes);
                    }

                    stdexec::set_value(std::move(self->_receiver), std::move(self->_clientData));

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
                self._clientData.socket = socket(static_cast<int>(Data::Family), static_cast<int>(Data::Type), 0);
                CHECK_ERR(self._clientData.socket == macroINVALID_SOCKET, Unknown);

                LPFN_ACCEPTEX acceptEx{ nullptr };

                auto guidAcceptEx{ GUID(WSAID_ACCEPTEX) };

                DWORD bytes{};


                auto funcPointerErr{ WSAIoctl(self._listenData->socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guidAcceptEx, sizeof(guidAcceptEx),
                    &acceptEx, sizeof(acceptEx),
                    &bytes, nullptr, nullptr) };
                SAFE_CHECK_ERR(funcPointerErr == macroSOCKET_ERROR, Unknown);

                self._status = {};
                self._status.context = &self;
                self._status.callback = IoCallback;

                DWORD bytesReceived{};
                const BOOL success = acceptEx(self._listenData->socket, self._clientData.socket,
                    self._buffer, 0,
                    sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                    &bytesReceived, &self._status);

                SAFE_CHECK_ERR(!success && WSAGetLastError() != WSA_IO_PENDING, ConnectionFailed);
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const AcceptSender& self, Receiver r) {
            return { self._listenData, self._options, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    ConnectionResultOper DefaultAsyncAcceptPolicy<Data>::Listen(Data& data, int backlog, ListenOptions options) {
        auto listenerPolicy{ DefaultAcceptPolicy<EndpointType, Type, Family>::Listen(data, backlog, options) };

        if (!listenerPolicy)
            return listenerPolicy;

        auto sched{ options.scheduler };

        if (!sched || !sched->RegisterHandle(reinterpret_cast<HANDLE>(data.socket)))
            return std::unexpected{ ConnectionErrorEnum::NoScheduler };

        return listenerPolicy;
    }

    template<SocketDataConcept Data>
    auto DefaultAsyncAcceptPolicy<Data>::AsyncAccept(Data& listenData, AcceptOptions options) {
        static_assert(stdexec::sender<AcceptSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<AcceptSender>, std::variant<std::tuple<Data>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<AcceptSender>, std::variant<ConnectionErrorEnum>>);

        return AcceptSender{ &listenData, options };
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
    auto DefaultAsyncAcceptPolicy<Data>::AsyncShutdown(Data& data) {
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


#undef SUCCESS_WITH
#undef SUCCESS_WITH_H
#undef CHECK_ERR
#undef CHECK_ERROR_H
