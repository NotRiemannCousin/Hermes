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
        TlsAsyncTransferPolicy* m_policy;
        Data* m_data;
        std::span<std::byte> m_recvBuffer;
        std::span<const std::byte> m_sendBuffer;
        RecvModeEnum m_mode;
        ActionEnum m_action;
        template<class Receiver>
        struct OperationState {
        private:
            TlsAsyncTransferPolicy* m_policy;
            Data* m_data;
            std::span<std::byte> m_recvBuffer;
            std::span<const std::byte> m_sendBuffer;
            RecvModeEnum m_mode;
            ActionEnum m_action;
            Receiver m_receiver;
            TransferOperStatus m_status{};
            size_t m_accumulatedBytes{};

            LongIoCount m_flags{};
        public:
            OperationState(TlsAsyncTransferPolicy* policy, Data* data, std::span<std::byte> recv, std::span<const std::byte> send, RecvModeEnum mode, ActionEnum action, Receiver receiver) :
                m_policy{ policy }, m_data{ data }, m_recvBuffer{ recv }, m_sendBuffer{ send }, m_mode{ mode }, m_action{ action }, m_receiver{ std::move(receiver) } {}

            void Pump() noexcept {
                m_flags = 0;
                while (true) {
                    using TransferStateOpResult = details_::TransferStateOpResult;
                    switch (m_data->transferStateMachine->Advance(*m_data)) {
                        case TransferStateOpResult::Send: {
                            auto buf{ m_data->transferStateMachine->GetSendBuffer(*m_data) };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(const_cast<std::byte*>(buf.data())) };
                            DWORD sendFlags{};

                            if (WSASend(m_data->socket, &wsaBuf, 1, nullptr, sendFlags, &m_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(m_receiver), TransferError{ m_data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                                }
                            }
#else
                            auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                            if (!loop) {
                                stdexec::set_error(std::move(m_receiver), TransferError{ m_data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                                return;
                            }
                            loop->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_send(sqe, static_cast<int>(m_data->socket), buf.data(), buf.size(), 0);
                                io_uring_sqe_set_data(sqe, &m_status);
                            });
#endif
                            return;
                        }
                        case TransferStateOpResult::Recv: {
                            auto buf{ m_data->transferStateMachine->GetRecvBuffer(*m_data) };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(buf.data()) };
                            if (WSARecv(m_data->socket, &wsaBuf, 1, nullptr, &m_flags, &m_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(m_receiver), TransferError{ m_data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                                }
                            }
#else
                            auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                            if (!loop) {
                                stdexec::set_error(std::move(m_receiver), TransferError{ m_data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                                return;
                            }
                            loop->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_recv(sqe, static_cast<int>(m_data->socket), buf.data(), buf.size(), 0);
                                io_uring_sqe_set_data(sqe, &m_status);
                            });
#endif
                            return;
                        }
                        case TransferStateOpResult::Done:
                        case TransferStateOpResult::Error: {
                            auto [bytes, oper]{ m_data->transferStateMachine->GetResult() };
                            if (oper) {
                                stdexec::set_value(std::move(m_receiver), m_accumulatedBytes + bytes);
                                return;
                            }

                            if (oper.error() == ConnectionErrorEnum::RenegotiationRequired) {
                                m_accumulatedBytes += bytes;
                                if (m_action == ActionEnum::Recv)
                                    m_recvBuffer = m_recvBuffer.subspan(bytes);
                                else
                                    m_sendBuffer = m_sendBuffer.subspan(bytes);
                                if (m_data->connectStateMachine) {
                                    m_data->connectStateMachine->SetToOpen();
                                    if (!m_data->connectStateMachine->IsFinished())
                                        m_data->connectStateMachine->Advance(*m_data);
                                }
                                else if (m_data->acceptStateMachine) {
                                    m_data->acceptStateMachine->SetToOpen();
                                    if (!m_data->acceptStateMachine->IsFinished())
                                        m_data->acceptStateMachine->Advance(*m_data);
                                } else {
                                    stdexec::set_error(std::move(m_receiver), TransferError{ m_accumulatedBytes, ConnectionErrorEnum::Unknown });
                                }

                                const auto hsResult{ m_data->connectStateMachine ?
                                    m_data->connectStateMachine->GetResult() : m_data->acceptStateMachine->GetResult() };

                                if (!hsResult) {
                                    stdexec::set_error(std::move(m_receiver), TransferError{ m_accumulatedBytes, hsResult.error() });
                                    return;
                                }

                                if (m_action == ActionEnum::Recv)
                                    m_data->transferStateMachine->StartToRecv(m_recvBuffer, m_mode);
                                else
                                    m_data->transferStateMachine->StartToSend(m_sendBuffer);
                                continue;
                            }

                            stdexec::set_error(std::move(m_receiver), TransferError{ m_accumulatedBytes + bytes, oper.error() });
                            return;
                        }
                    }
                }
            }

            static void IoCallback(void* context, LongIoCount bytesTransferred, bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);
                if (!success) {
                    stdexec::set_error(std::move(self->m_receiver), TransferError{ self->m_data->transferStateMachine->GetResult().first, ConnectionErrorEnum::Unknown });
                    return;
                }

                self->m_data->transferStateMachine->SetIoResult(bytesTransferred);
                self->Pump();
            }

            void start() & noexcept {
                m_status.context = this;
                m_status.callback = IoCallback;

                if (m_action == ActionEnum::Recv)
                    m_data->transferStateMachine->StartToRecv(m_recvBuffer, m_mode);
                else
                    m_data->transferStateMachine->StartToSend(m_sendBuffer);
                Pump();
            }
        };
        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_policy, m_data, m_recvBuffer, m_sendBuffer, m_mode, m_action, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsAsyncTransferPolicy<Data>::Recv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum recvMode) noexcept {
        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<details_::TlsTransferStateMachine<Data, TlsAsyncTransferPolicy>>();
        std::span<std::byte> byteSpan{ reinterpret_cast<std::byte*>(bufferRecv.data()), bufferRecv.size_bytes() };
        return TransferSender<Byte>{ this, &data, byteSpan, {}, recvMode, ActionEnum::Recv };
    }

    template<SocketDataConcept Data>
    template<ByteLike Byte>
    auto TlsAsyncTransferPolicy<Data>::Send(Data& data, std::span<const Byte> bufferSend) noexcept {
        static_assert(stdexec::sender<TransferSender<Byte>>);
        static_assert(std::same_as<stdexec::value_types_of_t<TransferSender<Byte>>, std::variant<std::tuple<size_t>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<TransferSender<Byte>>, std::variant<TransferError>>);

        if (!data.transferStateMachine)
            data.transferStateMachine = std::make_unique<details_::TlsTransferStateMachine<Data, TlsAsyncTransferPolicy>>();
        std::span<const std::byte> byteSpan{ reinterpret_cast<const std::byte*>(bufferSend.data()), bufferSend.size_bytes() };
        return TransferSender<Byte>{ this, &data, {}, byteSpan, RecvModeEnum::All, ActionEnum::Send };
    }
}