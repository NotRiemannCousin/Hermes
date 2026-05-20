// ReSharper disable CppRedundantTypenameKeyword
#pragma once
#include <Hermes/_base/Network.hpp>

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
    return TransferStateOpResult::opResult;                       \
} while (false)

namespace Hermes::_details {

    template<SocketDataConcept Data, class TransferPolicy>
    constexpr bool TlsTransferStateMachine<Data, TransferPolicy>::S_HasData(const SecBuffer& buffer) noexcept {
        return buffer.cbBuffer > 0;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    typename TlsTransferStateMachine<Data, TransferPolicy>::TlsTransferState 
    TlsTransferStateMachine<Data, TransferPolicy>::GetState() const noexcept {
        return _state;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    StreamByteOper TlsTransferStateMachine<Data, TransferPolicy>::GetResult() const noexcept {
        return { _totalTransferred, _errorStatus };
    }

    template<SocketDataConcept Data, class TransferPolicy>
    bool TlsTransferStateMachine<Data, TransferPolicy>::IsFinished() const noexcept {
        return _state == &TlsTransferStateMachine::_DoneState || _state == &TlsTransferStateMachine::_ErrorState;
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::StartToRecv(std::span<std::byte> buffer, RecvModeEnum mode) noexcept {
        _userRecvBuffer   = buffer;
        _recvMode         = mode;
        _initialSize      = buffer.size();
        _totalTransferred = 0;
        SetToRecv();
    }

    template<SocketDataConcept Data, class TransferPolicy>
    void TlsTransferStateMachine<Data, TransferPolicy>::StartToSend(std::span<const std::byte> buffer) noexcept {
        _userSendBuffer   = buffer;
        _initialSize      = buffer.size();
        _totalTransferred = 0;
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
        if (!data.isHandshakeComplete) {
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
            _status = _tul(EncryptStatusEnum::ErrOk);
            NEXT(RecvProcessNetwork);
        }

        if (!extraSpan.empty() && _status != _tul(EncryptStatusEnum::ErrIncompleteMessage)) {
            std::swap(dataSpan, extraSpan);
            _currReceived = dataSpan.size();
            NEXT(RecvProcessNetwork);
        }

        if constexpr (IsAsync())
            AWAIT(RecvProcessNetwork, Recv);
        else {
            _currReceived = recv(data.socket, reinterpret_cast<char*>(data.state->decryptedData.data() + extraSpan.size()), static_cast<int>(data.state->decryptedData.size() - extraSpan.size()), 0);
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
        auto& decryptedData{ data.state->decryptedData };

        data.state->decryptedDataSpan = std::span<std::byte>{ decryptedData.data(), extraSpan.size() + _currReceived };
        extraSpan = {};

        _secBuffers[0] = { _tul(data.state->decryptedDataSpan.size()), _tul(SecurityBufferEnum::Data), data.state->decryptedDataSpan.data() };
        _secBuffers[1] = _secBuffers[2] = _secBuffers[3] = { 0, _tul(SecurityBufferEnum::Empty), nullptr };

        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, _secBuffers.data() };
        _status = DecryptMessage(&data.ctxtHandle, &buffDesc, 0, nullptr);

        NEXT(RecvDecrypt);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_RecvDecryptState(Data &data) {
        auto& dataSpan{ data.state->decryptedDataSpan };
        auto& extraSpan{ data.state->decryptedExtraSpan };
        auto& decryptedData{ data.state->decryptedData };

        switch (static_cast<EncryptStatusEnum>(_status)) {
            case EncryptStatusEnum::ErrOk: {
                if (dataSpan.empty() || _secBuffers[0].BufferType != _tul(SecurityBufferEnum::Data)) {
                    const auto dataBuffer{ std::ranges::find(_secBuffers, _tul(SecurityBufferEnum::Data), &SecBuffer::BufferType) };
                    const auto extraBuffer{ std::ranges::find(_secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };

                    if (dataBuffer == _secBuffers.end()) {
                        _errorStatus = std::unexpected{ ConnectionErrorEnum::Unknown };
                        NEXT(Error);
                    }

                    dataSpan = { static_cast<std::byte*>(dataBuffer->pvBuffer), dataBuffer->cbBuffer };

                    if (extraBuffer != _secBuffers.end() && S_HasData(*extraBuffer))
                        extraSpan = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                    else
                        extraSpan = { decryptedData.data(), 0 };

                    if (!S_HasData(*dataBuffer)) NEXT(RecvCheckPending);
                }

                const size_t countToCopy{ (std::min)(_userRecvBuffer.size(), dataSpan.size()) };
                std::memmove(_userRecvBuffer.data(), dataSpan.data(), countToCopy);

                _userRecvBuffer = _userRecvBuffer.subspan(countToCopy);
                dataSpan = dataSpan.subspan(countToCopy);
                _totalTransferred += countToCopy;

                if (dataSpan.empty() && !extraSpan.empty()) {
                    std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());
                    extraSpan = { decryptedData.data(), extraSpan.size() };
                }

                if (_userRecvBuffer.empty() || _recvMode == RecvModeEnum::Any) NEXT(Done);
                NEXT(RecvCheckPending);
            }
            case EncryptStatusEnum::ErrIncompleteMessage: {
                const auto extraBuffer{ std::ranges::find(_secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };
                const auto missingBuffer{ std::ranges::find(_secBuffers, _tul(SecurityBufferEnum::Missing), &SecBuffer::BufferType) };

                if (extraBuffer == _secBuffers.end()) {
                    if (missingBuffer == _secBuffers.end()) {
                        _errorStatus = std::unexpected{ ConnectionErrorEnum::Unknown };
                        NEXT(Error);
                    }
                    extraSpan = dataSpan;
                } else
                    extraSpan = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };

                std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());
                dataSpan  = {};
                extraSpan = std::span<std::byte>{ decryptedData.data(), extraSpan.size() };
                NEXT(RecvCheckPending);
            }
            case EncryptStatusEnum::InfoRenegotiate: {
                data.isHandshakeComplete = false;
                const auto extraBuffer{ std::ranges::find(_secBuffers, _tul(SecurityBufferEnum::Extra), &SecBuffer::BufferType) };

                if (extraBuffer != _secBuffers.end() && S_HasData(*extraBuffer))
                    extraSpan = { static_cast<std::byte*>(extraBuffer->pvBuffer), extraBuffer->cbBuffer };
                else
                    extraSpan = {};

                if (!extraSpan.empty()) {
                    std::memmove(decryptedData.data(), extraSpan.data(), extraSpan.size());
                    extraSpan = { decryptedData.data(), extraSpan.size() };
                }

                data.pendingData = static_cast<uint32_t>(extraSpan.size());
                _errorStatus = std::unexpected{ ConnectionErrorEnum::RenegotiationRequired };
                dataSpan = {};
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
        if (!data.isHandshakeComplete) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            NEXT(Error);
        }
        NEXT(SendChunk);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_SendChunkState(Data &data) {
        if (_totalTransferred >= _initialSize) NEXT(Done);

        const auto& sizes{ data.contextStreamSizes };
        const size_t remainingBytes{ _initialSize - _totalTransferred };
        _chunkSize = (std::min)(remainingBytes, static_cast<size_t>(sizes.cbMaximumMessage));

        const auto dwChunkSize{ static_cast<ULONG>(_chunkSize) };
        _secBuffers[0] = SecBuffer{ sizes.cbHeader , _tul(SecurityBufferEnum::StreamHeader) , static_cast<void*>(data.state->encryptedData.data()) };
        _secBuffers[1] = SecBuffer{ dwChunkSize    , _tul(SecurityBufferEnum::Data)         , static_cast<void*>(data.state->encryptedData.data() + sizes.cbHeader) };
        _secBuffers[2] = SecBuffer{ sizes.cbTrailer, _tul(SecurityBufferEnum::StreamTrailer), static_cast<void*>(data.state->encryptedData.data() + sizes.cbHeader + _chunkSize) };
        _secBuffers[3] = SecBuffer{ 0              , _tul(SecurityBufferEnum::Empty)        , nullptr };

        std::memcpy(_secBuffers[1].pvBuffer, _userSendBuffer.data() + _totalTransferred, _chunkSize);

        SecBufferDesc buffDesc{ macroSECBUFFER_VERSION, 4, _secBuffers.data() };
        const SECURITY_STATUS status{ EncryptMessage(&data.ctxtHandle, 0, &buffDesc, 0) };

        if (status != _tul(EncryptStatusEnum::ErrOk)) {
            if (static_cast<EncryptStatusEnum>(status) == EncryptStatusEnum::InfoContextExpired)
                _errorStatus = std::unexpected{ ConnectionErrorEnum::ConnectionClosed };
            else
                _errorStatus = std::unexpected{ ConnectionErrorEnum::SendFailed };
            NEXT(Error);
        }

        _encryptedSize = _secBuffers[0].cbBuffer + _secBuffers[1].cbBuffer + _secBuffers[2].cbBuffer;
        _sentBytes = 0;
        NEXT(SendNetworkWrite);
    }

    template<SocketDataConcept Data, class TransferPolicy>
    TransferStateOpResult
    TlsTransferStateMachine<Data, TransferPolicy>::_SendNetworkWriteState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(SendProcessNetwork, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(data.state->encryptedData.data() + _sentBytes), static_cast<int>(_encryptedSize - _sentBytes), 0);
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

        _totalTransferred += _chunkSize;
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