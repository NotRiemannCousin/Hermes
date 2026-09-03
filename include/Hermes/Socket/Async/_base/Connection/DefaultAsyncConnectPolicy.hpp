#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

namespace Hermes {

#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = DefaultSocketData<>>
#else
    template<class ExecutionContext, SocketDataConcept Data = DefaultSocketData<>>
#endif
    struct DefaultAsyncConnectPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = typename Data::EndpointType;


        struct Options : details_::ConnectOptionsIpv6Base<Data::Family>, details_::OptionsTcpNoDelayBase<Data::Type> {
            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};

            ExecutionContext* scheduler;
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
    static_assert(AsyncConnectionPolicyConcept<DefaultAsyncConnectPolicy<>, DefaultSocketData<>>);
#endif
}

#endif