#pragma once
#include <Hermes/Socket/_base.hpp>
#include <span>
#include <cstdint>

namespace Hermes::_details {
    enum class TransferStateOpResult : std::uint8_t { Recv, Send, Error, Done };

    template<typename Data>
    struct ITlsTransferStateMachine {
        virtual ~ITlsTransferStateMachine() = default;

        virtual void StartToRecv(std::span<std::byte> buffer, RecvModeEnum mode) noexcept = 0;
        virtual void StartToSend(std::span<const std::byte> buffer) noexcept = 0;

        virtual void SetToRecv() noexcept = 0;
        virtual void SetToSend() noexcept = 0;

        virtual TransferStateOpResult Advance(Data& data) noexcept = 0;
        virtual StreamByteOper GetResult() const noexcept = 0;
        virtual bool IsFinished() const noexcept = 0;

        virtual void SetIoResult(int bytes) noexcept = 0;

        virtual std::span<std::byte> GetRecvBuffer(Data& data) const noexcept = 0;
        virtual std::span<const std::byte> GetSendBuffer(Data& data) const noexcept = 0;
    };
}