#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/Data/DefaultSocketData.hpp>

namespace Hermes {
    namespace details_ {
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

    //! @brief Plain TCP connect policy for blocking sockets.
    //! @details Options::connectionTimeout selects the connect strategy: 0 (default) uses the OS blocking connect() as
    //! it is, a non-zero duration temporarily switches the socket to non-blocking and waits on poll()/WSAPoll. Timeout
    //! expiry is reported as ConnectionErrorEnum::ConnectionFailed (no dedicated timeout code).
    template<
        EndpointConcept   Endpoint     = IpEndpoint,
        SocketTypeEnum    SocketType   = SocketTypeEnum::Stream,
        AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct DefaultConnectPolicy {
        static constexpr auto Family{ SocketFamily };
        static constexpr auto Type  { SocketType   };
        using EndpointType = Endpoint;

        struct Options : details_::ConnectOptionsIpv6Base<SocketFamily>, details_::OptionsTcpNoDelayBase<SocketType> {
            std::chrono::milliseconds connectionTimeout{}; //!< 0 = blocking connect, >0 = bounded non-blocking connect.

            bool keepAlive{};     //!< SO_KEEPALIVE.

            int recvBufferSize{}; //!< 0 = OS default.
            int sendBufferSize{}; //!< 0 = OS default.
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