#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/Socket/_base/_base.hpp>
#include <Hermes/_base/WinApi/WinApi.hpp>
#include <Hermes/_base/Credentials.hpp>

namespace Hermes {
    template<
            EndpointConcept Endpoint       = IpEndpoint,
            SocketTypeEnum SocketType      = SocketTypeEnum::Stream,
            AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct TlsSocketData {
        using EndpointType = Endpoint;

        static constexpr SocketTypeEnum Type = SocketType;
        static constexpr AddressFamilyEnum Family = SocketFamily;

        TlsSocketData() = default;
        TlsSocketData(Endpoint endpoint, std::string host, const Credentials* credentials = nullptr);
        ~TlsSocketData() = default;

        TlsSocketData(TlsSocketData&& other) noexcept;
        TlsSocketData& operator=(TlsSocketData&& other) noexcept;

        TlsSocketData MakeChild() const;

        std::string host{};

        const Credentials* credentials{};

        std::function<ConnectionResultOper(TlsSocketData&)> handshakeCallback{};
        uint32_t pendingData{};

        // private:
        CtxtHandle ctxtHandle{};


        Endpoint endpoint{};
        SOCKET   socket{ macroINVALID_SOCKET };

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

#include <Hermes/Socket/_base/Data/TlsSocketData.tpp>