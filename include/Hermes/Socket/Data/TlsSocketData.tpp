#pragma once
#include <Hermes/_base/OsApi/Macros.hpp>
#include <Hermes/Socket/_base/Connection/ITlsConnectStateMachine.hpp>
#include <Hermes/Socket/_base/Transfer/ITlsTransferStateMachine.hpp>
#include <Hermes/Socket/_base/Accept/ITlsAcceptStateMachine.hpp>

namespace Hermes {
    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::TlsSocketData(Endpoint endpoint, std::string host,
        const Credentials* credentials, ExecutionContextType executionContext)
        : host{ std::move(host) }, credentials { credentials }, endpoint{ endpoint }, executionContext { executionContext } {}

    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::~TlsSocketData() = default;
    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::TlsSocketData(TlsSocketData &&other) noexcept :
        host{ std::move(other.host) },
        credentials{ other.credentials },

        connectStateMachine{ std::move(other.connectStateMachine) },
        transferStateMachine{ std::move(other.transferStateMachine) },
        acceptStateMachine{ std::move(other.acceptStateMachine) },

        pendingData{ other.pendingData },
        endpoint{ std::move(other.endpoint) },
        socket{ std::exchange(other.socket, macroINVALID_SOCKET) },
        state{ std::move(other.state) },
        session{ std::move(other.session) },
        executionContext{ other.executionContext } {
        other.host = "MOVED";
    }

    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>&
    TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::operator=(TlsSocketData &&other) noexcept {
        if (this != &other) {
            endpoint            = std::move(other.endpoint);
            host                = std::move(other.host);
            state               = std::move(other.state);
            session             = std::move(other.session);

            socket     = std::exchange(other.socket, macroINVALID_SOCKET);
            pendingData       = other.pendingData;

            connectStateMachine  = std::move(other.connectStateMachine);
            transferStateMachine = std::move(other.transferStateMachine);
            acceptStateMachine   = std::move(other.acceptStateMachine);


            credentials = other.credentials;

            executionContext = std::exchange(other.executionContext, ExecutionContextType{});

            other.host = "MOVED";
        }

        return *this;
    }

    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily> TlsSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::
    MakeChild() const {
        TlsSocketData child{};
        child.pendingData = pendingData;

        child.credentials = credentials;
        child.session     = session.MakeChild();

        child.executionContext = executionContext;

        return child;
    }
}