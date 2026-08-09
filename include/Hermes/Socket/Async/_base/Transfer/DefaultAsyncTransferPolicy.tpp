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

        Data* m_data;
        std::span<Byte> m_buffer;
        RecvModeEnum m_mode;

        template<class Receiver>
        struct OperationState {
            Data* m_data;
            std::span<Byte>   m_buffer;
            RecvModeEnum      m_mode;
            Receiver          m_receiver;
            TransferOperStatus m_status{};
            size_t            m_total{};

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    stdexec::set_error(std::move(self->m_receiver),
                        TransferError{ self->m_total, ConnectionErrorEnum::ReceiveFailed });
                    return;
                }

                if (bytesTransferred == 0) {
                    stdexec::set_error(std::move(self->m_receiver),
                        TransferError{ self->m_total, ConnectionErrorEnum::ConnectionClosed });
                    return;
                }

                self->m_total += bytesTransferred;
                if (self->m_mode == RecvModeEnum::Any || self->m_total >= self->m_buffer.size()) {
                    stdexec::set_value(std::move(self->m_receiver), self->m_total);
                    return;
                }

                // RecvModeEnum::All — still need more bytes, re-post
                self->PostRecv();
            }

            void PostRecv() noexcept {
                m_status          = {};
                m_status.context  = this;
                m_status.callback = IoCallback;

#ifdef _WIN32
                WSABUF wsaBuf{};
                wsaBuf.buf = reinterpret_cast<char*>(m_buffer.data() + m_total);
                wsaBuf.len = static_cast<ULONG>(m_buffer.size() - m_total);
                DWORD flags{};
                const int res{ WSARecv(m_data->socket, &wsaBuf, 1,
                    nullptr, &flags,
                    static_cast<LPWSAOVERLAPPED>(&m_status), nullptr) };
                if (res == macroSOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::ReceiveFailed });
                }
#else
                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                if (!loop) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }
                loop->SubmitIo([this](struct io_uring_sqe* sqe) {
                    io_uring_prep_recv(sqe, static_cast<int>(m_data->socket),
                        m_buffer.data() + m_total, m_buffer.size() - m_total, 0);
                    io_uring_sqe_set_data(sqe, &m_status);
                });
#endif
            }

            void start() & noexcept {
                if (m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                PostRecv();
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, m_buffer, m_mode, std::move(r) };
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

        Data* m_data;
        std::span<const Byte> m_buffer;

        template<class Receiver>
        struct OperationState {
            Data* m_data;
            std::span<const Byte>  m_buffer;
            Receiver               m_receiver;
            TransferOperStatus     m_status{};
            size_t                 m_total{};

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };
                if (!success) {
                    stdexec::set_error(std::move(self->m_receiver),
                        TransferError{ self->m_total, ConnectionErrorEnum::SendFailed });
                    return;
                }

                self->m_total += bytesTransferred;
                if (self->m_total >= self->m_buffer.size()) {
                    stdexec::set_value(std::move(self->m_receiver), self->m_total);
                    return;
                }

                // Partial send — re-post the remainder
                self->PostSend();
            }

            void PostSend() noexcept {
                m_status          = {};
                m_status.context  = this;
                m_status.callback = IoCallback;

#ifdef _WIN32
                WSABUF wsaBuf{};
                wsaBuf.buf = const_cast<char*>(
                    reinterpret_cast<const char*>(m_buffer.data() + m_total));
                wsaBuf.len = static_cast<ULONG>(m_buffer.size() - m_total);

                const int res{ WSASend(m_data->socket, &wsaBuf, 1,
                    nullptr, 0,
                    static_cast<LPWSAOVERLAPPED>(&m_status), nullptr) };
                if (res == macroSOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SendFailed });
                }
#else
                auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                if (!loop) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }
                loop->SubmitIo([this](struct io_uring_sqe* sqe) {
                    io_uring_prep_send(sqe, static_cast<int>(m_data->socket),
                        m_buffer.data() + m_total, m_buffer.size() - m_total, 0);
                    io_uring_sqe_set_data(sqe, &m_status);
                });
#endif
            }

            void start() & noexcept {
                if (m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                PostSend();
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, m_buffer, std::move(r) };
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