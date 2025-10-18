#pragma once

namespace Hermes {
    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::TlsSocketData(Endpoint endpoint, std::string host) :
        endpoint{endpoint}, host{ host } {
    }

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::TlsSocketData(TlsSocketData &&other) noexcept :
    socket{ std::exchange(other.socket, macroINVALID_SOCKET) },
        ctxtHandle{std::exchange(other.ctxtHandle, {}) },
        endpoint{ std::move(other.endpoint) },
        isHandshakeComplete{ other.isHandshakeComplete },
        isServer{ other.isServer },
        buffers{ std::move(other.buffers) },
        secBuffers{ std::move(other.secBuffers) },
        contextStreamSizes{ other.contextStreamSizes },
        decryptedOffset{ other.decryptedOffset }, host{} {}

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>& TlsSocketData<Endpoint, SocketType, SocketFamily>::operator=(TlsSocketData &&other) noexcept {
        if (this != &other) {
            isHandshakeComplete = other.isHandshakeComplete;
            isServer            = other.isServer;

            endpoint            = std::move(other.endpoint);
            host                = std::move(other.host);

            buffers             = std::move(other.buffers);
            secBuffers          = std::move(other.secBuffers);
            contextStreamSizes  = other.contextStreamSizes;
            decryptedOffset     = other.decryptedOffset;

            for (int i{}; i < 4; i++)
                secBuffers[i].pvBuffer = buffers[i].data();

            ctxtHandle = std::exchange(other.ctxtHandle, {});
            socket     = std::exchange(other.socket, macroINVALID_SOCKET);
        }

        return *this;
    }
}