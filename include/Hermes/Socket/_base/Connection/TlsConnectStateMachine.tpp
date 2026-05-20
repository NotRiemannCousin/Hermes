#pragma once
#include <Hermes/_base/Network.hpp>

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
    TlsConnectStateMachine<Data, ConnectionPolicy>::TlsConnectState TlsConnectStateMachine<Data, ConnectionPolicy>::GetState() const noexcept {
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
        return { static_cast<const std::byte*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer };
    }


    template<SocketDataConcept Data, class ConnectionPolicy>
    SecBuffer & TlsConnectStateMachine<Data, ConnectionPolicy>::TokenBuffer() noexcept { return _secBuffers[0]; }
    template<SocketDataConcept Data, class ConnectionPolicy>
    SecBuffer & TlsConnectStateMachine<Data, ConnectionPolicy>::ExtraBuffer() noexcept { return _secBuffers[1]; }

    template<SocketDataConcept Data, class ConnectionPolicy>
    SecBuffer & TlsConnectStateMachine<Data, ConnectionPolicy>::OutBuffer() noexcept { return _secBuffers[2]; }
    template<SocketDataConcept Data, class ConnectionPolicy>
    SecBuffer & TlsConnectStateMachine<Data, ConnectionPolicy>::MsgBuffer() noexcept { return _secBuffers[3]; }

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

        _credHandle = data.credentials->GetCredHandle();
        _dwSspiFlags = InitializeSecurityContextFlags::SequenceDetect  |
                       InitializeSecurityContextFlags::ReplayDetect    |
                       InitializeSecurityContextFlags::Confidentiality |
                       InitializeSecurityContextFlags::ExtendedError   |
                       InitializeSecurityContextFlags::Stream;

        if (_options.requestMutualAuth)
            _dwSspiFlags |= InitializeSecurityContextFlags::MutualAuth;

        if (_options.ignoreCertificateErrors)
            _dwSspiFlags |= InitializeSecurityContextFlags::ManualCredValidation;

        _isRenegotiation = (data.ctxtHandle.dwLower != 0 || data.ctxtHandle.dwUpper != 0);
        _firstPass       = !_isRenegotiation;
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

        TokenBuffer() = SecBuffer{ _receivedBytes, _tul(SecurityBufferEnum::Token), data.state->decryptedData.data() };
        ExtraBuffer() = SecBuffer{ 0             , _tul(SecurityBufferEnum::Empty), nullptr };

        OutBuffer() = SecBuffer{ _tul(_buffers[2].size()), _tul(SecurityBufferEnum::Token), _buffers[2].data() };
        MsgBuffer() = SecBuffer{ _tul(_buffers[3].size()), _tul(SecurityBufferEnum::Alert), _buffers[3].data() };

        _status = InitializeSecurityContextA(
            &_credHandle,
            _firstPass ? nullptr : &data.ctxtHandle,
            const_cast<SEC_CHAR*>(data.host.c_str()),
            _tll(_dwSspiFlags), 0, 0,
            _firstPass ? nullptr : &_inBufferDesc, 0,
            &data.ctxtHandle,
            &_outBufferDesc, &_pfContextAttr, &_tsExpiry
        );

        _firstPass = false;

        if (HasData(ExtraBuffer()) && ExtraBuffer().BufferType == SecurityBufferEnum::Extra) {
            std::memmove(data.state->decryptedData.data(),
                        data.state->decryptedData.data() + _receivedBytes - ExtraBuffer().cbBuffer,
                        ExtraBuffer().cbBuffer); // reset buffer
            _receivedBytes = ExtraBuffer().cbBuffer;
        }

#pragma endregion

        switch (static_cast<EncryptStatusEnum>(_status)) {
                case EncryptStatusEnum::InfoCompleteAndContinue:
                case EncryptStatusEnum::InfoCompleteNeeded:             NEXT(CompleteAuth);
                case EncryptStatusEnum::InfoContinueNeeded:             if (HasData(OutBuffer())) NEXT(Send);      NEXT(CheckSend);
                case EncryptStatusEnum::ErrIncompleteMessage:           NEXT(CheckSend);
                case EncryptStatusEnum::ErrOk:                          if (HasData(OutBuffer())) NEXT(FinalSend); NEXT(HandshakeCompleted);
                case EncryptStatusEnum::ErrIncompleteCredentials:       NEXT(InvalidCertificateError);
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


#pragma region Complete Auth

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CompleteAuthState(Data &data) {
#pragma region Complete Auth

        CompleteAuthToken(&data.ctxtHandle, &_outBufferDesc);

#pragma endregion

        if (_status == EncryptStatusEnum::InfoCompleteAndContinue)
            NEXT(InitializeContext);
        NEXT(Send);
    }

#pragma endregion

#pragma region Send

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_SendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckSend, Send);
        } else {
            _currSent = send(data.socket, static_cast<const char*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer, 0);
            NEXT(CheckSend);
        }
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CheckSendState(Data &data) {
        if (_currSent == macroSOCKET_ERROR)
            NEXT(HandshakeFailedError);

        if (_currSent != OutBuffer().cbBuffer) {
            OutBuffer().pvBuffer = static_cast<char*>(OutBuffer().pvBuffer) + _currSent;
            OutBuffer().cbBuffer -= _currSent;
            NEXT(Send);
        }

        if (ExtraBuffer().BufferType != SecurityBufferEnum::Extra)
            _receivedBytes = 0;

        if (_receivedBytes > 0)
            NEXT(InitializeContext);
        NEXT(Recv);
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
            _currSent = send(data.socket, static_cast<const char*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer, 0);
            NEXT(CheckFinalSend);
        }
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_CheckFinalSendState(Data &data) {
        if (_currSent == macroSOCKET_ERROR)
            NEXT(HandshakeFailedError);

        if (_currSent != OutBuffer().cbBuffer) {
            OutBuffer().pvBuffer = static_cast<char*>(OutBuffer().pvBuffer) + _currSent;
            OutBuffer().cbBuffer -= _currSent;
            NEXT(FinalSend);
        }

        NEXT(HandshakeCompleted);
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_HandshakeCompletedState(Data &data) {
        _status = QueryContextAttributesA(&data.ctxtHandle,
                                        macroSECPKG_ATTR_STREAM_SIZES,
                                        &data.contextStreamSizes);

        if (_status != EncryptStatusEnum::ErrOk)
            NEXT(HandshakeFailedError);

        data.isHandshakeComplete = true;
        data.isServer            = ConnectionPolicy::IsServer; // TODO: Change when updating the template param

        if constexpr (!IsAsync())
            if (_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, 0);

        std::size_t extraBufferSize{
            HasData(ExtraBuffer()) && ExtraBuffer().BufferType == SecurityBufferEnum::Extra
                ? ExtraBuffer().cbBuffer
                : 0
        };

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
        DeleteSecurityContext(&data.ctxtHandle);
        data.ctxtHandle = {};

        NEXT(StartClose);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_StartCloseState(Data &data) {
        if (data.socket == macroINVALID_SOCKET) return ConnectStateOpResult::Error;

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

        constexpr auto dwSspiFlags{
            InitializeSecurityContextFlags::SequenceDetect  |
            InitializeSecurityContextFlags::ReplayDetect    |
            InitializeSecurityContextFlags::Confidentiality |
            InitializeSecurityContextFlags::Stream
        };

        DWORD dwSSPIOutFlags{};
        TimeStamp tsExpiry{};

        status = InitializeSecurityContextA(
            nullptr,
            &data.ctxtHandle,
            nullptr,
            _tl(dwSspiFlags),
            0,
            0,
            nullptr,
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
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_SendCloseNotifyState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(DeleteSecurityContext, Send);
        } else {
            _currSent = send(data.socket, static_cast<const char*>(OutBuffer().pvBuffer), OutBuffer().cbBuffer, 0);
            NEXT(DeleteSecurityContext);
        }
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_DeleteSecurityContextState(Data &data) {
        DeleteSecurityContext(&data.ctxtHandle);
        data.ctxtHandle = {};
        data.isHandshakeComplete = false;
        NEXT(EndClose);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::_EndCloseState(Data &data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        closesocket(data.socket);
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

        closesocket(data.socket);
        data.socket = macroINVALID_SOCKET;

        return ConnectStateOpResult::Closed;
    }
}

#pragma pop_macro("AWAIT")
#pragma pop_macro("NEXT")