#pragma once
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.tpp>
#include <Hermes/_base/OsApi/Macros.hpp>
#include <Hermes/_base/Network.hpp>

namespace Hermes {

    enum class ControlAction : std::uint8_t { Connect, Renegotiate, Shutdown };
    template<SocketDataConcept Data>
    struct TlsAsyncConnectPolicy<Data>::ControlSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data* _data;
        Options _options;
        ControlAction _action;

        template<class Receiver>
        struct OperationState {
        private:
            Data* _data;
            Options _options;
            Receiver _receiver;
            TransferOperStatus _status;
            ControlAction _action;

            LongIoCount flags{};
        public:

            OperationState(Data* data, Options options, Receiver receiver, const ControlAction action = ControlAction::Connect) :
                _data{ data }, _options{ options }, _receiver{ std::move(receiver) },
                _status{}, _action{ action } {}

            void Pump() noexcept {
                flags = 0;

                while (true) {
                    using ConnectStateOpResult = _details::ConnectStateOpResult;
                    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
                    switch (_data->connectStateMachine->Advance(*_data)) {
                        case ConnectStateOpResult::Send: {
                            auto buf{ _data->connectStateMachine->GetSendBuffer() };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(const_cast<std::byte*>(buf.data())) };

                            if (WSASend(_data->socket, &wsaBuf, 1, nullptr, flags, &_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(_receiver), ConnectionErrorEnum::Unknown);
                                }
                            }
#else
                            auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(_data->socket));
                            if (!loop) {
                                stdexec::set_error(std::move(_receiver), ConnectionErrorEnum::Unknown);
                                return;
                            }
                            loop->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_send(sqe, static_cast<int>(_data->socket), buf.data(), buf.size(), 0);
                                io_uring_sqe_set_data(sqe, &_status);
                            });
#endif
                            return;
                        }
                        case ConnectStateOpResult::Recv: {
                            auto buf{ _data->connectStateMachine->GetRecvBuffer(*_data) };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(buf.data()) };

                            if (WSARecv(_data->socket, &wsaBuf, 1, nullptr, &flags, &_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    auto sla = WSAGetLastError;
                                    stdexec::set_error(std::move(_receiver), ConnectionErrorEnum::Unknown);
                                }
                            }
#else
                            auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(_data->socket));
                            if (!loop) {
                                stdexec::set_error(std::move(_receiver), ConnectionErrorEnum::Unknown);
                                return;
                            }
                            loop->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_recv(sqe, static_cast<int>(_data->socket), buf.data(), buf.size(), 0);
                                io_uring_sqe_set_data(sqe, &_status);
                            });
#endif
                            return;
                        }
                        case ConnectStateOpResult::Done:
                            stdexec::set_value(std::move(_receiver));
                            return;
                        case ConnectStateOpResult::Error:
                            stdexec::set_error(std::move(_receiver), _data->connectStateMachine->GetResult().error_or(ConnectionErrorEnum::Unknown));
                            return;
                        case ConnectStateOpResult::Closed:
                            stdexec::set_error(std::move(_receiver), ConnectionErrorEnum::Unknown);
                            return;
                    }
                }
            }

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);
                if (!success) {
                    // TODO: FUTURE: Map the error
                    stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                self->_data->connectStateMachine->SetIoResult(bytesTransferred);

                self->Pump();
            }

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                self._status.context = &self;
                self._status.callback = IoCallback;

                if (self._action == ControlAction::Shutdown)
                    self._data->connectStateMachine->SetToClose();
                else
                    self._data->connectStateMachine->SetToOpen();
                self.Pump();
            }
        };
        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const ControlSender& self, Receiver r) {
            return { self._data, self._options, std::move(r), self._action };
        }
    };

    template<SocketDataConcept Data>
    auto TlsAsyncConnectPolicy<Data>::Connect(Data& data, Options options) {
        data.connectStateMachine = std::make_unique<_details::TlsConnectStateMachine<Data, TlsAsyncConnectPolicy>>(options);
        _options = options;

        static_assert(stdexec::sender<ControlSender>);

        return DefaultAsyncConnectPolicy<Data>::Connect(data, *reinterpret_cast<DefaultAsyncConnectPolicy<Data>::Options*>(&options))
             |
             stdexec::let_value(Utils::Overloaded{
                 [&data, options]() mutable { return ControlSender{ &data, options }; },
                 [](ConnectionErrorEnum e)  { return stdexec::just_error(e); }
             });
    }

    template<SocketDataConcept Data>
    auto TlsAsyncConnectPolicy<Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ControlSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ControlSender>, std::variant<ConnectionErrorEnum>>);

        return ControlSender{ &data, _options, ControlAction::Shutdown };
    }

    template<SocketDataConcept Data>
    void TlsAsyncConnectPolicy<Data>::Close(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }

    template<SocketDataConcept Data>
    void TlsAsyncConnectPolicy<Data>::Abort(Data &data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            constexpr linger lingerOption{ 1, 0 };
            setsockopt(data.socket, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lingerOption), sizeof(lingerOption));
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }
}