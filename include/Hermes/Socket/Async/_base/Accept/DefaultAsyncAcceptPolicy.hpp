#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>
#ifdef _WIN32
#include <MSWSock.h>
#else

#endif
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
        static auto Accept(Data& listenData, Data& clientData, AcceptOptions options);
        static auto Accept(Data& listenData, AcceptOptions options);
        static auto Shutdown(Data& data);

        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    public:
        struct AcceptSender;
        struct ShutdownSender;

#ifdef _WIN32
        struct ListenerExtensions {
            LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
            LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = nullptr;
        };
#else
        struct ListenerExtensions {
            // linux options
        };
#endif


        inline static std::mutex s_listenerExtensionsMutex;
        inline static std::unordered_map<SocketFd, ListenerExtensions> s_listenerExtensions;
    };
}

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AsyncAcceptPolicyConcept<DefaultAsyncAcceptPolicy<>, DefaultSocketData<>>);
}