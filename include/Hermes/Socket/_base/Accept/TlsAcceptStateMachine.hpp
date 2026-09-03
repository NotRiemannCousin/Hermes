#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_TLS

#include <Hermes/Socket/_base/Accept/ITlsAcceptStateMachine.hpp>
#include <Hermes/Socket/_base/TlsSession.hpp>

namespace Hermes::details_ {
    template<SocketDataConcept Data, class AcceptPolicy>
    struct TlsAcceptStateMachine : public ITlsAcceptStateMachine<Data> {

        using TlsAcceptState = AcceptStateOpResult(TlsAcceptStateMachine::*)(Data&);

#if HERMES_ENABLE_ASYNC
        static constexpr bool IsAsync() noexcept {
            return AsyncConnectionPolicyConcept<AcceptPolicy, Data>;
        }
#else
        static constexpr bool IsAsync() { return false; }
#endif

        explicit TlsAcceptStateMachine(typename AcceptPolicy::AcceptOptions opt);

        TlsAcceptStateMachine(const TlsAcceptStateMachine&) = delete;
        TlsAcceptStateMachine& operator=(const TlsAcceptStateMachine&) = delete;

        TlsAcceptStateMachine(TlsAcceptStateMachine&&) = delete;
        TlsAcceptStateMachine& operator=(TlsAcceptStateMachine&&) = delete;

        TlsAcceptState GetState() const noexcept;
        ConnectionResultOper GetResult() const noexcept override;
        bool IsFinished() const noexcept override;

        void SetToClose() noexcept override;
        void SetToAbort() noexcept override;
        void SetToOpen() noexcept override;

        AcceptStateOpResult Advance(Data &data) noexcept override;

        void SetIoResult(int bytes) noexcept override;

        std::span<std::byte> GetRecvBuffer(Data& data) noexcept;
        std::span<const std::byte> GetSendBuffer() noexcept;

    private:

#pragma region variables

        typename AcceptPolicy::AcceptOptions m_options;
        TlsAcceptState m_state{ &TlsAcceptStateMachine::SetupState };

        ConnectionResultOper m_errorStatus{};

        int m_currReceived{};
        int m_currSent{};
        std::uint32_t m_receivedBytes{};

#pragma region buffers e configuracoes

        std::array<std::byte, 0x4000> m_outBuffer{};
        std::uint32_t m_outSize{};
        EncryptStatusEnum m_status{};

#pragma endregion

#pragma endregion

        // Setup
        AcceptStateOpResult SetupState(Data &data);
        AcceptStateOpResult AcceptContextState(Data &data);

        // Incomplete
        AcceptStateOpResult SendState(Data &data);
        AcceptStateOpResult CheckSendState(Data &data);
        AcceptStateOpResult RecvState(Data &data);
        AcceptStateOpResult CheckRecvState(Data &data);

        // Complete
        AcceptStateOpResult FinalSendState(Data &data);
        AcceptStateOpResult CheckFinalSendState(Data &data);
        AcceptStateOpResult HandshakeCompletedState(Data &data);

        // Errors
        AcceptStateOpResult HandshakeFailedErrorState(Data &data);
        AcceptStateOpResult InvalidCertificateErrorState(Data &data);
        AcceptStateOpResult UnknownErrorState(Data &data);

        // Close
        AcceptStateOpResult CleanupState(Data &data);
        AcceptStateOpResult StartCloseState(Data &data);
        AcceptStateOpResult SendCloseNotifyState(Data &data);
        AcceptStateOpResult DeleteSecurityContextState(Data &data);
        AcceptStateOpResult EndCloseState(Data &data);

        // Abort
        AcceptStateOpResult AbortState(Data &data);
    };
}

#include <Hermes/Socket/_base/Accept/TlsAcceptStateMachine.tpp>

#endif