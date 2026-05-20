#pragma once
#include <Hermes/Socket/_base/Transfer/TlsTransferStateMachine.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    struct TlsAsyncTransferPolicy<Data>::TransferSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(size_t),
            stdexec::set_error_t(TransferError),
            stdexec::set_stopped_t()
        >;

        TlsAsyncTransferPolicy* _policy;
        Data* _data;
        std::span<std::byte> _recvBuffer;
        std::span<const std::byte> _sendBuffer;
        RecvModeEnum _mode;
        ActionEnum _action;

        template<class Receiver>
        struct OperationState {
        private:
            TlsAsyncTransferPolicy* _policy;
            Data* _data;
            std::span<std::byte> _recvBuffer;
            std::span<const std::byte> _sendBuffer;
            RecvModeEnum _mode;
            ActionEnum _action;
            Receiver _receiver;
            TransferOperStatus _status{};
            DWORD _flags{};
            size_t _accumulatedBytes{};  // ADDED

        public:
            OperationState(TlsAsyncTransferPolicy* policy, Data* data, std::span<std::byte> recv, std::span<const std::byte> send, RecvModeEnum mode, ActionEnum action, Receiver receiver) noexcept :
                _policy{ policy }, _data{ data }, _recvBuffer{ recv }, _sendBuffer{ send }, _mode{ mode }, _action{ action }, _receiver{ std::move(receiver) } {}

            void I_Pump() noexcept {
                _flags = 0;

                while (true) {
                    using TransferStateOpResult = _details::TransferStateOpResult;

                    switch (_data->transferStateMachine->Advance(*_data)) {
                        case TransferStateOpResult::Send: {
                            auto buf{ _data->transferStateMachine->GetSendBuffer(*_data) };
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(const_cast<std::byte*>(buf.data())) };
                            DWORD sendFlags{};

                            if (WSASend(_data->socket, &wsaBuf, 1, nullptr, sendFlags, &_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(_receiver), TransferError{ _data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                                }
                            }
                            return;
                        }
                        case TransferStateOpResult::Recv: {
                            auto buf{ _data->transferStateMachine->GetRecvBuffer(*_data) };
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(buf.data()) };

                            if (WSARecv(_data->socket, &wsaBuf, 1, nullptr, &_flags, &_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(_receiver), TransferError{ _data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                                }
                            }
                            return;
                        }
                        case TransferStateOpResult::Done:
                        case TransferStateOpResult::Error: {
                            auto [bytes, oper]{ _data->transferStateMachine->GetResult() };

                            if (oper) {
                                stdexec::set_value(std::move(_receiver), _accumulatedBytes + bytes);
                                return;
                            }

                            if (oper.error() == ConnectionErrorEnum::RenegotiationRequired) {
                                _accumulatedBytes += bytes;

                                if (_action == ActionEnum::Recv)
                                    _recvBuffer = _recvBuffer.subspan(bytes);
                                else
                                    _sendBuffer = _sendBuffer.subspan(bytes);

                                if (_data->connectStateMachine) {
                                    _data->connectStateMachine->SetToOpen();

                                    if (!_data->connectStateMachine->IsFinished())
                                        _data->connectStateMachine->Advance(*_data);
                                }
                                else if (_data->acceptStateMachine) {
                                    _data->acceptStateMachine->SetToOpen();

                                    if (!_data->acceptStateMachine->IsFinished())
                                        _data->acceptStateMachine->Advance(*_data);
                                } else {
                                    stdexec::set_error(std::move(_receiver), TransferError{ _accumulatedBytes, ConnectionErrorEnum::Unknown });
                                }
                                // TODO: Await properly

                                const auto hsResult{ _data->connectStateMachine->GetResult() };
                                if (!hsResult) {
                                    stdexec::set_error(std::move(_receiver), TransferError{ _accumulatedBytes, hsResult.error() });
                                    return;
                                }

                                if (_action == ActionEnum::Recv)
                                    _data->transferStateMachine->StartToRecv(_recvBuffer, _mode);
                                else
                                    _data->transferStateMachine->StartToSend(_sendBuffer);

                                continue;
                            }

                            stdexec::set_error(std::move(_receiver), TransferError{ _accumulatedBytes + bytes, oper.error() });
                            return;
                        }
                    }
                }
            }

            static void S_IoCallback(void* context, DWORD bytesTransferred, bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);
                if (!success) {
                    stdexec::set_error(std::move(self->_receiver), TransferError{ self->_data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                    return;
                }

                self->_data->transferStateMachine->SetIoResult(bytesTransferred);
                self->I_Pump();
            }

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                self._status.context = &self;
                self._status.callback = S_IoCallback;

                if (self._action == ActionEnum::Recv)
                    self._data->transferStateMachine->StartToRecv(self._recvBuffer, self._mode);
                else
                    self._data->transferStateMachine->StartToSend(self._sendBuffer);

                self.I_Pump();
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const TransferSender& self, Receiver r) {
            return { self._policy, self._data, self._recvBuffer, self._sendBuffer, self._mode, self._action, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsAsyncTransferPolicy<Data>::AsyncRecv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum recvMode) noexcept {
        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<_details::TlsTransferStateMachine<Data, TlsAsyncTransferPolicy>>();

        std::span<std::byte> byteSpan{ reinterpret_cast<std::byte*>(bufferRecv.data()), bufferRecv.size_bytes() };
        return TransferSender<Byte>{ this, &data, byteSpan, {}, recvMode, ActionEnum::Recv };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsAsyncTransferPolicy<Data>::AsyncSend(Data& data, std::span<const Byte> bufferSend) noexcept {
        static_assert(stdexec::sender<TransferSender<Byte>>);
        static_assert(std::same_as<stdexec::value_types_of_t<TransferSender<Byte>>, std::variant<std::tuple<size_t>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<TransferSender<Byte>>, std::variant<TransferError>>);

        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<_details::TlsTransferStateMachine<Data, TlsAsyncTransferPolicy>>();

        std::span<const std::byte> byteSpan{ reinterpret_cast<const std::byte*>(bufferSend.data()), bufferSend.size_bytes() };
        return TransferSender<Byte>{ this, &data, {}, byteSpan, RecvModeEnum::All, ActionEnum::Send };
    }
}
