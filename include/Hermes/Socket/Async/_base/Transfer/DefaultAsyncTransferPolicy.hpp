#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/_base.hpp>

#include <span>

namespace Hermes {

    //! @brief Plain TCP transfer policy returning stdexec senders.
    //! @details Bounded transfers are expressed with STDEXEC cancellation instead of deadlines.
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncTransferPolicy {
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

        template<ByteLike Byte>
        auto Recv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        template<ByteLike Byte>
        auto Send(Data& data, std::span<const Byte> bufferSend) noexcept;

    private:

        template<ByteLike Byte>
        struct RecvSender;
        template<ByteLike Byte>
        struct SendSender;
    };


}

#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.tpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncTransferPolicyConcept<DefaultAsyncTransferPolicy<>, DefaultSocketData<FastIoLoop>>);
#endif
}

#endif