#pragma once
#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.hpp>
#include <Hermes/Socket/_base/Accept/ITlsAcceptStateMachine.hpp>

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

#pragma region buffers

        std::array<std::array<std::byte, 0x4000>, 4> _buffers{};
        std::array<SecBuffer, 4> _secBuffers{};

        SecBuffer& TokenBuffer() noexcept; // input:  data received from client
        SecBuffer& ExtraBuffer() noexcept; // input:  leftover from previous iteration

        SecBuffer& OutBuffer() noexcept; // output: token to send to client
        SecBuffer& MsgBuffer() noexcept; // output: alert

        SecBufferDesc _inBufferDesc{ .ulVersion = macroSECBUFFER_VERSION, .cBuffers = 2, .pBuffers = &TokenBuffer() };
        SecBufferDesc _outBufferDesc{ .ulVersion = macroSECBUFFER_VERSION, .cBuffers = 2, .pBuffers = &OutBuffer() };

#pragma endregion

#pragma region settings and lifecycle

        CredHandle _credHandle{};
        TimeStamp _tsExpiry{};

        AcceptSecurityContextFlags _dwSspiFlags{};
        DWORD _pfContextAttr{};
        SECURITY_STATUS _status{};

        bool _isRenegotiation{};
        bool _firstPass{};

        DWORD _receivedBytes{};

#pragma endregion

#pragma endregion

#pragma region Helpers

        static constexpr bool HasData(const SecBuffer& buffer) noexcept {
            return buffer.cbBuffer > 0;
        }

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