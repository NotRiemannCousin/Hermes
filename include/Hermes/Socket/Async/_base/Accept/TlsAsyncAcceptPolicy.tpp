#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

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
        Data* m_data;
        AcceptOptions m_options;
        AcceptControlAction m_action;

        template<class Receiver>
        struct OperationState {
        private:
            Data* m_data;
            AcceptOptions m_options;
            Receiver m_receiver;
            TransferOperStatus m_status;
            AcceptControlAction m_action;

            LongIoCount m_flags{};
        public:
            OperationState(Data* data, AcceptOptions options, Receiver receiver, const AcceptControlAction action = AcceptControlAction::Accept) :
                m_data{ data }, m_options{ options }, m_receiver{ std::move(receiver) },
                m_status{}, m_action{ action } {}

            void Pump() noexcept {
                m_flags = 0;

                while (true) {
                    using AcceptStateOpResult = details_::AcceptStateOpResult;
                    switch (m_data->acceptStateMachine->Advance(*m_data)) {
                        case AcceptStateOpResult::Send: {
                            auto buf{ m_data->acceptStateMachine->GetSendBuffer() };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(const_cast<std::byte*>(buf.data())) };
                            if (WSASend(m_data->socket, &wsaBuf, 1, nullptr, m_flags, &m_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                }
                            }
#else
                            auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                            if (!loop) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                return;
                            }
                            loop->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_send(sqe, static_cast<int>(m_data->socket), buf.data(), buf.size(), 0);

                                 io_uring_sqe_set_data(sqe, &m_status);
                            });
#endif
                            return;
                        }
                        case AcceptStateOpResult::Recv: {
                            auto buf{ m_data->acceptStateMachine->GetRecvBuffer(*m_data) };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(buf.data()) };
                            if (WSARecv(m_data->socket, &wsaBuf, 1, nullptr, &m_flags, &m_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                }
                            }
#else
                            auto* loop = FastIoLoop::GetLoopForSocket(static_cast<int>(m_data->socket));
                            if (!loop) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                return;
                            }
                            loop->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_recv(sqe, static_cast<int>(m_data->socket), buf.data(), buf.size(), 0);

                                 io_uring_sqe_set_data(sqe, &m_status);
                            });
#endif
                            return;
                        }
                        case AcceptStateOpResult::Done:
                            stdexec::set_value(std::move(m_receiver));
                            return;
                        case AcceptStateOpResult::Error:
                            stdexec::set_error(std::move(m_receiver), m_data->acceptStateMachine->GetResult().error_or(ConnectionErrorEnum::Unknown));
                            return;
                        case AcceptStateOpResult::Closed:
                            stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                            return;
                    }
                }
            }

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);
                if (!success) {
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                self->m_data->acceptStateMachine->SetIoResult(bytesTransferred);
                self->Pump();
            }

            void start() & noexcept {
                m_status.context = this;
                m_status.callback = IoCallback;

                if (m_action == AcceptControlAction::Shutdown)
                    m_data->acceptStateMachine->SetToClose();
                else
                    m_data->acceptStateMachine->SetToOpen();
                Pump();
            }
        };
        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, m_options, std::move(r), m_action };
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
        Data m_data;
        AcceptOptions m_options;

        template<class Receiver>
        struct OperationState {
            Data m_data;
            Receiver m_receiver;

            struct InnerReceiver {
                using receiver_concept = stdexec::receiver_t;
                OperationState* m_parent;

                void set_value() && noexcept {
                    stdexec::set_value(std::move(m_parent->m_receiver), std::move(m_parent->m_data));
                }

                void set_error(ConnectionErrorEnum e) && noexcept {
                    stdexec::set_error(std::move(m_parent->m_receiver), e);
                }

                void set_stopped() && noexcept {
                    stdexec::set_stopped(std::move(m_parent->m_receiver));
                }

                auto get_env() const noexcept {
                    return stdexec::get_env(m_parent->m_receiver);
                }
            };

            using InnerOpState = stdexec::connect_result_t<ControlSender, InnerReceiver>;
            InnerOpState m_innerOp;
            OperationState(Data&& data, AcceptOptions options, Receiver receiver) :
                m_data{ std::move(data) },
                m_receiver{ std::move(receiver) },
                m_innerOp{ stdexec::connect(
                    ControlSender{ &m_data, options, AcceptControlAction::Accept },

                    InnerReceiver{ this }
                ) }
            {}

            void start() & noexcept {
                stdexec::start(m_innerOp);
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) && {
            return { std::move(m_data), m_options, std::move(r) };
        }
    };

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAsyncAcceptPolicy<Data>::Listen(Data& data, const int backlog, ListenOptions options) noexcept {
        return TlsAcceptPolicy<Data>::Listen(data, backlog, options);
    }

    template<SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<Data>::Accept(Data& listenData, Data&& clientData, AcceptOptions options) {
        clientData.acceptStateMachine = std::make_unique<details_::TlsAcceptStateMachine<Data, TlsAsyncAcceptPolicy>>(options);
        m_options = options;

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
        return ControlSender{ &data, m_options, AcceptControlAction::Renegotiate };
    }

    template<SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        return ControlSender{ &data, m_options, AcceptControlAction::Shutdown };
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

#endif