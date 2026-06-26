#pragma once
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/_base/OsApi/Macros.hpp>
#include <Hermes/_base/Network.hpp>

namespace Hermes {

    enum class AcceptControlAction : std::uint8_t { Accept, Renegotiate, Shutdown };
    template<SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy<Data>::ControlSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data* _data;
        AcceptOptions _options;
        AcceptControlAction _action;

        template<class Receiver>
        struct OperationState {
        private:
            Data* _data;
            AcceptOptions _options;
            Receiver _receiver;
            TransferOperStatus _status;
            AcceptControlAction _action;

            LongIoCount _flags{};
        public:
            OperationState(Data* data, AcceptOptions options, Receiver receiver, const AcceptControlAction action = AcceptControlAction::Accept) :
                _data{ data }, _options{ options }, _receiver{ std::move(receiver) },
                _status{}, _action{ action } {}

            void Pump() noexcept {
                _flags = 0;

                while (true) {
                    using AcceptStateOpResult = _details::AcceptStateOpResult;
                    switch (_data->acceptStateMachine->Advance(*_data)) {
                        case AcceptStateOpResult::Send: {
                            auto buf{ _data->acceptStateMachine->GetSendBuffer() };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(const_cast<std::byte*>(buf.data())) };
                            if (WSASend(_data->socket, &wsaBuf, 1, nullptr, _flags, &_status, nullptr) == macroSOCKET_ERROR) {
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
                        case AcceptStateOpResult::Recv: {
                            auto buf{ _data->acceptStateMachine->GetRecvBuffer(*_data) };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(buf.data()) };
                            if (WSARecv(_data->socket, &wsaBuf, 1, nullptr, &_flags, &_status, nullptr) == macroSOCKET_ERROR) {
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
                                io_uring_prep_recv(sqe, static_cast<int>(_data->socket), buf.data(), buf.size(), 0);

                                 io_uring_sqe_set_data(sqe, &_status);
                            });
#endif
                            return;
                        }
                        case AcceptStateOpResult::Done:
                            stdexec::set_value(std::move(_receiver));
                            return;
                        case AcceptStateOpResult::Error:
                            stdexec::set_error(std::move(_receiver), _data->acceptStateMachine->GetResult().error_or(ConnectionErrorEnum::Unknown));
                            return;
                        case AcceptStateOpResult::Closed:
                            stdexec::set_error(std::move(_receiver), ConnectionErrorEnum::Unknown);
                            return;
                    }
                }
            }

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);
                if (!success) {
                    stdexec::set_error(std::move(self->_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                self->_data->acceptStateMachine->SetIoResult(bytesTransferred);
                self->Pump();
            }

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                self._status.context = &self;
                self._status.callback = IoCallback;

                if (self._action == AcceptControlAction::Shutdown)
                    self._data->acceptStateMachine->SetToClose();
                else
                    self._data->acceptStateMachine->SetToOpen();
                self.Pump();
            }
        };
        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const ControlSender& self, Receiver r) {
            return { self._data, self._options, std::move(r), self._action };
        }
    };

    template<SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy<Data>::AcceptSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(Data),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data _data;
        AcceptOptions _options;

        template<class Receiver>
        struct OperationState {
            Data _data;
            Receiver _receiver;

            struct InnerReceiver {
                using receiver_concept = stdexec::receiver_t;
                OperationState* _parent;

                friend void tag_invoke(stdexec::set_value_t, InnerReceiver&& self) noexcept {
                    stdexec::set_value(std::move(self._parent->_receiver), std::move(self._parent->_data));
                }

                friend void tag_invoke(stdexec::set_error_t, InnerReceiver&& self, ConnectionErrorEnum e) noexcept {
                    stdexec::set_error(std::move(self._parent->_receiver), e);
                }

                friend void tag_invoke(stdexec::set_stopped_t, InnerReceiver&& self) noexcept {
                    stdexec::set_stopped(std::move(self._parent->_receiver));
                }

                friend auto tag_invoke(stdexec::get_env_t, const InnerReceiver& self) noexcept {
                    return stdexec::get_env(self._parent->_receiver);
                }
            };

            using InnerOpState = stdexec::connect_result_t<ControlSender, InnerReceiver>;
            InnerOpState _innerOp;
            OperationState(Data&& data, AcceptOptions options, Receiver receiver) :
                _data{ std::move(data) },
                _receiver{ std::move(receiver) },
                _innerOp{ stdexec::connect(
                    ControlSender{ &_data, options, AcceptControlAction::Accept },

                    InnerReceiver{ this }
                ) }
            {}

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                stdexec::start(self._innerOp);
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, AcceptSender&& self, Receiver r) {
            return { std::move(self._data), self._options, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAsyncAcceptPolicy<Data>::Listen(Data& data, const int backlog, ListenOptions options) noexcept {
        return TlsAcceptPolicy<Data>::Listen(data, backlog, options);
    }

    template<SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<Data>::Accept(Data& listenData, Data&& clientData, AcceptOptions options) {
        clientData.acceptStateMachine = std::make_unique<_details::TlsAcceptStateMachine<Data, TlsAsyncAcceptPolicy>>(options);
        _options = options;

        static_assert(stdexec::sender<ControlSender>);
        auto defaultOptions{ static_cast<DefaultAsyncAcceptPolicy<Data>::AcceptOptions>(options) };

        return DefaultAsyncAcceptPolicy<Data>::Accept(listenData, std::move(clientData), defaultOptions)
             | stdexec::let_value([options](Data& data) {
                   return AcceptSender{ std::move(data), options };
               });
    }

    template<SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<Data>::Accept(Data &listenData, AcceptOptions options) {
        return Accept(listenData, listenData.MakeChild(), options);
    }

    template<SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<Data>::Renegotiate(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        return ControlSender{ &data, _options, AcceptControlAction::Renegotiate };
    }

    template<SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        return ControlSender{ &data, _options, AcceptControlAction::Shutdown };
    }

    template<SocketDataConcept Data>
    void TlsAsyncAcceptPolicy<Data>::Close(Data& data) noexcept {
        TlsAcceptPolicy<Data>::Close(data);
    }

    template<SocketDataConcept Data>
    void TlsAsyncAcceptPolicy<Data>::Abort(Data &data) noexcept {
        TlsAcceptPolicy<Data>::Abort(data);
    }
}