#pragma once
#include <Hermes/_base/Network.hpp>

#pragma push_macro("NEXT")
#pragma push_macro("AWAIT")
#undef NEXT
#undef AWAIT

#define NEXT(state) do {                               \
    _state = &TlsAcceptStateMachine::_##state##State;  \
    return (this->*_state)(data);                      \
} while (false)

#define AWAIT(nextState, opResult) do {                    \
    _state = &TlsAcceptStateMachine::_##nextState##State;  \
    return AcceptStateOpResult::opResult;                  \
} while (false)

namespace Hermes::_details {
#pragma region Helpers

    template<SocketDataConcept Data, class AcceptPolicy>
    TlsAcceptStateMachine<Data, AcceptPolicy>::TlsAcceptStateMachine(typename AcceptPolicy::AcceptOptions opt)
        : _options{ std::move(opt) } {}

    template<SocketDataConcept Data, class AcceptPolicy>
    typename TlsAcceptStateMachine<Data, AcceptPolicy>::TlsAcceptState TlsAcceptStateMachine<Data, AcceptPolicy>::GetState() const noexcept {
        return _state;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    ConnectionResultOper TlsAcceptStateMachine<Data, AcceptPolicy>::GetResult() const noexcept {
        return _errorStatus;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    bool TlsAcceptStateMachine<Data, AcceptPolicy>::IsFinished() const noexcept {
        return _state == &TlsAcceptStateMachine::_HandshakeCompletedState
            || _state == &TlsAcceptStateMachine::_EndCloseState
            || _state == &TlsAcceptStateMachine::_AbortState
            || !_errorStatus;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetToClose() noexcept {
        _errorStatus = {};
        _state       = &TlsAcceptStateMachine::_StartCloseState;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetToAbort() noexcept {
        _errorStatus = {};
        _state       = &TlsAcceptStateMachine::_AbortState;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetToOpen() noexcept {
        _errorStatus = {};
        _state       = &TlsAcceptStateMachine::_SetupState;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
        TlsAcceptStateMachine<Data, AcceptPolicy>::Advance(Data &data) noexcept {
        return (this->*_state)(data);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetIoResult(const int bytes) noexcept {
        _currSent = bytes;
        _currReceived = bytes;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    std::span<std::byte> TlsAcceptStateMachine<Data, AcceptPolicy>::GetRecvBuffer(Data &data) noexcept {
        return { data.state->decryptedData.data() + _receivedBytes, data.state->decryptedData.size() - _receivedBytes };
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    std::span<const std::byte> TlsAcceptStateMachine<Data, AcceptPolicy>::GetSendBuffer() noexcept {
        return { _outBuffer.data(), _outSize };
    }

#pragma endregion

#pragma region Setup

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_SetupState(Data &data) {

#pragma region fast-fail and initialization

        if (data.credentials == nullptr) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
            return AcceptStateOpResult::Error;
        }
        if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();

        data.session.BeginServer(*data.credentials, _options.requestClientCertificate);

        _receivedBytes   = data.pendingData;
        data.pendingData = 0;
        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, _options.handshakeTimeout.count());
#pragma endregion

        if (_receivedBytes > 0)
            NEXT(AcceptContext);
        NEXT(Recv);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_AcceptContextState(Data &data) {

#pragma region AcceptContext

        auto outcome = data.session.AdvanceHandshake(
            std::span<std::byte>{data.state->decryptedData.data(), _receivedBytes},
            std::span<std::byte>{_outBuffer}
        );
        _status = outcome.status;
        _outSize = outcome.produced;

        if (outcome.consumed > 0 && outcome.consumed <= _receivedBytes) {
            std::memmove(data.state->decryptedData.data(),
                         data.state->decryptedData.data() + outcome.consumed,
                         _receivedBytes - outcome.consumed);
            _receivedBytes -= outcome.consumed;
        }

#pragma endregion

        switch (_status) {
            case EncryptStatusEnum::InfoContinueNeeded:
                if (_outSize > 0) NEXT(Send);
                NEXT(CheckSend);
            case EncryptStatusEnum::ErrIncompleteMessage:           NEXT(CheckSend);
            case EncryptStatusEnum::ErrOk:
                if (_outSize > 0) NEXT(FinalSend);
                NEXT(HandshakeCompleted);

            case EncryptStatusEnum::ErrIncompleteCredentials:       NEXT(InvalidCertificateError);
            case EncryptStatusEnum::ErrInsufficientMemory:
            case EncryptStatusEnum::ErrInvalidHandle:
            case EncryptStatusEnum::ErrInvalidToken:                NEXT(HandshakeFailedError);
            case EncryptStatusEnum::ErrLogonDenied:
            case EncryptStatusEnum::ErrCertUnknown:
            case EncryptStatusEnum::ErrCertExpired:
            case EncryptStatusEnum::ErrNoCredentials:
            case EncryptStatusEnum::ErrUntrustedRoot:
            case EncryptStatusEnum::ErrNoAuthenticatingAuthority:   NEXT(InvalidCertificateError);
            default:                                                NEXT(UnknownError);
        }
    }

#pragma endregion

#pragma region Incomplete

#pragma region Send

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_SendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckSend, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(_outBuffer.data()), _outSize, 0);
            NEXT(CheckSend);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_CheckSendState(Data &data) {
        if (_outSize > 0) {
            if (_currSent == macroSOCKET_ERROR)
                NEXT(HandshakeFailedError);

            if (static_cast<std::uint32_t>(_currSent) != _outSize) {
                std::memmove(_outBuffer.data(), _outBuffer.data() + _currSent, _outSize - _currSent);
                _outSize -= _currSent;
                NEXT(Send);
            }
            _outSize = 0;
        }

        if (_status == EncryptStatusEnum::ErrIncompleteMessage || _receivedBytes == 0)
            NEXT(Recv);
        NEXT(AcceptContext);
    }

#pragma endregion

#pragma region Recv

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_RecvState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckRecv, Recv);
        } else {
            _currReceived = recv(data.socket,
                reinterpret_cast<char*>(data.state->decryptedData.data() + _receivedBytes),
                data.state->decryptedData.size() - _receivedBytes, 0);
            NEXT(CheckRecv);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_CheckRecvState(Data &data) {
        if (_currReceived <= 0)
            NEXT(HandshakeFailedError);
        _receivedBytes += _currReceived;
        NEXT(AcceptContext);
    }

#pragma endregion

#pragma endregion

#pragma region Complete

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_FinalSendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckFinalSend, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(_outBuffer.data()), _outSize, 0);
            NEXT(CheckFinalSend);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_CheckFinalSendState(Data &data) {
        if (_outSize > 0) {
            if (_currSent == macroSOCKET_ERROR)
                NEXT(HandshakeFailedError);

            if (static_cast<std::uint32_t>(_currSent) != _outSize) {
                std::memmove(_outBuffer.data(), _outBuffer.data() + _currSent, _outSize - _currSent);
                _outSize -= _currSent;
                NEXT(FinalSend);
            }
            _outSize = 0;
        }

        NEXT(HandshakeCompleted);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_HandshakeCompletedState(Data &data) {
        _status = EncryptStatusEnum::ErrOk;

        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, 0);

        std::size_t extraBufferSize{ static_cast<std::size_t>(_receivedBytes) };
        data.state->decryptedExtraSpan = std::span<std::byte>{
            data.state->decryptedData.data(),
            extraBufferSize
        };
        return AcceptStateOpResult::Done;
    }

#pragma endregion

#pragma region Errors

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_HandshakeFailedErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
        NEXT(Cleanup);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_InvalidCertificateErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
        NEXT(Cleanup);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_UnknownErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::Unknown };
        NEXT(Cleanup);
    }

#pragma endregion

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_CleanupState(Data &data) {
        data.session.DeleteContext();
        NEXT(StartClose);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_StartCloseState(Data &data) {
        if (data.socket == macroINVALID_SOCKET) return AcceptStateOpResult::Error;

        if (!data.session.IsHandshakeComplete())
            NEXT(EndClose);

        auto produced = data.session.Shutdown(std::span<std::byte>{_outBuffer});
        if (produced > 0) {
            _outSize = produced;
            NEXT(SendCloseNotify);
        }

        NEXT(DeleteSecurityContext);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_SendCloseNotifyState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(DeleteSecurityContext, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(_outBuffer.data()), _outSize, 0);
            NEXT(DeleteSecurityContext);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_DeleteSecurityContextState(Data &data) {
        data.session.DeleteContext();
        NEXT(EndClose);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_EndCloseState(Data &data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;
        return AcceptStateOpResult::Closed;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_AbortState(Data &data) {
        constexpr linger lingerOption{ 1, 0 };
        setsockopt(
            data.socket,
            SOL_SOCKET,
            SO_LINGER,
            reinterpret_cast<const char*>(&lingerOption),
            sizeof(lingerOption)
        );
        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;
        return AcceptStateOpResult::Closed;
    }
}

#pragma pop_macro("AWAIT")
#pragma pop_macro("NEXT")