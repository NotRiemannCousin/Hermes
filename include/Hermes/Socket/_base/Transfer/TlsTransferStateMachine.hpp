#pragma once
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/_base/Transfer/ITlsTransferStateMachine.hpp>
#include <Hermes/Socket/_base/TlsSession.hpp>

namespace Hermes::_details {
    template<SocketDataConcept Data, class TransferPolicy>
    struct TlsTransferStateMachine : public ITlsTransferStateMachine<Data> {
        using TlsTransferState = TransferStateOpResult(TlsTransferStateMachine::*)(Data&);

        static constexpr bool IsAsync() noexcept {
            return AsyncTransferPolicyConcept<TransferPolicy, Data>;
        }

        TlsTransferStateMachine() = default;

        TlsTransferStateMachine(const TlsTransferStateMachine&) = delete;
        TlsTransferStateMachine& operator=(const TlsTransferStateMachine&) = delete;

        TlsTransferStateMachine(TlsTransferStateMachine&&) = delete;
        TlsTransferStateMachine& operator=(TlsTransferStateMachine&&) = delete;

        TlsTransferState GetState() const noexcept;
        StreamByteOper GetResult() const noexcept override;
        bool IsFinished() const noexcept override;

        void StartToRecv(std::span<std::byte> buffer, RecvModeEnum mode) noexcept override;
        void StartToSend(std::span<const std::byte> buffer) noexcept override;

        void SetToRecv() noexcept override;
        void SetToSend() noexcept override;

        TransferStateOpResult Advance(Data &data) noexcept override;

        void SetIoResult(int bytes) noexcept override;

        std::span<std::byte> GetRecvBuffer(Data& data) const noexcept override;
        std::span<const std::byte> GetSendBuffer(Data& data) const noexcept override;

    private:
        TlsTransferState _state{ nullptr };
        ConnectionResultOper _errorStatus{};

        int _currReceived{};
        int _currSent{};

        std::span<std::byte> _userRecvBuffer{};
        std::span<const std::byte> _userSendBuffer{};
        RecvModeEnum _recvMode{ RecvModeEnum::All };

        size_t _initialSize{};
        size_t _totalTransferred{};

        size_t _chunkSize{};
        size_t _encryptedSize{};
        size_t _sentBytes{};

        EncryptStatusEnum _status{};

        // Recv Branch
        TransferStateOpResult _RecvSetupState(Data &data);
        TransferStateOpResult _RecvCheckPendingState(Data &data);
        TransferStateOpResult _RecvProcessNetworkState(Data &data);
        TransferStateOpResult _RecvDecryptState(Data &data);

        // Send Branch
        TransferStateOpResult _SendSetupState(Data &data);
        TransferStateOpResult _SendChunkState(Data &data);
        TransferStateOpResult _SendNetworkWriteState(Data &data);
        TransferStateOpResult _SendProcessNetworkState(Data &data);

        // Endings
        TransferStateOpResult _ErrorState(Data &data);
        TransferStateOpResult _DoneState(Data &data);
    };
}

#include <Hermes/Socket/_base/Transfer/TlsTransferStateMachine.tpp>