#pragma once

namespace Hermes {
    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    DefaultSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::DefaultSocketData(Endpoint other) : endpoint{ other } {
    }

    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    DefaultSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::DefaultSocketData(DefaultSocketData &&other) noexcept :
                socket{ std::exchange(other.socket, macroINVALID_SOCKET) },
                endpoint{ other.endpoint }, executionContext{ other.executionContext } {}

    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    DefaultSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>&
    DefaultSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::operator=(DefaultSocketData &&other) noexcept {
        if (this != &other) {
            endpoint         = std::move(other.endpoint);
            socket           = std::exchange(other.socket, macroINVALID_SOCKET);
            options          = std::exchange(other.options, {});
            executionContext = std::exchange(other.executionContext, ExecutionContextType{});
        }

        return *this;
    }

    template<class ExecutionContext, EndpointConcept Endpoint, SocketTypeEnum SocketType, AddressFamilyEnum SocketFamily>
    DefaultSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily> DefaultSocketData<ExecutionContext, Endpoint, SocketType, SocketFamily>::
    MakeChild() const {
        DefaultSocketData data{};
        data.executionContext = executionContext;
        return data;
    }
}
