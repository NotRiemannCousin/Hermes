#pragma once

namespace Hermes {
    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    DefaultSocketData<Endpoint, SocketType, SocketFamily>::DefaultSocketData(DefaultSocketData &&other) noexcept :
                socket{std::exchange(other.socket, macroINVALID_SOCKET)} {}

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    DefaultSocketData<Endpoint, SocketType, SocketFamily>& DefaultSocketData<Endpoint, SocketType, SocketFamily>::operator=(DefaultSocketData &&other) noexcept {
        if (this != &other) {
            endpoint = std::move(other.endpoint);
            socket   = std::exchange(other.socket, macroINVALID_SOCKET);
        }

        return *this;
    }
}