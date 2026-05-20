#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

namespace Hermes {

    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultAsyncConnectPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;


        struct Options : _details::ConnectOptionsIpv6Base<Data::Family>, _details::OptionsTcpNoDelayBase<Data::Type> {
            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};

            FastIoLoop* scheduler;
        };


        static constexpr bool IsServer{ false };


        static auto AsyncConnect(Data& data, Options options) noexcept;
        static auto AsyncShutdown(Data& data) noexcept;

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        struct ConnectSender;
        struct ShutdownSender;
    };
}

#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.tpp>

namespace Hermes {
    static_assert(AsyncConnectionPolicyConcept<DefaultAsyncConnectPolicy<>, DefaultSocketData<>>);
}