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

    template<class ExecutionContext, SocketDataConcept Data>
    ConnectionResultOper TlsAsyncAcceptPolicy<ExecutionContext, Data>::Listen(Data& data, const int backlog, ListenOptions options) noexcept {
        auto result{ TlsAcceptPolicy<Data>::Listen(data, backlog, options) };
        if (!result)
            return result;

        auto* context{ data.executionContext };
r        if (!context || !context->RegisterHandle(reinterpret_cast<SocketHandle>(data.socket))) {
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
        TlsAcceptPolicy<Data>::Close(data);
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void TlsAsyncAcceptPolicy<ExecutionContext, Data>::Abort(Data &data) noexcept {
        TlsAcceptPolicy<Data>::Abort(data);
    }
}

#endif
