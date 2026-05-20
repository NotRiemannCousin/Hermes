#pragma once
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAsyncConnectPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;


        struct Options : DefaultAsyncConnectPolicy<Data>::Options {
            bool ignoreCertificateErrors{};
            bool requestMutualAuth{};
        };


        static constexpr bool IsServer{ false };


        auto AsyncConnect(Data& data, Options options);
        auto AsyncShutdown(Data& data);

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        Options _options;
        struct ControlSender;
    };
}

#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.tpp>

namespace Hermes {
    static_assert(AsyncConnectionPolicyConcept<TlsAsyncConnectPolicy<>, TlsSocketData<>>);
}