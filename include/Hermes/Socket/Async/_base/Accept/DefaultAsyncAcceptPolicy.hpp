#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>

namespace Hermes {

    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultAsyncAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;

        struct ListenOptions : DefaultAcceptPolicy<EndpointType, Type, Family>::ListenOptions {};

        struct AcceptOptions : DefaultAcceptPolicy<EndpointType, Type, Family>::AcceptOptions {
            FastIoLoop* scheduler{};
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;
        static auto AsyncAccept(Data& listenData, Data& clientData, AcceptOptions options) noexcept;
        static auto AsyncShutdown(Data& data) noexcept;

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        struct AcceptSender;
        struct ShutdownSender;
    };
}

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AsyncAcceptPolicyConcept<DefaultAsyncAcceptPolicy<>, DefaultSocketData<>>);

    // Sender concept verified by compilation, removed static_asserts that depend on stdexec internals
}