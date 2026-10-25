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
        static constexpr auto Type{ Data::Type };

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
    static_assert(AsyncTransferPolicyConcept<DefaultAsyncTransferPolicy<>, DefaultSocketData<>>);
#endif
}

#endif