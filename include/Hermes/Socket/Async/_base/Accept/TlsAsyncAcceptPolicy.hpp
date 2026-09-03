#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<SocketDataConcept Data = TlsSocketData<>, stdexec::scheduler Scheduler = FastIoLoop>
#else
    template<SocketDataConcept Data = TlsSocketData<>, stdexec::scheduler Scheduler>
#endif
    struct TlsAsyncAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = typename Data::EndpointType;

        struct ListenOptions : TlsAcceptPolicy<Data>::ListenOptions {};

        struct AcceptOptions : TlsAcceptPolicy<Data>::AcceptOptions {
            Scheduler* scheduler{};
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        auto Accept(Data& listenData, Data&& clientData, AcceptOptions options);
        auto Accept(Data& listenData, AcceptOptions options);

        auto Renegotiate(Data& data);
        auto Shutdown(Data& data);

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        AcceptOptions m_options;
        struct ControlSender;
        struct AcceptSender;
    };
}

#include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncAcceptPolicyConcept<TlsAsyncAcceptPolicy<>, TlsSocketData<>>);
#endif
}

#endif