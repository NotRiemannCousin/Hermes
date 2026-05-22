#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>
#include <MSWSock.h>
#include <unordered_map>
#include <mutex>

namespace Hermes {

    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultAsyncAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;

        struct ListenOptions : DefaultAcceptPolicy<EndpointType, Type, Family>::ListenOptions {
            FastIoLoop* scheduler;
        };

        struct AcceptOptions : DefaultAcceptPolicy<EndpointType, Type, Family>::AcceptOptions {
            FastIoLoop* scheduler;
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options);
        static auto AsyncAccept(Data& listenData, AcceptOptions options);
        static auto AsyncShutdown(Data& data);

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    public:
        struct AcceptSender;
        struct ShutdownSender;

        struct ListenerExtensions {
            LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
            LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = nullptr;
        };

        inline static std::mutex s_listenerExtensionsMutex;
        inline static std::unordered_map<SOCKET, ListenerExtensions> s_listenerExtensions;
    };
}

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AsyncAcceptPolicyConcept<DefaultAsyncAcceptPolicy<>, DefaultSocketData<>>);
}
