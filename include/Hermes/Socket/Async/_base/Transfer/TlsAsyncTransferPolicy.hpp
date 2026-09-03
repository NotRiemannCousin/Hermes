#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>
#include <span>

namespace Hermes {

    #if HERMES_ENABLE_NATIVE_SCHEDULER
    template<SocketDataConcept Data = TlsSocketData<>, stdexec::scheduler Scheduler = FastIoLoop>
#else
    template<SocketDataConcept Data = TlsSocketData<>, stdexec::scheduler Scheduler>
#endif
    struct TlsAsyncTransferPolicy {
        static constexpr auto Type{ Data::Type };

        template<ByteLike Byte>
        auto Recv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum recvMode = RecvModeEnum::All) noexcept;

        template<ByteLike Byte>
        auto Send(Data& data, std::span<const Byte> bufferSend) noexcept;

        enum class ActionEnum : std::uint8_t { Recv, Send };
    private:

        template<ByteLike Byte> struct TransferSender;
    };
}

#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.tpp>

namespace Hermes {
    #if HERMES_ENABLE_NATIVE_SCHEDULER
#if HERMES_ENABLE_NATIVE_SCHEDULER
    static_assert(AsyncTransferPolicyConcept<TlsAsyncTransferPolicy<>, TlsSocketData<>>);
#endif
#endif
}

#endif