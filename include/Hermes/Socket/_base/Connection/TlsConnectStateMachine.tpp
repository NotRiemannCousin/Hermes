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
    m_state = &TlsConnectStateMachine::state##State; \
    return (this->*m_state)(data);                      \
} while (false)

#define AWAIT(nextState, opResult) do {                    \
    m_state = &TlsConnectStateMachine::nextState##State; \
    return ConnectStateOpResult::opResult;                 \
} while (false)

namespace Hermes::details_ {
#pragma region Helpers

    template<SocketDataConcept Data, class ConnectionPolicy>
    TlsConnectStateMachine<Data, ConnectionPolicy>::TlsConnectStateMachine(typename ConnectionPolicy::Options opt)
        : m_options{ std::move(opt) } {}

    template<SocketDataConcept Data, class ConnectionPolicy>
    typename TlsConnectStateMachine<Data, ConnectionPolicy>::TlsConnectState TlsConnectStateMachine<Data, ConnectionPolicy>::GetState() const noexcept {
        return m_state;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectionResultOper TlsConnectStateMachine<Data, ConnectionPolicy>::GetResult() const noexcept {
        return m_errorStatus;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    bool TlsConnectStateMachine<Data, ConnectionPolicy>::IsFinished() const noexcept {
        return m_state == &TlsConnectStateMachine::HandshakeCompletedState
            || m_state == &TlsConnectStateMachine::EndCloseState
            || m_state == &TlsConnectStateMachine::AbortState
            || !m_errorStatus;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetToClose() noexcept {
        m_errorStatus = {};
        m_state       = &TlsConnectStateMachine::StartCloseState;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetToAbort() noexcept {
        m_errorStatus = {};
        m_state       = &TlsConnectStateMachine::AbortState;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetToOpen() noexcept {
        m_errorStatus = {};
        m_state       = &TlsConnectStateMachine::SetupState;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
        TlsConnectStateMachine<Data, ConnectionPolicy>::Advance(Data &data) noexcept {
        return (this->*m_state)(data);
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    void TlsConnectStateMachine<Data, ConnectionPolicy>::SetIoResult(const int bytes) noexcept {
        m_currSent = bytes;
        m_currReceived = bytes;
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    std::span<std::byte> TlsConnectStateMachine<Data, ConnectionPolicy>::GetRecvBuffer(Data &data) noexcept {
        return { data.state->decryptedData.data() + m_receivedBytes, data.state->decryptedData.size() - m_receivedBytes };
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    std::span<const std::byte> TlsConnectStateMachine<Data, ConnectionPolicy>::GetSendBuffer() noexcept {
        return { m_outBuffer.data(), m_outSize };
    }

#pragma endregion

#pragma region Setup

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::SetupState(Data &data) {

#pragma region fast-fail and initialization

        if (data.credentials == nullptr) data.credentials = &Network::GetClientCredentials();
        if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();

        if (data.host.size() >= 255) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
            return ConnectStateOpResult::Error;
        }

        data.session.BeginClient(*data.credentials, data.host, m_options.ignoreCertificateErrors, m_options.requestMutualAuth);

        m_receivedBytes   = data.pendingData;
        data.pendingData = 0;

        if constexpr (!IsAsync())
            if (m_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, m_options.handshakeTimeout.count());
#pragma endregion

        NEXT(InitializeContext);
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::InitializeContextState(Data &data) {

#pragma region InitializateContext

        auto outcome{ data.session.AdvanceHandshake(
            std::span<std::byte>{data.state->decryptedData.data(), m_receivedBytes},
            std::span<std::byte>{m_outBuffer}
        ) };
        m_status = outcome.status;
        m_outSize = outcome.produced;

        if (outcome.consumed > 0 && outcome.consumed <= m_receivedBytes) {
            std::memmove(data.state->decryptedData.data(),
                         data.state->decryptedData.data() + outcome.consumed,
                         m_receivedBytes - outcome.consumed);
            m_receivedBytes -= outcome.consumed;
        }

#pragma endregion

        switch (m_status) {
                case EncryptStatusEnum::InfoContinueNeeded:             if (m_outSize > 0) NEXT(Send);      NEXT(CheckSend);
                case EncryptStatusEnum::ErrIncompleteMessage:           NEXT(CheckSend);
                case EncryptStatusEnum::ErrOk:                          if (m_outSize > 0) NEXT(FinalSend);
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
    TlsConnectStateMachine<Data, ConnectionPolicy>::SendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckSend, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(m_outBuffer.data()), m_outSize, MSG_NOSIGNAL);
            NEXT(CheckSend);
        }
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::CheckSendState(Data &data) {
        if (m_outSize > 0) {
            if (m_currSent == macroSOCKET_ERROR)
                NEXT(HandshakeFailedError);

            if (static_cast<std::uint32_t>(m_currSent) != m_outSize) {
                std::memmove(m_outBuffer.data(), m_outBuffer.data() + m_currSent, m_outSize - m_currSent);
                m_outSize -= m_currSent;
                NEXT(Send);
            }
            m_outSize = 0;
        }

        if (m_status == EncryptStatusEnum::ErrIncompleteMessage || m_receivedBytes == 0)
            NEXT(Recv);
        NEXT(InitializeContext);
    }

#pragma endregion

#pragma region Recv

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::RecvState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckRecv, Recv);
        } else {
            m_currReceived = recv(data.socket,
                reinterpret_cast<char*>(data.state->decryptedData.data() + m_receivedBytes),
                data.state->decryptedData.size() - m_receivedBytes, 0);
            NEXT(CheckRecv);
        }
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::CheckRecvState(Data &data) {

        if (m_currReceived <= 0)
            NEXT(HandshakeFailedError);
        m_receivedBytes += m_currReceived;

        NEXT(InitializeContext);

    }

#pragma endregion

#pragma endregion

#pragma region Complete

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::FinalSendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckFinalSend, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(m_outBuffer.data()), m_outSize, MSG_NOSIGNAL);
            NEXT(CheckFinalSend);
        }
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::CheckFinalSendState(Data &data) {
        if (m_outSize > 0) {
            if (m_currSent == macroSOCKET_ERROR)
                NEXT(HandshakeFailedError);

            if (static_cast<std::uint32_t>(m_currSent) != m_outSize) {
                std::memmove(m_outBuffer.data(), m_outBuffer.data() + m_currSent, m_outSize - m_currSent);
                m_outSize -= m_currSent;
                NEXT(FinalSend);
            }
            m_outSize = 0;
        }

        NEXT(HandshakeCompleted);
    }

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::HandshakeCompletedState(Data &data) {
        if constexpr (!IsAsync())
            if (m_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, 0);

        std::size_t extraBufferSize{ static_cast<std::size_t>(m_receivedBytes) };
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
    TlsConnectStateMachine<Data, ConnectionPolicy>::HandshakeFailedErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
        NEXT(Cleanup);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::IncompleteCredentialsErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
        NEXT(Cleanup);

    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::InvalidCertificateErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
        NEXT(Cleanup);

    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::UnknownErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::Unknown };
        NEXT(Cleanup);

    }

#pragma endregion

    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::CleanupState(Data &data) {
        data.session.DeleteContext();

        NEXT(StartClose);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::StartCloseState(Data &data) {
        if (data.socket == macroINVALID_SOCKET) return ConnectStateOpResult::Error;

        if (!data.session.IsHandshakeComplete())
            NEXT(EndClose);

        m_outSize = data.session.Shutdown(std::span<std::byte>{m_outBuffer});
        if (m_outSize > 0)
            NEXT(SendCloseNotify);

        NEXT(DeleteSecurityContext);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::SendCloseNotifyState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(DeleteSecurityContext, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(m_outBuffer.data()), m_outSize, MSG_NOSIGNAL);
            NEXT(DeleteSecurityContext);
        }
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::DeleteSecurityContextState(Data &data) {
        data.session.DeleteContext();
        NEXT(EndClose);
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::EndCloseState(Data &data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;

        return ConnectStateOpResult::Closed;
    }
    template<SocketDataConcept Data, class ConnectionPolicy>
    ConnectStateOpResult
    TlsConnectStateMachine<Data, ConnectionPolicy>::AbortState(Data &data) {
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