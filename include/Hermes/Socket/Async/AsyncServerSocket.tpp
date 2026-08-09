#pragma once

#include <utility>
#include <ranges>

namespace Hermes {

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncServerSocket(AsyncServerSocket&& other) noexcept
        : m_socketData    (std::move(other.m_socketData)),
          m_acceptPolicy  (std::move(other.m_acceptPolicy)),
          m_transferPolicy(std::move(other.m_transferPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
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
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::~AsyncServerSocket() {
        Close();
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::FromAccepted(SocketData&& data) noexcept {
        AsyncServerSocket socket;
        socket.m_socketData = std::move(data);
        return socket;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<ContiguousByteRange R>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(R&& data) {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer));
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<WritableContiguousByteRange R>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Shutdown() noexcept {
        return m_acceptPolicy.Shutdown(m_socketData);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_acceptPolicy.Close(m_socketData);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_acceptPolicy.Abort(m_socketData);
    }

}