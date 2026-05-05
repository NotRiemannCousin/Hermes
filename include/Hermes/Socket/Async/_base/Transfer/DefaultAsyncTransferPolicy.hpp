#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/_base.hpp>

#include <stdexec/execution.hpp>
#include <span>

namespace Hermes {

    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultAsyncTransferPolicy {

        template<ByteLike Byte>
        auto AsyncRecv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        template<ByteLike Byte>
        auto AsyncSend(Data& data, std::span<const Byte> bufferSend) noexcept;

    private:

        template<ByteLike Byte>
        struct RecvSender;
        template<ByteLike Byte>
        struct SendSender;
    };


}

#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.tpp>

namespace Hermes {
    static_assert(AsyncTransferPolicyConcept<DefaultAsyncTransferPolicy, DefaultSocketData<>>);
}