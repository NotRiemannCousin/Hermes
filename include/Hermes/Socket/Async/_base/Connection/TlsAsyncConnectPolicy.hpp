#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    //! @brief Client-side TLS policy returning stdexec senders.
    //! @details Connect() chains the async TCP connect with the handshake driven through the ExecutionContext.
    //! Renegotiate()/Shutdown() have no options parameter, they reuses the Options stored by the last Connect() call
    //! on this policy instance.
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncConnectPolicy {
        static_assert(requires (Data data) {
            { *data.connectStateMachine } -> std::same_as<details_::ITlsConnectStateMachine<Data>&>;
        }, "The socket data must have a valid TLS state machine to start connections.");

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


        struct Options : DefaultAsyncConnectPolicy<ExecutionContext, Data>::Options {
            bool ignoreCertificateErrors{};
            bool requestMutualAuth{};
        };


        static constexpr bool IsServer{};


        auto Connect(Data& data, Options options);
        auto Shutdown(Data& data);

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        Options m_options;
        struct ControlSender;
    };
}

#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncConnectionPolicyConcept<TlsAsyncConnectPolicy<>, TlsSocketData<FastIoLoop>>);
#endif
}

#endif