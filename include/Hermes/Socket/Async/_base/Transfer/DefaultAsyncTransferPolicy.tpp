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
            std::span<Byte> _buffer;
            RecvModeEnum _mode;
            Receiver _receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                if (self._data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(self._receiver), TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                size_t total{};

                while (total < self._buffer.size()) {
                    const int res{ recv(self._data->socket,
                        reinterpret_cast<char*>(self._buffer.data() + total),
                        static_cast<int>(self._buffer.size() - total), 0) };

                    if (res == 0) {
                        closesocket(std::exchange(self._data->socket, macroINVALID_SOCKET));
                        stdexec::set_error(std::move(self._receiver), TransferError{ total, ConnectionErrorEnum::ConnectionClosed });
                        return;
                    }

                    if (res == macroSOCKET_ERROR) {
                        stdexec::set_error(std::move(self._receiver), TransferError{ total, ConnectionErrorEnum::ReceiveFailed });
                        return;
                    }

                    total += res;
                    if (self._mode == RecvModeEnum::Any) break;
                }

                stdexec::set_value(std::move(self._receiver), total);
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
            std::span<const Byte> _buffer;
            Receiver _receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                if (self._data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(self._receiver), TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                size_t total{};

                while (total < self._buffer.size()) {
                    const int res{ send(self._data->socket,
                        reinterpret_cast<const char*>(self._buffer.data() + total),
                        static_cast<int>(self._buffer.size() - total), 0) };

                    if (res == macroSOCKET_ERROR) {
                        stdexec::set_error(std::move(self._receiver), TransferError{ total, ConnectionErrorEnum::SendFailed });
                        return;
                    }

                    total += res;
                }

                stdexec::set_value(std::move(self._receiver), total);
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
    auto DefaultAsyncTransferPolicy<Data>::AsyncRecv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum mode) noexcept {
        return RecvSender<Byte>{ &data, bufferRecv, mode };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto DefaultAsyncTransferPolicy<Data>::AsyncSend(Data& data, std::span<const Byte> bufferSend) noexcept {
        return SendSender<Byte>{ &data, bufferSend };
    }
}