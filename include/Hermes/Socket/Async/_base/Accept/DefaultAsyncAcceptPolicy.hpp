#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    //! @brief Plain TCP listen/accept policy returning stdexec senders.
    //! @details Listen() is synchronous and registers the listening socket with Options::scheduler. Accept() completes
    //! with the accepted SocketData in the value channel, socket and endpoint already filled.
    //! @note listenerExtensions (Windows AcceptEx pointers) is an implementation detail that is required on Windows,
    //! so it exposes it.
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy {
        static_assert(!std::same_as<ExecutionContext, std::monostate>,
            "Async policies require a real execution context, std::monostate is a fallback that generic SocketData "
            "uses in sync sockets only.");

        static_assert(requires (Data data) {
            { data.executionContext } -> std::same_as<ExecutionContext*&>;
        }, "The socket data must carry this policy's execution context (SocketData::executionContext).");

        static_assert(ExecutionContextConcept<ExecutionContext>,
            "ExecutionContext must satisfy Hermes::ExecutionContextConcept (RegisterHandle, SubmitIo, PostWork, GetScheduler).");


        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = Data::EndpointType;

        using ListenOptions = DefaultAcceptPolicy<EndpointType, Type, Family>::ListenOptions;
        using AcceptOptions = DefaultAcceptPolicy<EndpointType, Type, Family>::AcceptOptions;


        ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options);
        auto Accept(Data& listenData, Data&& clientData, AcceptOptions options);
        auto Accept(Data& listenData, AcceptOptions options);
        auto Shutdown(Data& data);

        void Close(Data& data) noexcept;
        void Abort(Data& data) noexcept;

        //! @brief Resolves the platform accept extensions for @p data into SocketData::extensions.
        //! @details Windows resolves the AcceptEx function pointers of the already-created socket; other platforms are
        //! a no-op. Listen() calls it for every listener; TlsAsyncAcceptPolicy::Listen() calls it too because its
        //! listen path bypasses this policy's Listen().
        ConnectionResultOper LoadAcceptExtensions(Data& data) noexcept;

        struct AcceptSender;
        struct ShutdownSender;

        AcceptExtensions extensions{};
    };
}

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncAcceptPolicyConcept<DefaultAsyncAcceptPolicy<>, DefaultSocketData<FastIoLoop>>);
#endif
}

#endif
