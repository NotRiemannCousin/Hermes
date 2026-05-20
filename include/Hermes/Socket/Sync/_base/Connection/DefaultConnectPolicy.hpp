#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/Data/DefaultSocketData.hpp>

namespace Hermes {
    namespace _details {
        template <AddressFamilyEnum>
        struct ConnectOptionsIpv6Base {};

        template <>
        struct ConnectOptionsIpv6Base<AddressFamilyEnum::Inet6> {
            bool onlyIpv6{};
        };

        template <SocketTypeEnum>
        struct OptionsTcpNoDelayBase {};

        template <>
        struct OptionsTcpNoDelayBase<SocketTypeEnum::Stream> {
            bool tcpNoDelay{ true };
        };
    }


    template<
        EndpointConcept   Endpoint     = IpEndpoint,
        SocketTypeEnum    SocketType   = SocketTypeEnum::Stream,
        AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct DefaultConnectPolicy {
        static constexpr auto Family{ SocketFamily };
        static constexpr auto Type{ SocketType };
        using EndpointType = Endpoint;

        struct Options : _details::ConnectOptionsIpv6Base<SocketFamily>, _details::OptionsTcpNoDelayBase<SocketType> {
            std::chrono::milliseconds connectionTimeout{};

            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};
        };

        template<SocketDataConcept Data>
        static ConnectionResultOper Connect(Data& data, Options options) noexcept;

        template<SocketDataConcept Data>
        static void Close(Data& data);

        template<SocketDataConcept Data>
        static void Abort(Data& data);
    };
}

#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.tpp>

namespace Hermes {
    static_assert(ConnectionPolicyConcept<DefaultConnectPolicy<>, DefaultSocketData<>>);
}