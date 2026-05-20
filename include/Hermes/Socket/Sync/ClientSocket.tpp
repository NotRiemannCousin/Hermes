#pragma once

#include <Hermes/Utils/DropLast.hpp>

#include <utility>
#include <ranges>

namespace Hermes {
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::ClientSocket(ClientSocket&& other) noexcept
        : socketData(std::move(other.socketData)),
          connectionPolicy(std::move(other.connectionPolicy)),
          transferPolicy(std::move(other.transferPolicy)) { }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>&
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::operator=(ClientSocket&& other) noexcept {
        if (this != &other) {
            socketData = std::move(other.socketData);
            connectionPolicy = std::move(other.connectionPolicy);
            transferPolicy = std::move(other.transferPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::~ClientSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData &&data) noexcept
        requires std::default_initializable<typename ConnectionPolicy::Options> {
        return Connect(std::move(data), {});
    }


    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData&& data, typename ConnectionPolicy::Options opt) noexcept {
        Network::Initialize();

        ClientSocket socket;
        socket.socketData = std::move(data);

        const auto result{ socket.connectionPolicy.Connect(socket.socketData, opt) };

        if (!result) return std::unexpected{ result.error() };

        return socket;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(R&& data) noexcept {
        using Byte = std::add_const_t<std::ranges::range_value_t<R>>;

        std::span<const Byte> buffer(std::data(data), std::ranges::ssize(data));
        return transferPolicy.Send(socketData, buffer);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return transferPolicy.Recv(socketData, buffer, mode);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<ByteLike Byte> auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvStream() noexcept {
        return typename TransferPolicy::template RecvStream<Byte>{ socketData, transferPolicy };
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class
        TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy,
        SocketData>
    template<ByteLike Byte>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>
    ::RecvRange() noexcept {
        return RecvStream<Byte>() | Utils::dropLast;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        connectionPolicy.Close(const_cast<SocketData&>(socketData));
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class
        TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy,
        SocketData>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        connectionPolicy.Abort(const_cast<SocketData&>(socketData));
    }
}
