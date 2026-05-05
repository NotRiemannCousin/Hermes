#pragma once
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>
#include <span>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAsyncTransferPolicy {

        template<ByteLike Byte>
        auto AsyncRecv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum recvMode = RecvModeEnum::All) noexcept;

        template<ByteLike Byte>
        auto AsyncSend(Data& data, std::span<const Byte> bufferSend) noexcept;

    private:
        template<ByteLike Byte> struct RecvSender;
        template<ByteLike Byte> struct SendSender;
    };
}

#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.tpp>

namespace Hermes {
    static_assert(AsyncTransferPolicyConcept<TlsAsyncTransferPolicy, TlsSocketData<>>);
}