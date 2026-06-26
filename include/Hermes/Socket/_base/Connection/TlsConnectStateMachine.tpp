#pragma once
#include <Hermes/_base/Network.hpp>

#pragma push_macro("MSG_NOSIGNAL")
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#pragma push_macro("NEXT")
#pragma push_macro("AWAIT")
#undef NEXT
#undef AWAIT

#define NEXT(state) do {                               \
    _state = &TlsConnectStateMachine::_##state##State; \
    return (this->*_state)(data);                      \
} while (false)

#define AWAIT(nextState, opResult) do {                    \
    _state = &TlsConnectStateMachine::_##nextState##State; \
    return ConnectStateOpResult::opResult;                 \
} while (false)

namespace Hermes::_details {
#pragma region Helpers

    template<SocketDataConcept Data, class ConnectionPolicy>
    TlsConnectStateMachine<Data, ConnectionPolicy>::TlsConnectStateMachine(typename ConnectionPolicy::Options opt)
        : _options{ std::move(opt) } {}

    template<SocketDataConcept Data, class ConnectionPolicy>
    typename TlsConnectStateMachine<Data, ConnectionPolicy>::TlsConnectState TlsConnectStateMachine<Data, ConnectionPolicy>::GetState() const noexcept {
        return _state;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectionResultOper TlsConnectStateMachine<Data, ConnectionPolicy>::GetResult() const noexcept {
        return _errorStatus;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    bool TlsConnectStateMachine<Data, ConnectionPolicy>::IsFinished() const noexcept {
        return _state == &TlsConnectStateMachine::_HandshakeCompletedState
            || _state == &TlsConnectStateMachine::_EndCloseState
            || _state == &TlsConnectStateMachine::_AbortState
            || !_errorStatus;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetToClose() noexcept {
        _errorStatus = {};
        _state       = &TlsConnectStateMachine::_StartCloseState;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetToAbort() noexcept {
        _errorStatus = {};
        _state       = &TlsConnectStateMachine::_AbortState;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetToOpen() noexcept {
        _errorStatus = {};
        _state       = &TlsConnectStateMachine::_SetupState;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
        TlsConnectStateMachine<Data, ConnectionPolicy>::Advance(Data &data) noexcept {
        return (this->*_state)(data);
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetIoResult(const int bytes) noexcept {
        _currSent = bytes;
        _currReceived = bytes;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    std::span<std::byte> TlsConnectStateMachine<Data, ConnectionPolicy>::GetRecvBuffer(Data &data) noexcept {
        return { data.state->decryptedData.data() + _receivedBytes, data.state->decryptedData.size() - _receivedBytes };
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    std::span<const std::byte> TlsConnectStateMachine<Data, ConnectionPolicy>::GetSendBuffer() noexcept {
        return { _outBuffer.data(), _outSize };
    }

#pragma endregion

#pragma region Setup

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_SetupState(Data &data) {

#pragma region fast-fail and initialization

        if (data.credentials == nullptr) data.credentials = &Network::GetClientCredentials();
        if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();

        if (data.host.size() >= 255) {
            _errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            return ConnectStateOpResult::Error;
        }

        data.session.BeginClient(*data.credentials, data.host, _options.ignoreCertificateErrors, _options.requestMutualAuth);

        _receivedBytes   = data.pendingData;
        data.pendingData = 0;

        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, _options.handshakeTimeout.count());
#pragma endregion

        NEXT(InitializeContext);
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_InitializeContextState(Data &data) {

#pragma region InitializateContext

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
                case EncryptStatusEnum::InfoContinueNeeded:             if (_outSize > 0) NEXT(Send);      NEXT(CheckSend);
                case EncryptStatusEnum::ErrIncompleteMessage:           NEXT(CheckSend);
                case EncryptStatusEnum::ErrOk:                          if (_outSize > 0) NEXT(FinalSend);
                NEXT(HandshakeCompleted);
                case EncryptStatusEnum::ErrIncompleteCredentials:       NEXT(IncompleteCredentialsError);
                case EncryptStatusEnum::ErrInsufficientMemory:
                case EncryptStatusEnum::ErrInvalidHandle:
                case EncryptStatusEnum::ErrInvalidToken:                NEXT(HandshakeFailedError);
                case EncryptStatusEnum::ErrLogonDenied:
                case EncryptStatusEnum::ErrNoCredentials:
                case EncryptStatusEnum::ErrUntrustedRoot:
                case EncryptStatusEnum::ErrNoAuthenticatingAuthority:   NEXT(InvalidCertificateError);
                default:                                                NEXT(UnknownError);
        }

    }

#pragma endregion

#pragma region Incomplete

#pragma region Send

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_SendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckSend, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(_outBuffer.data()), _outSize, MSG_NOSIGNAL);
            NEXT(CheckSend);
        }
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CheckSendState(Data &data) {
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
        NEXT(InitializeContext);
    }

#pragma endregion

#pragma region Recv

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_RecvState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckRecv, Recv);
        } else {
            _currReceived = recv(data.socket,
                reinterpret_cast<char*>(data.state->decryptedData.data() + _receivedBytes),
                data.state->decryptedData.size() - _receivedBytes, 0);
            NEXT(CheckRecv);
        }
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CheckRecvState(Data &data) {

        if (_currReceived <= 0)
            NEXT(HandshakeFailedError);
        _receivedBytes += _currReceived;

        NEXT(InitializeContext);

    }

#pragma endregion

#pragma endregion

#pragma region Complete

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_FinalSendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckFinalSend, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(_outBuffer.data()), _outSize, MSG_NOSIGNAL);
            NEXT(CheckFinalSend);
        }
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CheckFinalSendState(Data &data) {
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

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_HandshakeCompletedState(Data &data) {
        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, 0);

        std::size_t extraBufferSize{ static_cast<std::size_t>(_receivedBytes) };
        data.state->decryptedExtraSpan = std::span<std::byte>{
            data.state->decryptedData.data(),
            extraBufferSize
        };
        return ConnectStateOpResult::Done;
    }

#pragma endregion

#pragma region Errors

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_HandshakeFailedErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
        NEXT(Cleanup);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_IncompleteCredentialsErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
        NEXT(Cleanup);

    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_InvalidCertificateErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
        NEXT(Cleanup);

    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_UnknownErrorState(Data &data) {
        _errorStatus = std::unexpected{ ConnectionErrorEnum::Unknown };
        NEXT(Cleanup);

    }

#pragma endregion

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CleanupState(Data &data) {
        data.session.DeleteContext();

        NEXT(StartClose);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_StartCloseState(Data &data) {
        if (data.socket == macroINVALID_SOCKET) return ConnectStateOpResult::Error;

        if (!data.session.IsHandshakeComplete())
            NEXT(EndClose);

        _outSize = data.session.Shutdown(std::span<std::byte>{_outBuffer});
        if (_outSize > 0)
            NEXT(SendCloseNotify);

        NEXT(DeleteSecurityContext);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_SendCloseNotifyState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(DeleteSecurityContext, Send);
        } else {
            _currSent = send(data.socket, reinterpret_cast<const char*>(_outBuffer.data()), _outSize, MSG_NOSIGNAL);
            NEXT(DeleteSecurityContext);
        }
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_DeleteSecurityContextState(Data &data) {
        data.session.DeleteContext();
        NEXT(EndClose);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_EndCloseState(Data &data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;

        return ConnectStateOpResult::Closed;
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_AbortState(Data &data) {
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

        return ConnectStateOpResult::Closed;
    }
}

#pragma pop_macro("AWAIT")
#pragma pop_macro("NEXT")
#pragma pop_macro("MSG_NOSIGNAL")