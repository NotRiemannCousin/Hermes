#pragma once

#include <utility>
#include <ranges>

#include <Hermes/Utils/DropLast.hpp>

namespace Hermes {

#pragma region Constructor

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocket(ServerSocket&& other) noexcept
        : m_socketData    (std::move(other.m_socketData)),
          m_acceptPolicy  (std::move(other.m_acceptPolicy)),
          m_transferPolicy(std::move(other.m_transferPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(ServerSocket&& other) noexcept {
        if (this != &other) {
            m_socketData     = std::move(other.m_socketData);
            m_acceptPolicy   = std::move(other.m_acceptPolicy);
            m_transferPolicy = std::move(other.m_transferPolicy);

            other.m_socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::~ServerSocket() {
        Close();
    }

#pragma endregion



    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ConnectionResult<ServerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::FromAccepted(SocketData&& data) noexcept {
        ServerSocket socket;
        socket.m_socketData = std::move(data);
        return socket;
    }



#pragma region Transfer

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(R&& data) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer));
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvStream() noexcept {
        return typename TransferPolicy::template RecvStream<Byte>{ m_socketData, m_transferPolicy };
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvRange() noexcept {
        return RecvStream<Byte>() | Utils::dropLast;
    }

#pragma endregion



#pragma region Close

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_acceptPolicy.Close(m_socketData);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_acceptPolicy.Abort(m_socketData);
    }

#pragma endregion

}
