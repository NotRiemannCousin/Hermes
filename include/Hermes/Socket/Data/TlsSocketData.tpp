#pragma once
#include <Hermes/_base/OsApi/Macros.hpp>
#include <Hermes/Socket/_base/Connection/ITlsConnectStateMachine.hpp>
#include <Hermes/Socket/_base/Transfer/ITlsTransferStateMachine.hpp>
#include <Hermes/Socket/_base/Accept/ITlsAcceptStateMachine.hpp>

namespace Hermes {
    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::TlsSocketData(Endpoint endpoint, std::string host,
        const Credentials* credentials) : host{ std::move(host) }, endpoint{endpoint}, credentials { credentials } {}

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::~TlsSocketData() = default;

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily>::TlsSocketData(TlsSocketData &&other) noexcept :
        ctxtHandle{ std::exchange(other.ctxtHandle, {}) },
        endpoint{ std::move(other.endpoint) },
        socket{ std::exchange(other.socket, macroINVALID_SOCKET) },
        isHandshakeComplete{ other.isHandshakeComplete },
        isServer{ other.isServer },
        state{ std::move(other.state) },
        contextStreamSizes{ other.contextStreamSizes },
        decryptedOffset{ other.decryptedOffset },
        pendingData{ other.pendingData },
        host{ std::move(other.host) },
        connectStateMachine{ std::move(other.connectStateMachine) },
        transferStateMachine{ std::move(other.transferStateMachine) },
        acceptStateMachine{ std::move(other.acceptStateMachine) },
        credentials{ other.credentials } {
        other.host = "MOVED";
    }

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

            pendingData       = other.pendingData;

            connectStateMachine  = std::move(other.connectStateMachine);
            transferStateMachine = std::move(other.transferStateMachine);
            acceptStateMachine   = std::move(other.acceptStateMachine);


            credentials = other.credentials;

            other.host = "MOVED";
        }

        return *this;
    }

    template<EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<Endpoint, SocketType, SocketFamily> TlsSocketData<Endpoint, SocketType, SocketFamily>::
    MakeChild() const {
        auto child{ TlsSocketData{} };

        child.pendingData = pendingData;

        child.credentials = credentials;
        child.isServer    = isServer;

        return child;
    }
}