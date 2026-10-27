#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

namespace Hermes {
    template<class ExecutionContext, SocketDataConcept Data>
    template<ByteLike Byte>
    struct DefaultAsyncTransferPolicy<ExecutionContext, Data>::RecvSender {
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
            std::span<Byte>    m_buffer;
            RecvModeEnum       m_mode;
            Receiver           m_receiver;
            TransferOperStatus m_status{};
            size_t             m_total{};

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

                self->m_total += static_cast<size_t>(bytesTransferred);
                if (self->m_mode == RecvModeEnum::Any || self->m_total >= self->m_buffer.size()) {
                    stdexec::set_value(std::move(self->m_receiver), self->m_total);
                    return;
                }

                self->PostRecv();
            }

            void PostRecv() noexcept {
                auto* context{ m_data->executionContext };
                if (!context) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                m_status          = {};
                m_status.context  = this;
                m_status.callback = IoCallback;

                const IoRequest request{
                    .operation = IoOperation::Recv,
                    .socket    = m_data->socket,
                    .buffer    = m_buffer.data() + m_total,
                    .length    = m_buffer.size() - m_total,
                    .status    = &m_status
                };

                if (!context->SubmitIo(request)) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::ReceiveFailed });
                }
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

    template<class ExecutionContext, SocketDataConcept Data>
    template<ByteLike Byte>
    struct DefaultAsyncTransferPolicy<ExecutionContext, Data>::SendSender {
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

                self->m_total += static_cast<size_t>(bytesTransferred);
                if (self->m_total >= self->m_buffer.size()) {
                    stdexec::set_value(std::move(self->m_receiver), self->m_total);
                    return;
                }

                self->PostSend();
            }

            void PostSend() noexcept {
                auto* context{ m_data->executionContext };
                if (!context) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                m_status          = {};
                m_status.context  = this;
                m_status.callback = IoCallback;

                const IoRequest request{
                    .operation = IoOperation::Send,
                    .socket    = m_data->socket,
                    .buffer    = const_cast<std::byte*>(m_buffer.data() + m_total),
                    .length    = m_buffer.size() - m_total,
                    .status    = &m_status
                };

                if (!context->SubmitIo(request)) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SendFailed });
                }
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

    template<class ExecutionContext, SocketDataConcept Data>
    template<ByteLike Byte>
    auto DefaultAsyncTransferPolicy<ExecutionContext, Data>::Recv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum mode) noexcept {
        return RecvSender<Byte>{ &data, bufferRecv, mode };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    template<ByteLike Byte>
    auto DefaultAsyncTransferPolicy<ExecutionContext, Data>::Send(Data& data, std::span<const Byte> bufferSend) noexcept {
        return SendSender<Byte>{ &data, bufferSend };
    }
}

#endif