#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/ExecutionContext/StopGuard.hpp>

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

        [[nodiscard]] auto get_env() const noexcept {
            return details_::CompletionEnv(m_data->executionContext);
        }

        template<class Receiver>
        struct OperationState {
            Data* m_data;
            std::span<Byte>    m_buffer;
            RecvModeEnum       m_mode;
            Receiver           m_receiver;
            TransferOperStatus m_status{};
            size_t             m_total{};
            using StopToken = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
            details_::StopGuard<StopToken> m_stop{};

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };

                if (!success) {
                    self->m_stop.Disarm();

                    if (self->m_stop.Cancelled()) {
                        stdexec::set_stopped(std::move(self->m_receiver));
                        return;
                    }

                    stdexec::set_error(std::move(self->m_receiver),
                        TransferError{ self->m_total, ConnectionErrorEnum::ReceiveFailed });
                    return;
                }

                if (bytesTransferred == 0) {
                    self->m_stop.Disarm();
                    stdexec::set_error(std::move(self->m_receiver),
                        TransferError{ self->m_total, ConnectionErrorEnum::ConnectionClosed });
                    return;
                }

                self->m_total += static_cast<size_t>(bytesTransferred);
                if (self->m_mode == RecvModeEnum::Any || self->m_total >= self->m_buffer.size()) {
                    self->m_stop.Disarm();
                    stdexec::set_value(std::move(self->m_receiver), self->m_total);
                    return;
                }

                self->PostRecv();
            }

            void PostRecv() noexcept {
                auto* context{ m_data->executionContext };
                if (!context) {
                    m_stop.Disarm();
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                if (m_stop.Cancelled()) {
                    m_stop.Disarm();
                    stdexec::set_stopped(std::move(m_receiver));
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
                    m_stop.Disarm();
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::ReceiveFailed });
                    return;
                }

                if (m_stop.Cancelled())
                    m_stop.CancelNow();
            }

            void start() & noexcept {
                if (m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                m_stop.Arm(m_data->executionContext, reinterpret_cast<SocketHandle>(m_data->socket), &m_status,
                           stdexec::get_stop_token(stdexec::get_env(m_receiver)));
                PostRecv();
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const noexcept {
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

        //! @brief Advertises the bound execution context's scheduler as the value completion scheduler.
        [[nodiscard]] auto get_env() const noexcept {
            return details_::CompletionEnv(m_data->executionContext);
        }

        template<class Receiver>
        struct OperationState {
            Data* m_data;
            std::span<const Byte>  m_buffer;
            Receiver               m_receiver;
            TransferOperStatus     m_status{};
            size_t                 m_total{};
            using StopToken        = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
            details_::StopGuard<StopToken> m_stop{};

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self{ static_cast<OperationState*>(context) };

                if (!success) {
                    self->m_stop.Disarm();

                    if (self->m_stop.Cancelled()) {
                        stdexec::set_stopped(std::move(self->m_receiver));
                        return;
                    }

                    stdexec::set_error(std::move(self->m_receiver),
                        TransferError{ self->m_total, ConnectionErrorEnum::SendFailed });
                    return;
                }

                self->m_total += static_cast<size_t>(bytesTransferred);
                if (self->m_total >= self->m_buffer.size()) {
                    self->m_stop.Disarm();
                    stdexec::set_value(std::move(self->m_receiver), self->m_total);
                    return;
                }

                self->PostSend();
            }

            void PostSend() noexcept {
                auto* context{ m_data->executionContext };
                if (!context) {
                    m_stop.Disarm();
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                if (m_stop.Cancelled()) {
                    m_stop.Disarm();
                    stdexec::set_stopped(std::move(m_receiver));
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
                    m_stop.Disarm();
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ m_total, ConnectionErrorEnum::SendFailed });
                    return;
                }

                if (m_stop.Cancelled())
                    m_stop.CancelNow();
            }

            void start() & noexcept {
                if (m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_error(std::move(m_receiver),
                        TransferError{ 0, ConnectionErrorEnum::SocketNotOpen });
                    return;
                }

                m_stop.Arm(m_data->executionContext, reinterpret_cast<SocketHandle>(m_data->socket), &m_status,
                           stdexec::get_stop_token(stdexec::get_env(m_receiver)));
                PostSend();
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const noexcept {
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