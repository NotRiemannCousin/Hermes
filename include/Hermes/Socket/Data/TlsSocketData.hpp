#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <Hermes/_base/Credentials.hpp>
#include <memory>

namespace Hermes::_details {
    template<typename Data>
    struct ITlsConnectStateMachine;
    template<typename Data>
    struct ITlsTransferStateMachine;
    template<typename Data>
    struct ITlsAcceptStateMachine;
}

namespace Hermes {
    template<
            EndpointConcept Endpoint       = IpEndpoint,
            SocketTypeEnum SocketType      = SocketTypeEnum::Stream,
            AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct TlsSocketData {
        using DataType = TlsSocketData<Endpoint, SocketType, SocketFamily>;
        using EndpointType = Endpoint;

        static constexpr SocketTypeEnum Type = SocketType;
        static constexpr AddressFamilyEnum Family = SocketFamily;

        TlsSocketData() = default;
        TlsSocketData(Endpoint endpoint, std::string host, const Credentials* credentials = nullptr);
        ~TlsSocketData();

        TlsSocketData(TlsSocketData&& other) noexcept;
        TlsSocketData& operator=(TlsSocketData&& other) noexcept;

        TlsSocketData MakeChild() const;

        std::string host{};

        const Credentials* credentials{};

        std::unique_ptr<_details::ITlsConnectStateMachine<DataType>> connectStateMachine;
        std::unique_ptr<_details::ITlsTransferStateMachine<DataType>> transferStateMachine;
        std::unique_ptr<_details::ITlsAcceptStateMachine<DataType>> acceptStateMachine;

        uint32_t pendingData{};

        // private:
        CtxtHandle ctxtHandle{};

        bool requestClientCertificate{};


        Endpoint endpoint{};
        SocketFd   socket{ macroINVALID_SOCKET };

        bool isHandshakeComplete{};
        bool isServer{};

        struct State {
            std::array<std::byte, 0x4800>  encryptedData{};
            std::array<std::byte, 0x10000> decryptedData{};

            std::span<std::byte> decryptedDataSpan{};
            std::span<std::byte> decryptedExtraSpan{};
        };

        std::unique_ptr<State> state{};

        SecPkgContext_StreamSizes contextStreamSizes{};
        size_t decryptedOffset{};

        static_assert(SocketType == SocketTypeEnum::Stream && "DTLS not implemented yet");
    };

    static_assert(SocketDataConcept<TlsSocketData<>>);
}

#include <Hermes/Socket/Data/TlsSocketData.tpp>