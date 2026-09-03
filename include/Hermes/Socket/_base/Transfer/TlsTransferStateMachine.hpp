#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_TLS

#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/_base/Transfer/ITlsTransferStateMachine.hpp>
#include <Hermes/Socket/_base/TlsSession.hpp>

namespace Hermes::details_ {
    template<SocketDataConcept Data, class TransferPolicy>
    struct TlsTransferStateMachine : public ITlsTransferStateMachine<Data> {
        using TlsTransferState = TransferStateOpResult(TlsTransferStateMachine::*)(Data&);

#if HERMES_ENABLE_ASYNC
        static constexpr bool IsAsync() noexcept {
            return AsyncTransferPolicyConcept<TransferPolicy, Data>;
        }
#else
        static constexpr bool IsAsync() { return false; }
#endif

        TlsTransferStateMachine() = default;

        TlsTransferStateMachine(const TlsTransferStateMachine&) = delete;
        TlsTransferStateMachine& operator=(const TlsTransferStateMachine&) = delete;

        TlsTransferStateMachine(TlsTransferStateMachine&&) = delete;
        TlsTransferStateMachine& operator=(TlsTransferStateMachine&&) = delete;

        TlsTransferState GetState() const noexcept;
        StreamByteOper GetResult() const noexcept override;
        bool IsFinished() const noexcept override;

        void StartToRecv(
            std::span<std::byte> buffer,
            RecvModeEnum mode,
            std::optional<TransferDeadline> deadline
        ) noexcept override;
        void StartToSend(
            std::span<const std::byte> buffer,
            std::optional<TransferDeadline> deadline
        ) noexcept override;

        void SetToRecv() noexcept override;
        void SetToSend() noexcept override;

        TransferStateOpResult Advance(Data &data) noexcept override;

        void SetIoResult(int bytes) noexcept override;

        std::span<std::byte> GetRecvBuffer(Data& data) const noexcept override;
        std::span<const std::byte> GetSendBuffer(Data& data) const noexcept override;

    private:
        TlsTransferState m_state{ nullptr };
        ConnectionResultOper m_errorStatus{};

        int m_currReceived{};
        int m_currSent{};

        std::span<std::byte> m_userRecvBuffer{};
        std::span<const std::byte> m_userSendBuffer{};
        RecvModeEnum m_recvMode{ RecvModeEnum::All };

        size_t m_initialSize{};
        size_t m_totalProcessed{};

        size_t m_chunkSize{};
        size_t m_encryptedSize{};
        size_t m_sentBytes{};
        std::optional<TransferDeadline> m_deadline{};

        EncryptStatusEnum m_status{};

        // Recv Branch
        TransferStateOpResult RecvSetupState(Data &data);
        TransferStateOpResult RecvCheckPendingState(Data &data);
        TransferStateOpResult RecvProcessNetworkState(Data &data);
        TransferStateOpResult RecvDecryptState(Data &data);

        // Send Branch
        TransferStateOpResult SendSetupState(Data &data);
        TransferStateOpResult SendChunkState(Data &data);
        TransferStateOpResult SendNetworkWriteState(Data &data);
        TransferStateOpResult SendProcessNetworkState(Data &data);

        // Endings
        TransferStateOpResult ErrorState(Data &data);
        TransferStateOpResult DoneState(Data &data);
    };
}

#include <Hermes/Socket/_base/Transfer/TlsTransferStateMachine.tpp>

#endif