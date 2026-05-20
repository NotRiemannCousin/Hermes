#pragma once
#include <Hermes/Socket/_base.hpp>
#include <span>
#include <cstdint>

namespace Hermes::_details {
    enum class AcceptStateOpResult : std::uint8_t { Recv, Send, Close, Error, Done, Closed };

    template<typename Data>
    struct ITlsAcceptStateMachine {
        virtual ~ITlsAcceptStateMachine() = default;
        virtual bool IsFinished() const noexcept = 0;
        virtual AcceptStateOpResult Advance(Data& data) noexcept = 0;
        virtual ConnectionResultOper GetResult() const noexcept = 0;

        virtual void SetToOpen()  noexcept = 0;
        virtual void SetToClose() noexcept = 0;
        virtual void SetToAbort() noexcept = 0;

        virtual void SetIoResult(int bytes) noexcept = 0;
        virtual std::span<std::byte> GetRecvBuffer(Data& data) noexcept = 0;
        virtual std::span<const std::byte> GetSendBuffer() noexcept = 0;
    };
}