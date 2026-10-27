#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    //! @brief Server-side TLS policy returning stdexec senders.
    //! @details Accept() chains the async TCP accept with the TLS server handshake and completes with the
    //! handshake-completed SocketData. Renegotiate()/Shutdown() reuse stored options, same contract as the connect
    //! policy. The TCP accept stage runs on the embedded DefaultAsyncAcceptPolicy instance, which owns the listener's
    //! Windows accept extensions loaded by Listen().
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy {
        static_assert(requires (Data data) {
            { *data.acceptStateMachine } -> std::same_as<details_::ITlsAcceptStateMachine<Data>&>;
        }, "The socket data must have a valid TLS state machine to accept requests.");

        static_assert(!std::same_as<ExecutionContext, std::monostate>,
            "Async policies require a real execution context, std::monostate is a fallback that generic SocketData "
            "uses in sync sockets only.");

        static_assert(requires (Data data) {
            { data.executionContext } -> std::same_as<ExecutionContext*&>;
        }, "The socket data must carry this policy's execution context (SocketData::executionContext).");

        static_assert(ExecutionContextConcept<ExecutionContext>,
            "ExecutionContext must satisfy Hermes::ExecutionContextConcept (RegisterHandle, SubmitIo, PostWork, GetScheduler).");

        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = Data::EndpointType;

        using ListenOptions = TlsAcceptPolicy<Data>::ListenOptions;
        using AcceptOptions = TlsAcceptPolicy<Data>::AcceptOptions;

        ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        auto Accept(Data& listenData, Data&& clientData, AcceptOptions options);
        auto Accept(Data& listenData, AcceptOptions options);

        auto Renegotiate(Data& data);
        auto Shutdown(Data& data);

        void Close(Data& data) noexcept;
        void Abort(Data& data) noexcept;

    private:
        AcceptOptions m_options;
        DefaultAsyncAcceptPolicy<ExecutionContext, Data> m_tcp;
        struct ControlSender;
        struct AcceptSender;
    };
}

#include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncAcceptPolicyConcept<TlsAsyncAcceptPolicy<>, TlsSocketData<FastIoLoop>>);
#endif
}

#endif
