#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/Socket/_base/_base.hpp>
#include <Hermes/_base/WinApi.hpp>

namespace Hermes {
    template<
            EndpointConcept Endpoint       = IpEndpoint,
            SocketTypeEnum SocketType      = SocketTypeEnum::STREAM,
            AddressFamilyEnum SocketFamily = AddressFamilyEnum::INET6>
    struct TlsSocketData {
        using EndpointType = Endpoint;

        static constexpr SocketTypeEnum Type = SocketType;
        static constexpr AddressFamilyEnum Family = SocketFamily;

        TlsSocketData() = default;
        explicit TlsSocketData(std::string host);
        ~TlsSocketData() = default;

        TlsSocketData(TlsSocketData&& other) noexcept;
        TlsSocketData& operator=(TlsSocketData&& other) noexcept;

        CtxtHandle ctxtHandle{};


        Endpoint endpoint{};
        SOCKET   socket{ macroINVALID_SOCKET };

        bool isHandshakeComplete{};
        bool isServer{};

        std::array<std::array<std::byte, 0x4000>, 4> buffers{};
        std::array<SecBuffer, 4> secBuffers{};

        std::array<std::byte, 0x11000> encryptedData{};
        size_t encryptedDataSize{};


        std::array<std::byte, 0x10000> decryptedData{};
        size_t decryptedDataSize{};

        SecPkgContext_StreamSizes contextStreamSizes{};
        size_t decryptedOffset{};
        std::string host{};

        static_assert(SocketType == SocketTypeEnum::STREAM && "DTLS not implemented yet");
    };

    static_assert(SocketDataConcept<TlsSocketData<>>);
}

#include <Hermes/Socket/_base/Data/TlsSocketData.tpp>