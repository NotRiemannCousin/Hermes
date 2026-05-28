#pragma once

namespace Hermes {
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    struct DefaultAsyncTransferPolicy<Data>::RecvSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(size_t),
            stdexec::set_error_t(TransferError),
            stdexec::set_stopped_t()
        >;

        Data* _data;
        std::span<Byte> _buffer;
        RecvModeEnum _mode;

        template<class Receiver>
        struct OperationState {
            Data* _data;
            std::span<Byte>   _buffer;
            RecvModeEnum      _mode;
            Receiver          _receiver;
            TransferOperStatus _status{};
            size_t            _total{};

            static void IoCallback(void* context, size_t bytesTransferred, bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    stdexec::set_error(std::move(self->_receiver),
                        TransferError{ self->_total, ConnectionErrorEnum::ReceiveFailed });
                    return;
                }

                if (bytesTransferred == 0) {
                    stdexec::set_error(std::move(self->_receiver),
                        TransferError{ self->_total, ConnectionErrorEnum::ConnectionClosed });
                    return;
                }

                self->_total += bytesTransferred;
                if (self->_mode == RecvModeEnum::Any || self->_total >= self->_buffer.size()) {
                    stdexec::set_value(std::move(self->_receiver), self->_total);
                    return;
                }

                // RecvModeEnum::All — still need more bytes, re-post
                self->PostRecv();
            }

            void PostRecv() noexcept {
                _status          = {};
                _status.context  = this;
                _status.callback = IoCallback;

#ifdef _WIN32
                WSABUF wsaBuf{};
                wsaBuf.buf = reinterpret_cast<char*>(_buffer.data() + _total);
                wsaBuf.len = static_cast<ULONG>(_buffer.size() - _total);
                DWORD flags{};
                const int res{ WSARecv(_data->socket, &wsaBuf, 1,
                    nullptr, &flags,
                    static_cast<LPWSAOVERLAPPED>(&_status), nullptr) };
                if (res == macroSOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    stdexec::set_error(std::move(_receiver),
                        TransferError{ _total, ConnectionErrorEnum::ReceiveFailed });
                }
#else
                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(_data->socket));
                if (!loop) {
                    stdexec::set_error(std::move(_receiver),
                        TransferError{ _total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }
                loop->SubmitIo([this](struct io_uring_sqe* sqe) {
                    io_uring_prep_recv(sqe, static_cast<int>(_data->socket),
                        _buffer.data() + _total, _buffer.size() - _total, 0);
                    io_uring_sqe_set_data(sqe, &_status);
                });
#endif
            }

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                if (self._data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(self._receiver),
                        TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                self.PostRecv();
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const RecvSender& self, Receiver r) {
            return { self._data, self._buffer, self._mode, std::move(r) };
        }
    };


    template<SocketDataConcept Data>
    template<ByteLike Byte>
    struct DefaultAsyncTransferPolicy<Data>::SendSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(size_t),
            stdexec::set_error_t(TransferError),
            stdexec::set_stopped_t()
        >;

        Data* _data;
        std::span<const Byte> _buffer;

        template<class Receiver>
        struct OperationState {
            Data* _data;
            std::span<const Byte>  _buffer;
            Receiver               _receiver;
            TransferOperStatus     _status{};
            size_t                 _total{};

            static void IoCallback(void* context, size_t bytesTransferred, bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    stdexec::set_error(std::move(self->_receiver),
                        TransferError{ self->_total, ConnectionErrorEnum::SendFailed });
                    return;
                }

                self->_total += bytesTransferred;
                if (self->_total >= self->_buffer.size()) {
                    stdexec::set_value(std::move(self->_receiver), self->_total);
                    return;
                }

                // Partial send — re-post the remainder
                self->PostSend();
            }

            void PostSend() noexcept {
                _status          = {};
                _status.context  = this;
                _status.callback = IoCallback;

#ifdef _WIN32
                WSABUF wsaBuf{};
                wsaBuf.buf = const_cast<char*>(
                    reinterpret_cast<const char*>(_buffer.data() + _total));
                wsaBuf.len = static_cast<ULONG>(_buffer.size() - _total);

                const int res{ WSASend(_data->socket, &wsaBuf, 1,
                    nullptr, 0,
                    static_cast<LPWSAOVERLAPPED>(&_status), nullptr) };
                if (res == macroSOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    stdexec::set_error(std::move(_receiver),
                        TransferError{ _total, ConnectionErrorEnum::SendFailed });
                }
#else
                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(_data->socket));
                if (!loop) {
                    stdexec::set_error(std::move(_receiver),
                        TransferError{ _total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }
                loop->SubmitIo([this](struct io_uring_sqe* sqe) {
                    io_uring_prep_send(sqe, static_cast<int>(_data->socket),
                        _buffer.data() + _total, _buffer.size() - _total, 0);
                    io_uring_sqe_set_data(sqe, &_status);
                });
#endif
            }

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                if (self._data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(self._receiver),
                        TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                self.PostSend();
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const SendSender& self, Receiver r) {
            return { self._data, self._buffer, std::move(r) };
        }
    };


    // =========================================================================
    // Policy Methods
    // =========================================================================
    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto DefaultAsyncTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum mode) noexcept {
        return RecvSender<Byte>{ &data, bufferRecv, mode };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto DefaultAsyncTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) noexcept {
        return SendSender<Byte>{ &data, bufferSend };
    }
}