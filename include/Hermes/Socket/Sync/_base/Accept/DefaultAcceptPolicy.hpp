#pragma once
#include <Hermes/Socket/Sync/_base/Data/DefaultSocketData.hpp>
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


    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultAcceptPolicy {
        struct ListenOptions : _details::AcceptOptionsIpv6Base<Data::Family> {
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
        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;

        //! @brief Accepts one incoming connection into outData.
        //! Fills outData.socket with the accepted handle and outData.endpoint
        //! with the remote peer address.
        static ConnectionResultOper Accept(Data& listenData, Data& outData, AcceptOptions options) noexcept;

        //! @brief Closes an accepted connection with a proper TCP shutdown.
        static void Close(Data& data) noexcept;

        //! @brief Closes the listening socket. No protocol-level shutdown required.
        static void Abort(Data& data) noexcept;
    };

}

#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AcceptPolicyConcept<DefaultAcceptPolicy, DefaultSocketData<>>);
}