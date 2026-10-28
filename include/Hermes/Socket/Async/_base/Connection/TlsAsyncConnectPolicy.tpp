#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.hpp>
#include <Hermes/_base/OsApi/Macros.hpp>
#include <Hermes/_base/Network.hpp>

namespace Hermes {

    enum class ControlAction : std::uint8_t { Connect, Renegotiate, Shutdown };
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncConnectPolicy<ExecutionContext, Data>::ControlSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;
        Data* m_data;
        Options m_options;
        ControlAction m_action;

        template<class Receiver>
        struct OperationState {
        private:
            Data* m_data;
            Options m_options;
            Receiver m_receiver;
            TransferOperStatus m_status;
            ControlAction m_action;

            LongIoCount m_flags{};
        public:

            OperationState(Data* data, Options options, Receiver receiver, const ControlAction action = ControlAction::Connect) :
                m_data{ data }, m_options{ options }, m_receiver{ std::move(receiver) },
                m_status{}, m_action{ action } {}

            void Pump() noexcept {
                m_flags = 0;

                while (true) {
                    using ConnectStateOpResult = details_::ConnectStateOpResult;
                    switch (m_data->connectStateMachine->Advance(*m_data)) {
                        case ConnectStateOpResult::Send: {
                            auto buf{ m_data->connectStateMachine->GetSendBuffer() };

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
                        case ConnectStateOpResult::Recv: {
                            auto buf{ m_data->connectStateMachine->GetRecvBuffer(*m_data) };

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
                        case ConnectStateOpResult::Done:
                            stdexec::set_value(std::move(m_receiver));
                            return;
                        case ConnectStateOpResult::Error:
                            stdexec::set_error(std::move(m_receiver), m_data->connectStateMachine->GetResult().error_or(ConnectionErrorEnum::Unknown));
                            return;
                        case ConnectStateOpResult::Closed: {
                            if (const auto result{ m_data->connectStateMachine->GetResult() })
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

                self->m_data->connectStateMachine->SetIoResult(bytesTransferred);

                self->Pump();
            }

            void start() & noexcept {
                m_status.context = this;
                m_status.callback = IoCallback;

                if (m_action == ControlAction::Shutdown && m_data->socket == macroINVALID_SOCKET) {
                    stdexec::set_value(std::move(m_receiver));
                    return;
                }

                if (m_action == ControlAction::Shutdown)
                    m_data->connectStateMachine->SetToClose();
                else
                    m_data->connectStateMachine->SetToOpen();
                Pump();
            }
        };
        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) const {
            return { m_data, m_options, std::move(r), m_action };
        }
    };

    template<class ExecutionContext, SocketDataConcept Data>
    auto TlsAsyncConnectPolicy<ExecutionContext, Data>::Connect(Data& data, Options options) {
        data.connectStateMachine = std::make_unique<details_::TlsConnectStateMachine<Data, TlsAsyncConnectPolicy>>(options);
        m_options = options;

        static_assert(stdexec::sender<ControlSender>);

        // TCP connect first, then drive the TLS handshake through the control sender. If ANYTHING along the chain
        // fails, the socket handle must be released HERE, not left to the caller: after set_error no facade may
        // exist to close it in its destructor (e.g. when this policy is used directly), which would leak a created
        // and possibly connected socket. Close() is idempotent, so it is safe even when the TCP connect error
        // paths already closed the handle, or when the facade destructor closes it again during unwinding.
        //
        // The let_error handler MUST be generic: let_value adds std::exception_ptr to the error channel whenever
        // its function is potentially-throwing, so the channel here is {ConnectionErrorEnum, std::exception_ptr}
        // and stdexec requires the handler to accept every error type (same rule as the mapErrorPipeline in the
        // examples). The original error is forwarded unchanged; Close() runs for both error types.
        auto connectSender{ DefaultAsyncConnectPolicy<ExecutionContext, Data>::Connect(data, options)
             | stdexec::let_value([&data, options]() { return ControlSender{ &data, options }; })
             | stdexec::let_error([&data](auto&& error) {
                   Close(data);
                   return stdexec::just_error(std::forward<decltype(error)>(error));
               }) };
        static_assert(stdexec::sender_in<decltype(connectSender)>,
            "TlsAsyncConnectPolicy::Connect must produce a well-formed sender: if the error channel changed, "
            "the let_error handler above must accept every error type.");
        return connectSender;
    }

    template<class ExecutionContext, SocketDataConcept Data>
    auto TlsAsyncConnectPolicy<ExecutionContext, Data>::Shutdown(Data& data) {
        static_assert(stdexec::sender<ControlSender>);
        static_assert(std::same_as<stdexec::value_types_of_t<ControlSender>, std::variant<std::tuple<>>>);
        static_assert(std::same_as<stdexec::error_types_of_t<ControlSender>, std::variant<ConnectionErrorEnum>>);

        return ControlSender{ &data, m_options, ControlAction::Shutdown };
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void TlsAsyncConnectPolicy<ExecutionContext, Data>::Close(Data& data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }

    template<class ExecutionContext, SocketDataConcept Data>
    void TlsAsyncConnectPolicy<ExecutionContext, Data>::Abort(Data &data) noexcept {
        if (data.socket != macroINVALID_SOCKET) {
            constexpr linger lingerOption{ 1, 0 };
            setsockopt(data.socket, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lingerOption), sizeof(lingerOption));
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
        }
    }
}

#endif
