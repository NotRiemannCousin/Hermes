#pragma once
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAsyncConnectPolicy {
        struct Options : DefaultAsyncConnectPolicy<Data>::Options {
            bool ignoreCertificateErrors{};
            bool requestMutualAuth{};
        };

        auto AsyncConnect(Data& data, Options options) noexcept;
        auto AsyncShutdown(Data& data) noexcept;

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        struct HandshakeSender;
        struct ShutdownSender;
    };
}

#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.tpp>

namespace Hermes {
    static_assert(AsyncConnectionPolicyConcept<TlsAsyncConnectPolicy, TlsSocketData<>>);
}