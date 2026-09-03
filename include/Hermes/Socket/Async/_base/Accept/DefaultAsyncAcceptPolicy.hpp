#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

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
#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = DefaultSocketData<>>
#else
    template<class ExecutionContext, SocketDataConcept Data = DefaultSocketData<>>
#endif
    struct DefaultAsyncAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = typename Data::EndpointType;

        struct ListenOptions : DefaultAcceptPolicy<EndpointType, Type, Family>::ListenOptions {
            ExecutionContext* scheduler;
        };

        struct AcceptOptions : DefaultAcceptPolicy<EndpointType, Type, Family>::AcceptOptions {
            ExecutionContext* scheduler;
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options);
        static auto Accept(Data& listenData, Data&& clientData, AcceptOptions options);
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
        };
#endif

        inline static std::mutex listenerExtensionsMutex;
        inline static std::unordered_map<SocketFd, ListenerExtensions> listenerExtensions;
    };
}

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncAcceptPolicyConcept<DefaultAsyncAcceptPolicy<>, DefaultSocketData<>>);
#endif
}

#endif