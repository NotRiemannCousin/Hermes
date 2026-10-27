#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    //! @brief Plain TCP connect policy returning stdexec senders.
    //! @details Options::scheduler is REQUIRED. if absent it fails the operation with ConnectionErrorEnum::NoScheduler.
    //! Close()/Abort() are synchronous and never touch the scheduler.
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy {
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


        struct Options : details_::ConnectOptionsIpv6Base<Data::Family>, details_::OptionsTcpNoDelayBase<Data::Type> {
            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};
        };


        static constexpr bool IsServer{};


        static auto Connect(Data& data, Options options) noexcept;
        static auto Shutdown(Data& data) noexcept;

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        struct ConnectSender;
        struct ShutdownSender;
    };
}

#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncConnectionPolicyConcept<DefaultAsyncConnectPolicy<>, DefaultSocketData<FastIoLoop>>);
#endif
}

#endif