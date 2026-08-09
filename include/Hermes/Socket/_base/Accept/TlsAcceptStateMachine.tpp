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
    m_state = &TlsAcceptStateMachine::state##State;  \
    return (this->*m_state)(data);                      \
} while (false)

#define AWAIT(nextState, opResult) do {                    \
    m_state = &TlsAcceptStateMachine::nextState##State;  \
    return AcceptStateOpResult::opResult;                  \
} while (false)

namespace Hermes::details_ {
#pragma region Helpers

    template<SocketDataConcept Data, class AcceptPolicy>
    TlsAcceptStateMachine<Data, AcceptPolicy>::TlsAcceptStateMachine(typename AcceptPolicy::AcceptOptions opt)
        : m_options{ std::move(opt) } {}

    template<SocketDataConcept Data, class AcceptPolicy>
    typename TlsAcceptStateMachine<Data, AcceptPolicy>::TlsAcceptState TlsAcceptStateMachine<Data, AcceptPolicy>::GetState() const noexcept {
        return m_state;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    ConnectionResultOper TlsAcceptStateMachine<Data, AcceptPolicy>::GetResult() const noexcept {
        return m_errorStatus;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    bool TlsAcceptStateMachine<Data, AcceptPolicy>::IsFinished() const noexcept {
        return m_state == &TlsAcceptStateMachine::HandshakeCompletedState
            || m_state == &TlsAcceptStateMachine::EndCloseState
            || m_state == &TlsAcceptStateMachine::AbortState
            || !m_errorStatus;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetToClose() noexcept {
        m_errorStatus = {};
        m_state       = &TlsAcceptStateMachine::StartCloseState;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetToAbort() noexcept {
        m_errorStatus = {};
        m_state       = &TlsAcceptStateMachine::AbortState;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetToOpen() noexcept {
        m_errorStatus = {};
        m_state       = &TlsAcceptStateMachine::SetupState;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
        TlsAcceptStateMachine<Data, AcceptPolicy>::Advance(Data &data) noexcept {
        return (this->*m_state)(data);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    void TlsAcceptStateMachine<Data, AcceptPolicy>::SetIoResult(const int bytes) noexcept {
        m_currSent = bytes;
        m_currReceived = bytes;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    std::span<std::byte> TlsAcceptStateMachine<Data, AcceptPolicy>::GetRecvBuffer(Data &data) noexcept {
        return { data.state->decryptedData.data() + m_receivedBytes, data.state->decryptedData.size() - m_receivedBytes };
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    std::span<const std::byte> TlsAcceptStateMachine<Data, AcceptPolicy>::GetSendBuffer() noexcept {
        return { m_outBuffer.data(), m_outSize };
    }

#pragma endregion

#pragma region Setup

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::SetupState(Data &data) {

#pragma region fast-fail and initialization

        if (data.credentials == nullptr) {
            m_errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
            return AcceptStateOpResult::Error;
        }
        if (data.state == nullptr) data.state = std::make_unique<typename decltype(data.state)::element_type>();

        data.session.BeginServer(*data.credentials, m_options.requestClientCertificate);

        m_receivedBytes   = data.pendingData;
        data.pendingData = 0;
        if constexpr (!IsAsync())
            if (m_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, m_options.handshakeTimeout.count());
#pragma endregion

        if (m_receivedBytes > 0)
            NEXT(AcceptContext);
        NEXT(Recv);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::AcceptContextState(Data &data) {

#pragma region AcceptContext

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
            case EncryptStatusEnum::InfoContinueNeeded:
                if (m_outSize > 0) NEXT(Send);
                NEXT(CheckSend);
            case EncryptStatusEnum::ErrIncompleteMessage:           NEXT(CheckSend);
            case EncryptStatusEnum::ErrOk:
                if (m_outSize > 0) NEXT(FinalSend);
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
    TlsAcceptStateMachine<Data, AcceptPolicy>::SendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckSend, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(m_outBuffer.data()), m_outSize, MSG_NOSIGNAL);
            NEXT(CheckSend);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::CheckSendState(Data &data) {
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
        NEXT(AcceptContext);
    }

#pragma endregion

#pragma region Recv

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::RecvState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckRecv, Recv);
        } else {
            m_currReceived = recv(data.socket,
                reinterpret_cast<char*>(data.state->decryptedData.data() + m_receivedBytes),
                data.state->decryptedData.size() - m_receivedBytes, 0);
            NEXT(CheckRecv);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::CheckRecvState(Data &data) {
        if (m_currReceived <= 0)
            NEXT(HandshakeFailedError);
        m_receivedBytes += m_currReceived;
        NEXT(AcceptContext);
    }

#pragma endregion

#pragma endregion

#pragma region Complete

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::FinalSendState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(CheckFinalSend, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(m_outBuffer.data()), m_outSize, MSG_NOSIGNAL);
            NEXT(CheckFinalSend);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::CheckFinalSendState(Data &data) {
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

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::HandshakeCompletedState(Data &data) {
        m_status = EncryptStatusEnum::ErrOk;

        if constexpr (!IsAsync())
            if (m_options.handshakeTimeout.count() != 0)
                SetTimeout(data.socket, 0);

        std::size_t extraBufferSize{ static_cast<std::size_t>(m_receivedBytes) };
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
    TlsAcceptStateMachine<Data, AcceptPolicy>::HandshakeFailedErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::HandshakeFailed };
        NEXT(Cleanup);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::InvalidCertificateErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::CertificateError };
        NEXT(Cleanup);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::UnknownErrorState(Data &data) {
        m_errorStatus = std::unexpected{ ConnectionErrorEnum::Unknown };
        NEXT(Cleanup);
    }

#pragma endregion

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::CleanupState(Data &data) {
        data.session.DeleteContext();
        NEXT(StartClose);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::StartCloseState(Data &data) {
        if (data.socket == macroINVALID_SOCKET) return AcceptStateOpResult::Error;

        if (!data.session.IsHandshakeComplete())
            NEXT(EndClose);

        auto produced{ data.session.Shutdown(std::span<std::byte>{ m_outBuffer }) };
        if (produced > 0) {
            m_outSize = produced;
            NEXT(SendCloseNotify);
        }

        NEXT(DeleteSecurityContext);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::SendCloseNotifyState(Data &data) {
        if constexpr (IsAsync()) {
            AWAIT(DeleteSecurityContext, Send);
        } else {
            m_currSent = send(data.socket, reinterpret_cast<const char*>(m_outBuffer.data()), m_outSize, MSG_NOSIGNAL);
            NEXT(DeleteSecurityContext);
        }
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::DeleteSecurityContextState(Data &data) {
        data.session.DeleteContext();
        NEXT(EndClose);
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::EndCloseState(Data &data) {
        shutdown(data.socket, static_cast<int>(SocketShutdownEnum::Send));
        CloseSocket(data.socket);
        data.socket = macroINVALID_SOCKET;
        return AcceptStateOpResult::Closed;
    }

    template<SocketDataConcept Data, class AcceptPolicy>
    AcceptStateOpResult
    TlsAcceptStateMachine<Data, AcceptPolicy>::AbortState(Data &data) {
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
#pragma pop_macro("MSG_NOSIGNAL")