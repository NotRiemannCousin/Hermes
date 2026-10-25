#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
    namespace details_ {
        template <AddressFamilyEnum>
        struct AcceptOptionsIpv6Base {};

        template <>
        struct AcceptOptionsIpv6Base<AddressFamilyEnum::Inet6> {
            bool onlyIpv6{};
        };
    }


    //! @brief Plain TCP listen/accept policy for blocking sockets.
    //! @details Size and flag options follow the "0/false = leave OS default" convention.
    //! Close() performs shutdown(Send) + close, Abort() sets SO_LINGER(1,0) and closes immediately (RST).
    template<
        EndpointConcept   Endpoint     = IpEndpoint,
        SocketTypeEnum    SocketType   = SocketTypeEnum::Stream,
        AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct DefaultAcceptPolicy {
        static constexpr auto Family{ SocketFamily };
        static constexpr auto Type  { SocketType   };
        using EndpointType = Endpoint;

        //! @note onlyIpv6 (conditionally inherited base, Inet6 only) controls IPV6_V6ONLY.
        struct ListenOptions : details_::AcceptOptionsIpv6Base<Family> {
            bool reuseAddress{ true };  //!< SO_REUSEADDR before bind (default: on).

            int recvBufferSize{};       //!< SO_RCVBUF. 0 = OS default.
            int sendBufferSize{};       //!< SO_SNDBUF. 0 = OS default.
        };
        struct AcceptOptions {
            bool tcpNoDelay{ true };    //!< TCP_NODELAY on the accepted socket (Stream only).
            bool keepAlive{};           //!< SO_KEEPALIVE on the accepted socket.

            int recvBufferSize{};       //!< 0 = OS default.
            int sendBufferSize{};       //!< 0 = OS default.
        };

        //! @brief Creates the listening socket: socket() + bind() + listen().
        //! @details On success, data.socket holds the listening socket handle.
        template<SocketDataConcept Data>
        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        //! @brief Accepts one incoming connection into outData.
        //! @details Fills outData.socket with the accepted handle and outData.endpoint with the remote peer address.
        template<SocketDataConcept Data>
        static ConnectionResultOper Accept(Data& listenData, Data& outData, AcceptOptions options) noexcept;

        //! @brief Closes an accepted connection with a proper TCP shutdown.
        template<SocketDataConcept Data>
        static void Close(Data& data) noexcept;

        //! @brief Closes the listening socket. No protocol-level shutdown required.
        template<SocketDataConcept Data>
        static void Abort(Data& data) noexcept;
    };

}

#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AcceptPolicyConcept<DefaultAcceptPolicy<>, DefaultSocketData<>>);
}