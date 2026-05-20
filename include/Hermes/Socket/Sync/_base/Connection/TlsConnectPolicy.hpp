#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    struct TlsAsyncConnectPolicy;

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsConnectPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;

        static constexpr bool IsServer{ false };

        using DataType = TlsSocketData<EndpointType, Type, Family>;

        struct Options : DefaultConnectPolicy<EndpointType, SocketTypeEnum::Stream, Family>::Options {
            std::chrono::milliseconds handshakeTimeout{}; // std::chrono::seconds{ 10 }

            bool ignoreCertificateErrors{};
            bool requestMutualAuth{};

            // no ALPN/mTLS for now
            // std::span<const std::string_view> alpnProtocols{};
        };

        ConnectionResultOper Connect(Data& data, Options options) noexcept;

        void Close(Data& data);

        void Abort(Data& data);

        ConnectionResultOper Renegotiate(Data& data);

        TlsConnectPolicy() noexcept = default;
        TlsConnectPolicy(TlsConnectPolicy&&) noexcept = default;

    private:
        ConnectionResultOper ClientHandshake(Data& data);

        friend struct TlsAsyncConnectPolicy<Data>;

    };
}

#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.tpp>

namespace Hermes {
    static_assert(ConnectionPolicyConcept<TlsConnectPolicy<>, TlsSocketData<>>);
}