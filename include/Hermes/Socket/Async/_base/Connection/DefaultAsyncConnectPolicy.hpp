#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultAsyncConnectPolicy {
        struct Options : _details::ConnectOptionsIpv6Base<Data::Family>, _details::OptionsTcpNoDelayBase<Data::Type> {
            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};
        };

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
    static_assert(AsyncConnectionPolicyConcept<DefaultAsyncConnectPolicy, DefaultSocketData<>>);
}