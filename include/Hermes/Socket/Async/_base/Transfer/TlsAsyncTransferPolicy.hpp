#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_TLS

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>
#include <span>

namespace Hermes {

    //! @brief TLS transfer policy returning stdexec senders.
    //! @details Recv()/Send() drive the TLS transfer state machine through the ExecutionContext. Mid-transfer
    //! renegotiation stores accumulated bytes and runs the connect/accept state machine's handshake. If the
    //! renegotiation fails the transference fails to (obviously).
    //! @note Value channel carries the total bytes transferred (across renegotiations) and the error channel carries
    //! TransferError with the partial count. Send() targets the whole range, while Recv() honors RecvModeEnum.
    template<class ExecutionContext, SocketDataConcept Data>
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
    static_assert(AsyncTransferPolicyConcept<TlsAsyncTransferPolicy<>, TlsSocketData<>>);
#endif
}

#endif
