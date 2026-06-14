#pragma once
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAsyncAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;

        struct ListenOptions : TlsAcceptPolicy<Data>::ListenOptions {};

        struct AcceptOptions : TlsAcceptPolicy<Data>::AcceptOptions {
            FastIoLoop* scheduler{};
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        auto Accept(Data& listenData, Data& clientData, AcceptOptions options);
        auto Accept(Data& listenData, AcceptOptions options) { return Accept(listenData, listenData, options); }

        auto Renegotiate(Data& data);
        auto Shutdown(Data& data);

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        AcceptOptions _options;
        struct ControlSender;
    };
}

#include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AsyncAcceptPolicyConcept<TlsAsyncAcceptPolicy<>, TlsSocketData<>>);
}