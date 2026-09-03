#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.tpp>
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
                    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
                    switch (m_data->connectStateMachine->Advance(*m_data)) {
                        case ConnectStateOpResult::Send: {
                            auto buf{ m_data->connectStateMachine->GetSendBuffer() };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(const_cast<std::byte*>(buf.data())) };

                            if (WSASend(m_data->socket, &wsaBuf, 1, nullptr, m_flags, &m_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                }
                            }
#else
                            auto* scheduler = m_options.scheduler;
                            if (!scheduler) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                return;
                            }
                            scheduler->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_send(sqe, static_cast<int>(m_data->socket), buf.data(), buf.size(), 0);
                                io_uring_sqe_set_data(sqe, &m_status);
                            });
#endif
                            return;
                        }
                        case ConnectStateOpResult::Recv: {
                            auto buf{ m_data->connectStateMachine->GetRecvBuffer(*m_data) };
#ifdef _WIN32
                            WSABUF wsaBuf{ static_cast<ULONG>(buf.size()), reinterpret_cast<char*>(buf.data()) };

                            if (WSARecv(m_data->socket, &wsaBuf, 1, nullptr, &m_flags, &m_status, nullptr) == macroSOCKET_ERROR) {
                                if (WSAGetLastError() != WSA_IO_PENDING) {
                                    auto sla{ WSAGetLastError };
                                    stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                }
                            }
#else
                            auto* scheduler = m_options.scheduler;
                            if (!scheduler) {
                                stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                                return;
                            }
                            scheduler->SubmitIo([this, buf](struct io_uring_sqe* sqe) {
                                io_uring_prep_recv(sqe, static_cast<int>(m_data->socket), buf.data(), buf.size(), 0);
                                io_uring_sqe_set_data(sqe, &m_status);
                            });
#endif
                            return;
                        }
                        case ConnectStateOpResult::Done:
                            stdexec::set_value(std::move(m_receiver));
                            return;
                        case ConnectStateOpResult::Error:
                            stdexec::set_error(std::move(m_receiver), m_data->connectStateMachine->GetResult().error_or(ConnectionErrorEnum::Unknown));
                            return;
                        case ConnectStateOpResult::Closed:
                            stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::Unknown);
                            return;
                    }
                }
            }

            static void IoCallback(void* context, LongIoCount bytesTransferred, const bool success) noexcept {
                auto* self = static_cast<OperationState*>(context);
                if (!success) {
                    // TODO: FUTURE: Map the error
                    stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::Unknown);
                    return;
                }

                self->m_data->connectStateMachine->SetIoResult(bytesTransferred);

                self->Pump();
            }

            void start() & noexcept {
                m_status.context = this;
                m_status.callback = IoCallback;

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

        return DefaultAsyncConnectPolicy<ExecutionContext, Data>::Connect(data, *reinterpret_cast<DefaultAsyncConnectPolicy<ExecutionContext, Data>::Options*>(&options))
             |
             stdexec::let_value(Utils::Overloaded{
                 [&data, options]() mutable { return ControlSender{ &data, options }; },
                 [](ConnectionErrorEnum e)  { return stdexec::just_error(e); }
             });
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