#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

namespace Hermes {

#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<SocketDataConcept Data = TlsSocketData<>, stdexec::scheduler Scheduler = FastIoLoop>
#else
    template<SocketDataConcept Data = TlsSocketData<>, stdexec::scheduler Scheduler>
#endif
    struct TlsAsyncConnectPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = typename Data::EndpointType;


        struct Options : DefaultAsyncConnectPolicy<Data, Scheduler>::Options {
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
    static_assert(AsyncConnectionPolicyConcept<TlsAsyncConnectPolicy<>, TlsSocketData<>>);
#endif
}

#endif