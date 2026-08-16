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
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(R&& data) noexcept
        requires std::default_initializable<SendOptions> {
        return Send(std::forward<R>(data), SendOptions{});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(
        R&& data, SendOptions options
    ) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer), options);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy> requires ServerSocketConcept<
        SocketData, AcceptPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::SendAndLift(R&& data) noexcept
        requires std::default_initializable<SendOptions> {
        return SendAndLift(std::forward<R>(data), {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy> requires ServerSocketConcept<
        SocketData, AcceptPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::SendAndLift(R&& data, SendOptions options) noexcept {
        return [options, fData{ std::forward<R>(data) }](ServerSocket&& self) mutable -> ConnectionResult<ServerSocket> {
            auto val{ self.Send(std::move(fData), options) };

            auto movFromThis{ [client{ std::move(self) }](const auto) mutable {
                return std::move(client);
            } };
            return val.second.transform(movFromThis);
        };
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept
        requires std::default_initializable<RecvOptions> {
        return Recv(std::forward<R>(data), mode, RecvOptions{});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(
        R&& data, RecvModeEnum mode, RecvOptions options
    ) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode, options);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(
        R&& data, RecvOptions options
    ) noexcept {
        return Recv(std::forward<R>(data), RecvModeEnum::All, options);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvStream() noexcept
        requires std::default_initializable<RecvOptions> {
        return RecvStream<Byte>(RecvOptions{});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvStream(
        RecvOptions options
    ) noexcept {
        return typename TransferPolicy::template RecvStream<Byte>{ m_socketData, m_transferPolicy, options };
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvRange() noexcept
        requires std::default_initializable<RecvOptions> {
        return RecvRange<Byte>(RecvOptions{});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvRange(
        RecvOptions options
    ) noexcept {
        return RecvStream<Byte>(options) | Utils::dropLast;
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
