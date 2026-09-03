#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/_base.hpp>

#include <stdexec/execution.hpp>
#include <span>

namespace Hermes {

    #if HERMES_ENABLE_NATIVE_SCHEDULER
    template<SocketDataConcept Data = DefaultSocketData<>, stdexec::scheduler Scheduler = FastIoLoop>
#else
    template<SocketDataConcept Data = DefaultSocketData<>, stdexec::scheduler Scheduler>
#endif
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