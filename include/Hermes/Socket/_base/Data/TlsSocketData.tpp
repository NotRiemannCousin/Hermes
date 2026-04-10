#pragma once
#include <Hermes/_base/WinApi/Macros.hpp>

namespace Hermes {
    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::TlsSocketData(Endpoint endpoint, std::string host,
        const Credentials* credentials) : host{ std::move(host) }, endpoint{endpoint}, credentials { credentials } {}

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::TlsSocketData(TlsSocketData &&other) noexcept :
        ctxtHandle{std::exchange(other.ctxtHandle, {}) },
        endpoint{ std::move(other.endpoint) },
        socket{ std::exchange(other.socket, macroINVALID_SOCKET) },
        isHandshakeComplete{ other.isHandshakeComplete },
        isServer{ other.isServer },
        state{ std::move(other.state) },
        contextStreamSizes{ other.contextStreamSizes },
        decryptedOffset{ other.decryptedOffset },
        handshakeCallback{ other.handshakeCallback },
        pendingData{ other.pendingData },
        credentials{ other.credentials } {}

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>& TlsSocketData<Endpoint, SocketType, SocketFamily>::operator=(TlsSocketData &&other) noexcept {
        if (this != &other) {
            isHandshakeComplete = other.isHandshakeComplete;
            isServer            = other.isServer;

            endpoint            = std::move(other.endpoint);
            host                = std::move(other.host);

            state               = std::move(other.state);
            contextStreamSizes  = other.contextStreamSizes;
            decryptedOffset     = other.decryptedOffset;

            ctxtHandle = std::exchange(other.ctxtHandle, {});
            socket     = std::exchange(other.socket, macroINVALID_SOCKET);

            handshakeCallback = other.handshakeCallback;
            pendingData       = other.pendingData;

            credentials = other.credentials;
        }

        return *this;
    }

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily> TlsSocketData<Endpoint, SocketType, SocketFamily>::
    MakeChild() const {
        auto child{ TlsSocketData{} };

        child.handshakeCallback = handshakeCallback;
        child.pendingData = pendingData;

        child.credentials = credentials;
        child.isServer    = isServer;

        return child;
    }
}
