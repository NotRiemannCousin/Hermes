#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
    namespace _details {
        template <AddressFamilyEnum>
        struct AcceptOptionsIpv6Base {};

        template <>
        struct AcceptOptionsIpv6Base<AddressFamilyEnum::Inet6> {
            bool onlyIpv6{};
        };
    }


    template<
        EndpointConcept   Endpoint     = IpEndpoint,
        SocketTypeEnum    SocketType   = SocketTypeEnum::Stream,
        AddressFamilyEnum SocketFamily = AddressFamilyEnum::Inet6>
    struct DefaultAcceptPolicy {
        static constexpr auto Family{ SocketFamily };
        static constexpr auto Type{ SocketType };
        using EndpointType = Endpoint;

        struct ListenOptions : _details::AcceptOptionsIpv6Base<Family> {
            bool reuseAddress{ true };

            int recvBufferSize{};
            int sendBufferSize{};
        };
        struct AcceptOptions {
            bool tcpNoDelay{ true };
            bool keepAlive{};

            int recvBufferSize{};
            int sendBufferSize{};
        };

        //! @brief Creates the listening socket: socket() + bind() + listen().
        //! On success, data.socket holds the listening socket handle.

        template<SocketDataConcept Data>
        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        //! @brief Accepts one incoming connection into outData.
        //! Fills outData.socket with the accepted handle and outData.endpoint
        //! with the remote peer address.
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