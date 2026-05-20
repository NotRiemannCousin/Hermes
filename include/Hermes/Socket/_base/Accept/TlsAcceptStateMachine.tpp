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
    TlsAcceptStateMachine<Data, AcceptPolicy>::TlsAcceptState TlsAcceptStateMachine<Data, AcceptPolicy>::GetState() const noexcept {
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
        return { static_cast<const std::byte*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer };
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    SecBuffer & TlsAcceptStateMachine<Data, AcceptPolicy>::TokenBuffer() noexcept { return _secBuffers[0]; }
    template<SocketDataConcept Data, class AcceptPolicy>
    SecBuffer & TlsAcceptStateMachine<Data, AcceptPolicy>::ExtraBuffer() noexcept { return _secBuffers[1]; }

    template<SocketDataConcept Data, class AcceptPolicy>
    SecBuffer & TlsAcceptStateMachine<Data, AcceptPolicy>::OutBuffer() noexcept { return _secBuffers[2]; }
    template<SocketDataConcept Data, class AcceptPolicy>
    SecBuffer & TlsAcceptStateMachine<Data, AcceptPolicy>::MsgBuffer() noexcept { return _secBuffers[3]; }

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

        _credHandle = data.credentials->GetCredHandle();
        _dwSspiFlags = AcceptSecurityContextFlags::SequenceDetect  |
                       AcceptSecurityContextFlags::ReplayDetect    |
                       AcceptSecurityContextFlags::Confidentiality |
                       AcceptSecurityContextFlags::ExtendedError   |
                       AcceptSecurityContextFlags::Stream;

        if (_options.requestClientCertificate)
            _dwSspiFlags |= AcceptSecurityContextFlags::MutualAuth;

        _isRenegotiation = (data.ctxtHandle.dwLower != 0 || data.ctxtHandle.dwUpper != 0);
        _firstPass       = !_isRenegotiation;

        _receivedBytes   = data.pendingData;
        data.pendingData = 0;

        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, _options.handshakeTimeout.count());

#pragma endregion

        // Servidor precisa de dados (ClientHello) para a primeira chamada
        if (_receivedBytes > 0)
            NEXT(AcceptContext);
        NEXT(Recv);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_AcceptContextState(Data &data) {

#pragma region AcceptContext

        TokenBuffer() = SecBuffer{ _receivedBytes, _tul(SecurityBufferEnum::Token), data.state->decryptedData.data() };
        ExtraBuffer() = SecBuffer{ 0             , _tul(SecurityBufferEnum::Empty), nullptr };

        OutBuffer() = SecBuffer{ _tul(_buffers[2].size()), _tul(SecurityBufferEnum::Token), _buffers[2].data() };
        MsgBuffer() = SecBuffer{ _tul(_buffers[3].size()), _tul(SecurityBufferEnum::Alert), _buffers[3].data() };

        _status = AcceptSecurityContext(
            &_credHandle,
            _firstPass ? nullptr : &data.ctxtHandle,
            &_inBufferDesc,
            _tul(_dwSspiFlags), 0,
            &data.ctxtHandle,
            &_outBufferDesc, &_pfContextAttr, &_tsExpiry
        );

        _firstPass = false;

        if (HasData(ExtraBuffer()) && ExtraBuffer().BufferType == SecurityBufferEnum::Extra) {
            std::memmove(data.state->decryptedData.data(),
                        data.state->decryptedData.data() + _receivedBytes - ExtraBuffer().cbBuffer,
                        ExtraBuffer().cbBuffer);
            _receivedBytes = ExtraBuffer().cbBuffer;
        } else {
            _receivedBytes = 0;
        }

#pragma endregion

        switch (static_cast<EncryptStatusEnum>(_status)) {
            case EncryptStatusEnum::InfoContinueNeeded:             if (HasData(OutBuffer())) NEXT(Send);      NEXT(CheckSend);
            case EncryptStatusEnum::ErrIncompleteMessage:           NEXT(CheckSend);
            case EncryptStatusEnum::ErrOk:                          if (HasData(OutBuffer())) NEXT(FinalSend); NEXT(HandshakeCompleted);

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
            _currSent = send(data.socket, static_cast<const char*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer, 0);
            NEXT(CheckSend);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_CheckSendState(Data &data) {
        if (_currSent == macroSOCKET_ERROR)
            NEXT(HandshakeFailedError);

        if (_currSent != OutBuffer().cbBuffer) {
            OutBuffer().pvBuffer = static_cast<char*>(OutBuffer().pvBuffer) + _currSent;
            OutBuffer().cbBuffer -= _currSent;
            NEXT(Send);
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
            _currSent = send(data.socket, static_cast<const char*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer, 0);
            NEXT(CheckFinalSend);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_CheckFinalSendState(Data &data) {
        if (_currSent == macroSOCKET_ERROR)
            NEXT(HandshakeFailedError);

        if (_currSent != OutBuffer().cbBuffer) {
            OutBuffer().pvBuffer = static_cast<char*>(OutBuffer().pvBuffer) + _currSent;
            OutBuffer().cbBuffer -= _currSent;
            NEXT(FinalSend);
        }

        NEXT(HandshakeCompleted);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_HandshakeCompletedState(Data &data) {
        _status = QueryContextAttributesA(&data.ctxtHandle,
                                        macroSECPKG_ATTR_STREAM_SIZES,
                                        &data.contextStreamSizes);

        if (_status != EncryptStatusEnum::ErrOk)
            NEXT(HandshakeFailedError);

        data.requestClientCertificate = _options.requestClientCertificate;
        data.isHandshakeComplete      = true;
        data.isServer                 = true;

        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, 0);

        std::size_t extraBufferSize{ _receivedBytes };
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
        DeleteSecurityContext(&data.ctxtHandle);
        data.ctxtHandle = {};
        NEXT(StartClose);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_StartCloseState(Data &data) {
        if (data.socket == macroINVALID_SOCKET) return AcceptStateOpResult::Error;

        if (!data.isHandshakeComplete)
            NEXT(EndClose);

        DWORD dwType{ macroSCHANNEL_SHUTDOWN };
        OutBuffer() = { sizeof(dwType), _tul(SecurityBufferEnum::Token), &dwType };

        SecBufferDesc outBufferDesc{ macroSECBUFFER_VERSION, 1, &OutBuffer() };

        SECURITY_STATUS status{ ApplyControlToken(&data.ctxtHandle, &outBufferDesc) };

        if (status != EncryptStatusEnum::ErrOk)
            NEXT(DeleteSecurityContext);

        OutBuffer()   = SecBuffer{ 0, _tul(SecurityBufferEnum::Token), nullptr };
        outBufferDesc = SecBufferDesc{ macroSECBUFFER_VERSION, 1, &OutBuffer() };

        auto dwSspiFlags{
            AcceptSecurityContextFlags::SequenceDetect  |
            AcceptSecurityContextFlags::ReplayDetect    |
            AcceptSecurityContextFlags::Confidentiality |
            AcceptSecurityContextFlags::Stream
        };

        if (data.requestClientCertificate)
            dwSspiFlags |= AcceptSecurityContextFlags::MutualAuth;

        DWORD dwSSPIOutFlags{};
        TimeStamp tsExpiry{};

        status = AcceptSecurityContext(
            &_credHandle,
            &data.ctxtHandle,
            nullptr,
            _tul(dwSspiFlags),
            0,
            nullptr,
            &outBufferDesc,
            &dwSSPIOutFlags,
            &tsExpiry
        );

        if (status == EncryptStatusEnum::ErrOk && HasData(OutBuffer()))
            NEXT(SendCloseNotify);

        NEXT(DeleteSecurityContext);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_SendCloseNotifyState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(DeleteSecurityContext, Send);
        } else {
            _currSent = send(data.socket, static_cast<const char*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer, 0);
            NEXT(DeleteSecurityContext);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_DeleteSecurityContextState(Data &data) {
        DeleteSecurityContext(&data.ctxtHandle);
        data.ctxtHandle = {};
        data.isHandshakeComplete = false;
        NEXT(EndClose);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::_EndCloseState(Data &data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        closesocket(data.socket);
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
        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;
        return AcceptStateOpResult::Closed;
    }
}

#pragma pop_macro("AWAIT")
#pragma pop_macro("NEXT")