#pragma once

#include <Hermes/Utils/DropLast.hpp>

#include <utility>
#include <ranges>

namespace Hermes {

#pragma region Constructor

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::ClientSocket(ClientSocket&& other) noexcept
        : m_socketData(std::move(other.m_socketData)),
          m_connectionPolicy(std::move(other.m_connectionPolicy)),
          m_transferPolicy(std::move(other.m_transferPolicy)) { }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>&
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::operator=(ClientSocket&& other) noexcept {
        if (this != &other) {
            m_socketData = std::move(other.m_socketData);
            m_connectionPolicy = std::move(other.m_connectionPolicy);
            m_transferPolicy = std::move(other.m_transferPolicy);

            other.m_socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::~ClientSocket() {
        Close();
    }

#pragma endregion


#pragma region Connect

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<class>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData &&data) noexcept
        requires std::default_initializable<ConnOptions> {
        return Connect(std::move(data), {});
    }


    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData&& data, ConnOptions opt) noexcept {
        Network::Initialize();

        ClientSocket socket;
        socket.m_socketData = std::move(data);

        const auto result{ socket.m_connectionPolicy.Connect(socket.m_socketData, opt) };

        if (!result) return std::unexpected{ result.error() };

        return socket;
    }

#pragma endregion


#pragma region Transfer

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(R&& data) noexcept
        requires std::default_initializable<SendOptions> {
        return Send(std::forward<R>(data), SendOptions{});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(
        R&& data, SendOptions options
    ) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer), options);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy> requires ClientSocketConcept<
        SocketData, ConnectionPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::SendAndLift(R&& data) noexcept
        requires std::default_initializable<SendOptions> {
        return SendAndLift(std::forward<R>(data), {});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy> requires ClientSocketConcept<
        SocketData, ConnectionPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::SendAndLift(R&& data, SendOptions options) noexcept {
        return [options, fData{ std::forward<R>(data) }](ClientSocket&& self) mutable -> ConnectionResult<ClientSocket> {
            auto val{ self.Send(std::move(fData), options) };

            auto movFromThis{ [client{ std::move(self) }](const auto...) mutable {
                return std::move(client);
            } };
            return val.second.transform(movFromThis);
        };
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept
        requires std::default_initializable<RecvOptions> {
        return Recv(std::forward<R>(data), mode, RecvOptions{});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(
        R&& data, RecvModeEnum mode, RecvOptions options
    ) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode, options);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(
        R&& data, RecvOptions options
    ) noexcept {
        return Recv(std::forward<R>(data), RecvModeEnum::All, options);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
    requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvStream() noexcept
        requires std::default_initializable<RecvOptions> {
        return RecvStream<Byte>(RecvOptions{});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvStream(RecvOptions options) noexcept {
        return typename TransferPolicy::template RecvStream<Byte>{ m_socketData, m_transferPolicy, options };
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvRange() noexcept
        requires std::default_initializable<RecvOptions> {
        return RecvRange<Byte>(RecvOptions{});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ByteLike Byte>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvRange(RecvOptions options) noexcept {
        return RecvStream<Byte>(options) | Utils::dropLast;
    }

#pragma endregion


#pragma region Close

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_connectionPolicy.Close(const_cast<SocketData&>(m_socketData));
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
    requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_connectionPolicy.Abort(const_cast<SocketData&>(m_socketData));
    }


#pragma endregion

}
