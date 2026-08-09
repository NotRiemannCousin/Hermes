#pragma once
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base/Connection/ITlsConnectStateMachine.hpp>
#include <Hermes/Socket/_base/TlsSession.hpp>

namespace Hermes::details_ {
    template<SocketDataConcept Data, class ConnectionPolicy>
    struct TlsConnectStateMachine : public ITlsConnectStateMachine<Data> {
        using TlsConnectState = ConnectStateOpResult(TlsConnectStateMachine::*)(Data&);

        static constexpr bool IsServer{ ConnectionPolicy::IsServer };
        static constexpr bool IsAsync() noexcept {
            return AsyncConnectionPolicyConcept<ConnectionPolicy, Data>;
        }

        explicit TlsConnectStateMachine(typename ConnectionPolicy::Options opt);


        TlsConnectStateMachine(const TlsConnectStateMachine&) = delete;
        TlsConnectStateMachine& operator=(const TlsConnectStateMachine&) = delete;

        TlsConnectStateMachine(TlsConnectStateMachine&&) = delete;
        TlsConnectStateMachine& operator=(TlsConnectStateMachine&&) = delete;

        TlsConnectState GetState() const noexcept;
        ConnectionResultOper GetResult() const noexcept override;
        bool IsFinished() const noexcept override;

        void SetToClose() noexcept override;
        void SetToAbort() noexcept override;
        void SetToOpen() noexcept override;

        ConnectStateOpResult Advance(Data &data) noexcept override;

        void SetIoResult(int bytes) noexcept override;

        std::span<std::byte> GetRecvBuffer(Data& data) noexcept;
        std::span<const std::byte> GetSendBuffer() noexcept;

    private:

#pragma region variables

        typename ConnectionPolicy::Options m_options;
        TlsConnectState m_state{ &TlsConnectStateMachine::SetupState };

        ConnectionResultOper m_errorStatus{};

        int m_currReceived{};
        int m_currSent{};

#pragma region buffers

        std::array<std::byte, 0x4000> m_outBuffer{};
        std::uint32_t m_outSize{};

#pragma endregion

#pragma region settings and lifecycle

        EncryptStatusEnum m_status{};
        std::uint32_t m_receivedBytes{};

#pragma endregion

#pragma endregion

        // Setup
        ConnectStateOpResult SetupState(Data &data);
        ConnectStateOpResult InitializeContextState(Data &data);

        // Incomplete
        ConnectStateOpResult SendState(Data &data);
        ConnectStateOpResult CheckSendState(Data &data);
        ConnectStateOpResult RecvState(Data &data);
        ConnectStateOpResult CheckRecvState(Data &data);

        // Complete
        ConnectStateOpResult FinalSendState(Data &data);
        ConnectStateOpResult CheckFinalSendState(Data &data);
        ConnectStateOpResult HandshakeCompletedState(Data &data);

        // Errors
        ConnectStateOpResult HandshakeFailedErrorState(Data &data);
        ConnectStateOpResult IncompleteCredentialsErrorState(Data &data);
        ConnectStateOpResult InvalidCertificateErrorState(Data &data);
        ConnectStateOpResult UnknownErrorState(Data &data);

        // Close
        ConnectStateOpResult CleanupState(Data &data);
        ConnectStateOpResult StartCloseState(Data &data);
        ConnectStateOpResult SendCloseNotifyState(Data &data);
        ConnectStateOpResult DeleteSecurityContextState(Data &data);
        ConnectStateOpResult EndCloseState(Data &data);

        // Abort
        ConnectStateOpResult AbortState(Data &data);
    };
}

#include <Hermes/Socket/_base/Connection/TlsConnectStateMachine.tpp>