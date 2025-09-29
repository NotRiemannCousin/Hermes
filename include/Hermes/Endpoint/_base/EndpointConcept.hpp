#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/_base/WinAPI.hpp>

namespace Hermes {
    using SocketInfoAddr = std::tuple<sockaddr_storage, size_t, AddressFamilyEnum>;


    //! A socket represents one endpoint of a network communication. A connection
    //! is established between two or more sockets. Each endpoint holds the necessary
    //! information to identify itself. To extend StreamSocket and DgramSocket,
    //! you will need a class that satisfies the EndpointConcept.
    template<class T>
    concept EndpointConcept = requires(T t, SocketInfoAddr socketInfo) {
        { T::FromSockAddr(socketInfo) } ->std::same_as<ConnectionResult<T>>;
        { t.ToSockAddr() } ->std::same_as<ConnectionResult<SocketInfoAddr>>;
    };
} // namespace Hermes