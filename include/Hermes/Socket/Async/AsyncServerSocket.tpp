#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <utility>
#include <ranges>

namespace Hermes {
#pragma region Initialization

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncServerSocket(AsyncServerSocket&& other) noexcept
        : m_socketData    (std::move(other.m_socketData)),
          m_acceptPolicy  (std::move(other.m_acceptPolicy)),
          m_transferPolicy(std::move(other.m_transferPolicy)) { }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(AsyncServerSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socketData     = std::move(other.m_socketData);
            m_acceptPolicy   = std::move(other.m_acceptPolicy);
            m_transferPolicy = std::move(other.m_transferPolicy);

            other.m_socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::~AsyncServerSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::FromAccepted(SocketData&& data) noexcept {
        AsyncServerSocket socket;
        socket.m_socketData = std::move(data);
        return socket;
    }

#pragma endregion


#pragma region Properties

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetEndpoint() noexcept -> EndpointType& {
        return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetEndpoint() const noexcept -> const EndpointType& {
        return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetSocketData() noexcept -> SocketData& {
        return m_socketData;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetSocketData() const noexcept -> const SocketData& {
        return m_socketData;
    }

#pragma endregion


#pragma region Send Methods

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(R&& data) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer));
    }

#pragma endregion


#pragma region Recv Methods

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode);
    }

#pragma endregion


#pragma region Close Methods

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Shutdown() noexcept {
        return m_acceptPolicy.Shutdown(m_socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    void AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_acceptPolicy.Close(m_socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    void AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_acceptPolicy.Abort(m_socketData);
    }

#pragma endregion

}

#endif