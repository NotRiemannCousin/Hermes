#pragma once
#include <Hermes/Socket/_base/Accept/ITlsAcceptStateMachine.hpp>
#include <Hermes/Socket/_base/TlsSession.hpp>

namespace Hermes::_details {
    template<SocketDataConcept Data, class AcceptPolicy>
    struct TlsAcceptStateMachine : public ITlsAcceptStateMachine<Data> {

        using TlsAcceptState = AcceptStateOpResult(TlsAcceptStateMachine::*)(Data&);

        static constexpr bool IsAsync() noexcept {
            return AsyncConnectionPolicyConcept<AcceptPolicy, Data>;
        }

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

        typename AcceptPolicy::AcceptOptions _options;
        TlsAcceptState _state{ &TlsAcceptStateMachine::_SetupState };

        ConnectionResultOper _errorStatus{};

        int _currReceived{};
        int _currSent{};
        std::uint32_t _receivedBytes{};

#pragma region buffers e configuracoes

        std::array<std::byte, 0x4000> _outBuffer{};
        std::uint32_t _outSize{};
        EncryptStatusEnum _status{};

#pragma endregion

#pragma endregion

        // Setup
        AcceptStateOpResult _SetupState(Data &data);
        AcceptStateOpResult _AcceptContextState(Data &data);

        // Incomplete
        AcceptStateOpResult _SendState(Data &data);
        AcceptStateOpResult _CheckSendState(Data &data);
        AcceptStateOpResult _RecvState(Data &data);
        AcceptStateOpResult _CheckRecvState(Data &data);

        // Complete
        AcceptStateOpResult _FinalSendState(Data &data);
        AcceptStateOpResult _CheckFinalSendState(Data &data);
        AcceptStateOpResult _HandshakeCompletedState(Data &data);

        // Errors
        AcceptStateOpResult _HandshakeFailedErrorState(Data &data);
        AcceptStateOpResult _InvalidCertificateErrorState(Data &data);
        AcceptStateOpResult _UnknownErrorState(Data &data);

        // Close
        AcceptStateOpResult _CleanupState(Data &data);
        AcceptStateOpResult _StartCloseState(Data &data);
        AcceptStateOpResult _SendCloseNotifyState(Data &data);
        AcceptStateOpResult _DeleteSecurityContextState(Data &data);
        AcceptStateOpResult _EndCloseState(Data &data);

        // Abort
        AcceptStateOpResult _AbortState(Data &data);
    };
}

#include <Hermes/Socket/_base/Accept/TlsAcceptStateMachine.tpp>