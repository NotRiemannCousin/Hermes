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
    _state = &TlsTransferStateMachine::_##state##State; \
    return (this->*_state)(data);                       \
} while (false)

#define AWAIT(nextState, opResult) do {                     \
    _state = &TlsTransferStateMachine::_##nextState##State; \
    return TransferStateOpResult::opResult;                 \
} while (false)

namespace Hermes::_details {

    template<SocketDataConcept Data, class TransferPolicy>
    typename TlsTransferStateMachine<Data, TransferPolicy>::TlsTransferState
    TlsTransferStateMachine<Data, TransferPolicy>::GetState() const noexcept {
        return _state;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    StreamByteOper TlsTransferStateMachine<Data, TransferPolicy>::GetResult() const noexcept {
        return { _totalProcessed, _errorStatus };
    }

    template<SocketDataConcept Data, class TransferPolicy>
    bool TlsTransferStateMachine<Data, TransferPolicy>::IsFinished() const noexcept {
        return _state == &TlsTransferStateMachine::_DoneState ||
        _state == &TlsTransferStateMachine::_ErrorState;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::StartToRecv(std::span<std::byte> buffer, RecvModeEnum mode) noexcept {
        _userRecvBuffer   = buffer;
        _recvMode         = mode;
        _initialSize      = buffer.size();
        _totalProcessed = 0;
        SetToRecv();
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::StartToSend(std::span<const std::byte> buffer) noexcept {
        _userSendBuffer   = buffer;
        _initialSize      = buffer.size();
        _totalProcessed = 0;
        SetToSend();
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::SetToRecv() noexcept {
        _errorStatus = {};
        _state       = &TlsTransferStateMachine::_RecvSetupState;
    }
    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::SetToSend() noexcept {
        _errorStatus = {};
        _state       = &TlsTransferStateMachine::_SendSetupState;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::Advance(Data &data) noexcept {
        return (this->*_state)(data);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::SetIoResult(const int bytes) noexcept {
        _currSent     = bytes;
        _currReceived = bytes;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    std::span<std::byte> TlsTransferStateMachine<Data, TransferPolicy>::GetRecvBuffer(Data &data) const noexcept {
        auto& extraSpan{ data.state->decryptedExtraSpan };
        return { data.state->decryptedData.data() + extraSpan.size(), data.state->decryptedData.size() - extraSpan.size() };
    }

    template<SocketDataConcept Data, class TransferPolicy>
    std::span<const std::byte> TlsTransferStateMachine<Data, TransferPolicy>::GetSendBuffer(Data &data) const noexcept {
        return { static_cast<const std::byte*>(static_cast<const void*>(data.state->encryptedData.data())) + _sentBytes, _encryptedSize - _sentBytes };
    }

#pragma region Recv

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_RecvSetupState(Data &data) {
        if (!data.session.IsHandshakeComplete()) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            NEXT(Error);
        }
        NEXT(RecvCheckPending);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_RecvCheckPendingState(Data &data) {
        auto& dataSpan{ data.state->decryptedDataSpan };
        auto& extraSpan{ data.state->decryptedExtraSpan };

        if (!dataSpan.empty()) {
            const size_t countToCopy{ std::min(_userRecvBuffer.size(), dataSpan.size()) };
            std::memmove(_userRecvBuffer.data(), dataSpan.data(), countToCopy);

            _userRecvBuffer = _userRecvBuffer.subspan(countToCopy);
            dataSpan = dataSpan.subspan(countToCopy);
            _totalProcessed += countToCopy;

            if (!dataSpan.empty()) {
                std::memmove(data.state->decryptedData.data(), dataSpan.data(), dataSpan.size());
                dataSpan = { data.state->decryptedData.data(), dataSpan.size() };

                if (!extraSpan.empty()) {
                    std::memmove(data.state->decryptedData.data() + dataSpan.size(), extraSpan.data(), extraSpan.size());
                    extraSpan = { data.state->decryptedData.data() + dataSpan.size(), extraSpan.size() };
                }
                NEXT(Done);
            }

            if (_userRecvBuffer.empty() || _recvMode == RecvModeEnum::Any) {
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
            _currReceived = recv(data.socket, reinterpret_cast<char*>(data.state->decryptedData.data()), static_cast<int>(data.state->decryptedData.size()), 0);
            NEXT(RecvProcessNetwork);
        }
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_RecvProcessNetworkState(Data &data) {
        if (_currReceived == 0) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::ConnectionClosed };
            NEXT(Error);
        }
        if (_currReceived < 0) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::ReceiveFailed };
            NEXT(Error);
        }

        auto& extraSpan{ data.state->decryptedExtraSpan };
        data.state->decryptedDataSpan = std::span<std::byte>{ data.state->decryptedData.data(), extraSpan.size() + _currReceived };
        extraSpan = {};

        NEXT(RecvDecrypt);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_RecvDecryptState(Data &data) {
        auto outcome{ data.session.Decrypt(data.state->decryptedDataSpan) };
        _status = outcome.status;

        data.state->decryptedDataSpan = outcome.data;
        data.state->decryptedExtraSpan = outcome.extra;

        switch (_status) {
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
                    _currReceived = recv(data.socket, reinterpret_cast<char*>(data.state->decryptedData.data() + extraSpan.size()), static_cast<int>(data.state->decryptedData.size() - extraSpan.size()), 0);
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
                _errorStatus = std::unexpected{ ConnectionErrorEnum::RenegotiationRequired };
                data.state->decryptedDataSpan = {};
                NEXT(Error);
            }
            case EncryptStatusEnum::InfoContextExpired:
                NEXT(Done);
            default:
                _errorStatus = std::unexpected{ ConnectionErrorEnum::DecryptionFailed };
                NEXT(Error);
        }
    }

#pragma endregion

#pragma region Send

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_SendSetupState(Data &data) {
        if (!data.session.IsHandshakeComplete()) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            NEXT(Error);
        }
        NEXT(SendChunk);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_SendChunkState(Data &data) {
        if (_totalProcessed >= _initialSize) NEXT(Done);

        const size_t remainingBytes{ _initialSize - _totalProcessed };
        _chunkSize = std::min(remainingBytes, static_cast<size_t>(data.session.GetStreamSizes().maxMessage));


        const auto outcome{ data.session.Encrypt(
            _userSendBuffer.subspan(_totalProcessed, _chunkSize), std::span<std::byte>{ data.state->encryptedData }
        ) };
        _status = outcome.status;
        _encryptedSize = outcome.produced;
        _totalProcessed += _chunkSize;

        if (_status != EncryptStatusEnum::ErrOk) {
            if (_status == EncryptStatusEnum::InfoContextExpired)
                _errorStatus = std::unexpected{ ConnectionErrorEnum::ConnectionClosed };
            else
                _errorStatus = std::unexpected{ ConnectionErrorEnum::SendFailed };
            NEXT(Error);
        }

        _sentBytes = 0;
        NEXT(SendNetworkWrite);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_SendNetworkWriteState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(SendProcessNetwork, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(data.state->encryptedData.data() + _sentBytes), static_cast<int>(_encryptedSize - _sentBytes), MSG_NOSIGNAL);
            NEXT(SendProcessNetwork);
        }
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_SendProcessNetworkState(Data &data) {
        if (_currSent <= 0) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::SendFailed };
            NEXT(Error);
        }

        _sentBytes += static_cast<size_t>(_currSent);
        if (_sentBytes < _encryptedSize) {
            NEXT(SendNetworkWrite);
        }

        NEXT(SendChunk);
    }

#pragma endregion

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_ErrorState(Data &data) { return TransferStateOpResult::Error; }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_DoneState(Data &data) { return TransferStateOpResult::Done; }

}

#pragma pop_macro("AWAIT")
#pragma pop_macro("NEXT")
#pragma pop_macro("MSG_NOSIGNAL")