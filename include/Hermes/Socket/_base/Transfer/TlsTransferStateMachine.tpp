// ReSharper disable CppRedundantTypenameKeyword
#pragma once

#pragma push_macro("MSG_NOSIGNAL")
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#pragma push_macro("NEXT")
#pragma push_macro("AWAIT")
#undef NEXT
#undef AWAIT

#define NEXT(state) do {                                \
    m_state = &TlsTransferStateMachine::state##State; \
    return (this->*m_state)(data);                       \
} while (false)

#define AWAIT(nextState, opResult) do {                     \
    m_state = &TlsTransferStateMachine::nextState##State; \
    return TransferStateOpResult::opResult;                 \
} while (false)

namespace Hermes::details_ {

    template<SocketDataConcept Data, class TransferPolicy>
    typename TlsTransferStateMachine<Data, TransferPolicy>::TlsTransferState
    TlsTransferStateMachine<Data, TransferPolicy>::GetState() const noexcept {
        return m_state;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    StreamByteOper TlsTransferStateMachine<Data, TransferPolicy>::GetResult() const noexcept {
        return { m_totalProcessed, m_errorStatus };
    }

    template<SocketDataConcept Data, class TransferPolicy>
    bool TlsTransferStateMachine<Data, TransferPolicy>::IsFinished() const noexcept {
        return m_state == &TlsTransferStateMachine::DoneState ||
        m_state == &TlsTransferStateMachine::ErrorState;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::StartToRecv(std::span<std::byte> buffer, RecvModeEnum mode) noexcept {
        m_userRecvBuffer   = buffer;
        m_recvMode         = mode;
        m_initialSize      = buffer.size();
        m_totalProcessed = 0;
        SetToRecv();
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::StartToSend(std::span<const std::byte> buffer) noexcept {
        m_userSendBuffer   = buffer;
        m_initialSize      = buffer.size();
        m_totalProcessed = 0;
        SetToSend();
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::SetToRecv() noexcept {
        m_errorStatus = {};
        m_state       = &TlsTransferStateMachine::RecvSetupState;
    }
    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::SetToSend() noexcept {
        m_errorStatus = {};
        m_state       = &TlsTransferStateMachine::SendSetupState;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::Advance(Data &data) noexcept {
        return (this->*m_state)(data);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::SetIoResult(const int bytes) noexcept {
        m_currSent     = bytes;
        m_currReceived = bytes;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    std::span<std::byte> TlsTransferStateMachine<Data, TransferPolicy>::GetRecvBuffer(Data &data) const noexcept {
        auto& extraSpan{ data.state->decryptedExtraSpan };
        return { data.state->decryptedData.data() + extraSpan.size(), data.state->decryptedData.size() - extraSpan.size() };
    }

    template<SocketDataConcept Data, class TransferPolicy>
    std::span<const std::byte> TlsTransferStateMachine<Data, TransferPolicy>::GetSendBuffer(Data &data) const noexcept {
        return { static_cast<const std::byte*>(static_cast<const void*>(data.state->encryptedData.data())) + m_sentBytes, m_encryptedSize - m_sentBytes };
    }

#pragma region Recv

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::RecvSetupState(Data &data) {
        if (!data.session.IsHandshakeComplete()) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            NEXT(Error);
        }
        NEXT(RecvCheckPending);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::RecvCheckPendingState(Data &data) {
        auto& dataSpan{ data.state->decryptedDataSpan };
        auto& extraSpan{ data.state->decryptedExtraSpan };

        if (!dataSpan.empty()) {
            const size_t countToCopy{ std::min(m_userRecvBuffer.size(), dataSpan.size()) };
            std::memmove(m_userRecvBuffer.data(), dataSpan.data(), countToCopy);

            m_userRecvBuffer = m_userRecvBuffer.subspan(countToCopy);
            dataSpan = dataSpan.subspan(countToCopy);
            m_totalProcessed += countToCopy;

            if (!dataSpan.empty()) {
                std::memmove(data.state->decryptedData.data(), dataSpan.data(), dataSpan.size());
                dataSpan = { data.state->decryptedData.data(), dataSpan.size() };

                if (!extraSpan.empty()) {
                    std::memmove(data.state->decryptedData.data() + dataSpan.size(), extraSpan.data(), extraSpan.size());
                    extraSpan = { data.state->decryptedData.data() + dataSpan.size(), extraSpan.size() };
                }
                NEXT(Done);
            }

            if (m_userRecvBuffer.empty() || m_recvMode == RecvModeEnum::Any) {
                if (!extraSpan.empty()) {
                    std::memmove(data.state->decryptedData.data(), extraSpan.data(), extraSpan.size());
                    extraSpan = { data.state->decryptedData.data(), extraSpan.size() };
                }
                NEXT(Done);
            }
        }

        if (!extraSpan.empty()) {
            std::memmove(data.state->decryptedData.data(), extraSpan.data(), extraSpan.size());
            dataSpan = { data.state->decryptedData.data(), extraSpan.size() };
            extraSpan = {};
            NEXT(RecvDecrypt);
        }

        if constexpr (IsAsync())
            AWAIT(RecvProcessNetwork, Recv);
        else {
            m_currReceived = recv(data.socket, reinterpret_cast<char*>(data.state->decryptedData.data()), static_cast<int>(data.state->decryptedData.size()), 0);
            NEXT(RecvProcessNetwork);
        }
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::RecvProcessNetworkState(Data &data) {
        if (m_currReceived == 0) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::ConnectionClosed };
            NEXT(Error);
        }
        if (m_currReceived < 0) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::ReceiveFailed };
            NEXT(Error);
        }

        auto& extraSpan{ data.state->decryptedExtraSpan };
        data.state->decryptedDataSpan = std::span<std::byte>{ data.state->decryptedData.data(), extraSpan.size() + m_currReceived };
        extraSpan = {};

        NEXT(RecvDecrypt);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::RecvDecryptState(Data &data) {
        auto outcome{ data.session.Decrypt(data.state->decryptedDataSpan) };
        m_status = outcome.status;

        data.state->decryptedDataSpan = outcome.data;
        data.state->decryptedExtraSpan = outcome.extra;

        switch (m_status) {
            case EncryptStatusEnum::ErrOk:
                NEXT(RecvCheckPending);

            case EncryptStatusEnum::ErrIncompleteMessage: {
                auto& extraSpan{ data.state->decryptedExtraSpan };
                if (!extraSpan.empty()) {
                    std::memmove(data.state->decryptedData.data(), extraSpan.data(), extraSpan.size());
                    extraSpan = { data.state->decryptedData.data(), extraSpan.size() };
                }

                if constexpr (IsAsync())
                    AWAIT(RecvProcessNetwork, Recv);
                else {
                    m_currReceived = recv(data.socket, reinterpret_cast<char*>(data.state->decryptedData.data() + extraSpan.size()), static_cast<int>(data.state->decryptedData.size() - extraSpan.size()), 0);
                    NEXT(RecvProcessNetwork);
                }
            }
            case EncryptStatusEnum::InfoRenegotiate: {
                auto& extraSpan{ data.state->decryptedExtraSpan };
                if (!extraSpan.empty()) {
                    std::memmove(data.state->decryptedData.data(), extraSpan.data(), extraSpan.size());
                    extraSpan = { data.state->decryptedData.data(), extraSpan.size() };
                }

                data.pendingData = static_cast<uint32_t>(extraSpan.size());
                m_errorStatus = std::unexpected{ ConnectionErrorEnum::RenegotiationRequired };
                data.state->decryptedDataSpan = {};
                NEXT(Error);
            }
            case EncryptStatusEnum::InfoContextExpired:
                NEXT(Done);
            default:
                m_errorStatus = std::unexpected{ ConnectionErrorEnum::DecryptionFailed };
                NEXT(Error);
        }
    }

#pragma endregion

#pragma region Send

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::SendSetupState(Data &data) {
        if (!data.session.IsHandshakeComplete()) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            NEXT(Error);
        }
        NEXT(SendChunk);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::SendChunkState(Data &data) {
        if (m_totalProcessed >= m_initialSize) NEXT(Done);

        const size_t remainingBytes{ m_initialSize - m_totalProcessed };
        m_chunkSize = std::min(remainingBytes, static_cast<size_t>(data.session.GetStreamSizes().maxMessage));


        const auto outcome{ data.session.Encrypt(
            m_userSendBuffer.subspan(m_totalProcessed, m_chunkSize), std::span<std::byte>{ data.state->encryptedData }
        ) };
        m_status = outcome.status;
        m_encryptedSize = outcome.produced;
        m_totalProcessed += m_chunkSize;

        if (m_status != EncryptStatusEnum::ErrOk) {
            if (m_status == EncryptStatusEnum::InfoContextExpired)
                m_errorStatus = std::unexpected{ ConnectionErrorEnum::ConnectionClosed };
            else
                m_errorStatus = std::unexpected{ ConnectionErrorEnum::SendFailed };
            NEXT(Error);
        }

        m_sentBytes = 0;
        NEXT(SendNetworkWrite);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::SendNetworkWriteState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(SendProcessNetwork, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(data.state->encryptedData.data() + m_sentBytes), static_cast<int>(m_encryptedSize - m_sentBytes), MSG_NOSIGNAL);
            NEXT(SendProcessNetwork);
        }
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::SendProcessNetworkState(Data &data) {
        if (m_currSent <= 0) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::SendFailed };
            NEXT(Error);
        }

        m_sentBytes += static_cast<size_t>(m_currSent);
        if (m_sentBytes < m_encryptedSize) {
            NEXT(SendNetworkWrite);
        }

        NEXT(SendChunk);
    }

#pragma endregion

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::ErrorState(Data &data) { return TransferStateOpResult::Error; }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::DoneState(Data &data) { return TransferStateOpResult::Done; }

}

#pragma pop_macro("AWAIT")
#pragma pop_macro("NEXT")
#pragma pop_macro("MSG_NOSIGNAL")