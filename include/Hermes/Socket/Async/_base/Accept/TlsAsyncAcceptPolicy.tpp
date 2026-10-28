#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/_base/OsApi/Macros.hpp>
#include <Hermes/_base/Network.hpp>
#include <stdexec/execution.hpp>

namespace Hermes {

    enum class AcceptControlAction : std::uint8_t { Accept, Renegotiate, Shutdown };
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy<ExecutionContext, Data>::ControlSender {
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

                            auto* context{ m_data->executionContext };
                            if (!context) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
                                return;
                            }

                            const IoRequest request{
                                .operation = IoOperation::Send,
                                .socket    = m_data->socket,
                                .buffer    = const_cast<std::byte*>(buf.data()),
                                .length    = buf.size(),
                                .status    = &m_status
                            };

                            if (!context->SubmitIo(request)) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                            }
                            return;
                        }
                        case AcceptStateOpResult::Recv: {
                            auto buf{ m_data->acceptStateMachine->GetRecvBuffer(*m_data) };

                            auto* context{ m_data->executionContext };
                            if (!context) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
                                return;
                            }

                            const IoRequest request{
                                .operation = IoOperation::Recv,
                                .socket    = m_data->socket,
                                .buffer    = buf.data(),
                                .length    = buf.size(),
                                .status    = &m_status
                            };

                            if (!context->SubmitIo(request)) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                            }
                            return;
                        }
                        case AcceptStateOpResult::Done:
                            stdexec::set_value(std::move(m_receiver));
                            return;
                        case AcceptStateOpResult::Error:
                            stdexec::set_error(std::move(m_receiver), m_data->acceptStateMachine->GetResult().error_or(ConnectionErrorEnum::Unknown));
                            return;
                        case AcceptStateOpResult::Closed: {
                            if (const auto result{ m_data->acceptStateMachine->GetResult() })
                                stdexec::set_value(std::move(m_receiver));
                            else
                                stdexec::set_error(std::move(m_receiver), result.error());
                            return;
                        }
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

    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy<ExecutionContext, Data>::AcceptSender {
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
                    ReleaseAcceptedHandle();
                    stdexec::set_error(std::move(m_parent->m_receiver), e);
                }

                void set_stopped() && noexcept {
                    ReleaseAcceptedHandle();
                    stdexec::set_stopped(std::move(m_parent->m_receiver));
                }

                // The return type MUST be spelled out instead of `auto`: MSVC evaluates the
                // InnerOpState member typedef (connect_result_t) while this OperationState is
                // still being completed, and that check probes get_env - with `auto` the body
                // is instantiated immediately, dereferencing m_parent whose type is incomplete
                // at that point (C2027 "use of undefined type"). A declared return type only
                // needs the declaration; the body compiles later, in complete-class context.
                decltype(stdexec::get_env(std::declval<const Receiver&>())) get_env() const noexcept {
                    return stdexec::get_env(m_parent->m_receiver);
                }

            private:
                // The accepted socket is open (and on Windows pre-created) by the time the TLS handshake
                // starts. On handshake failure/cancellation no facade will ever own it, so the handle must
                // be released HERE - same contract as the TCP-phase cleanup and the TlsAsyncConnectPolicy
                // error path. Guarded raw close: the state machine's own error cleanup (Cleanup ->
                // StartClose -> EndClose) may already have closed it, and CloseSocket on INVALID_SOCKET
                // is a no-op guarded by the caller-facing check below.
                void ReleaseAcceptedHandle() const noexcept {
                    if (m_parent->m_data.socket != macroINVALID_SOCKET) {
                        CloseSocket(m_parent->m_data.socket);
                        m_parent->m_data.socket = macroINVALID_SOCKET;
                    }
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

    template<class ExecutionContext, SocketDataConcept Data>
    ConnectionResultOper TlsAsyncAcceptPolicy<ExecutionContext, Data>::Listen(Data& data, const int backlog, ListenOptions options) noexcept {
        auto result{ TlsAcceptPolicy<Data>::Listen(data, backlog, options) };
        if (!result)
            return result;

        auto* context{ data.executionContext };
        if (!context || !context->RegisterHandle(reinterpret_cast<SocketHandle>(data.socket))) {
            Close(data);
            return std::unexpected{ ConnectionErrorEnum::NoScheduler };
        }

        if (auto loaded{ m_tcp.LoadAcceptExtensions(data) }; !loaded) {
            Close(data);
            return loaded;
        }

        return {};
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<ExecutionContext, Data>::Accept(Data& listenData, Data&& clientData, AcceptOptions options) {
        clientData.acceptStateMachine = std::make_unique<details_::TlsAcceptStateMachine<Data, TlsAsyncAcceptPolicy>>(options);
        m_options = options;

        static_assert(stdexec::sender<ControlSender>);

        return m_tcp.Accept(listenData, std::move(clientData), options)
             | stdexec::let_value([options](Data& data) {
                   return AcceptSender{ std::move(data), options };
               });
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<ExecutionContext, Data>::Accept(Data &listenData, AcceptOptions options) {
        return Accept(listenData, listenData.MakeChild(), options);
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<ExecutionContext, Data>::Renegotiate(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        return ControlSender{ &data, m_options, AcceptControlAction::Renegotiate };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto TlsAsyncAcceptPolicy<ExecutionContext, Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        return ControlSender{ &data, m_options, AcceptControlAction::Shutdown };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void TlsAsyncAcceptPolicy<ExecutionContext, Data>::Close(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void TlsAsyncAcceptPolicy<ExecutionContext, Data>::Abort(Data &data) noexcept {
        TlsAcceptPolicy<Data>::Abort(data);
    }
}

#endif
