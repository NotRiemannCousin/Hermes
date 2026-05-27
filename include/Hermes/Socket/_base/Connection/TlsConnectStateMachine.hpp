#pragma once
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/_base/Connection/ITlsConnectStateMachine.hpp>

namespace Hermes::_details {
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

        typename ConnectionPolicy::Options _options;
        TlsConnectState _state{ &TlsConnectStateMachine::_SetupState };


        ConnectionResultOper _errorStatus{};

        IoCount _currReceived{};
        IoCount _currSent{};

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

        InitializeSecurityContextFlags _dwSspiFlags{};
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
        ConnectStateOpResult _SetupState(Data &data);
        ConnectStateOpResult _InitializeContextState(Data &data);

        // Incomplete
        ConnectStateOpResult _CompleteAuthState(Data &data);
        ConnectStateOpResult _SendState(Data &data);
        ConnectStateOpResult _CheckSendState(Data &data);
        ConnectStateOpResult _RecvState(Data &data);
        ConnectStateOpResult _CheckRecvState(Data &data);

        // Complete
        ConnectStateOpResult _FinalSendState(Data &data);
        ConnectStateOpResult _CheckFinalSendState(Data &data);
        ConnectStateOpResult _HandshakeCompletedState(Data &data);

        // Errors
        ConnectStateOpResult _HandshakeFailedErrorState(Data &data);
        ConnectStateOpResult _IncompleteCredentialsErrorState(Data &data);
        ConnectStateOpResult _InvalidCertificateErrorState(Data &data);
        ConnectStateOpResult _UnknownErrorState(Data &data);

        // Close
        ConnectStateOpResult _CleanupState(Data &data);
        ConnectStateOpResult _StartCloseState(Data &data);
        ConnectStateOpResult _SendCloseNotifyState(Data &data);
        ConnectStateOpResult _DeleteSecurityContextState(Data &data);
        ConnectStateOpResult _EndCloseState(Data &data);

        // Abort
        ConnectStateOpResult _AbortState(Data &data);
    };
}

#include <Hermes/Socket/_base/Connection/TlsConnectStateMachine.tpp>