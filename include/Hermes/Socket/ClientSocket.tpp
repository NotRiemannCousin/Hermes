#pragma once

#include <utility>
#include <ranges>

namespace Hermes {
    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::ClientSocket(ClientSocket&& other) noexcept
        : socketData(std::move(other.socketData)),
          connectionPolicy(std::move(other.connectionPolicy)),
          transferPolicy(std::move(other.transferPolicy)) { }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
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

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::~ClientSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(const typename SocketData::EndpointType& endpoint, SocketData data) noexcept {
        ClientSocket socket;
        socket.socketData = std::move(data);

        socket.socketData.endpoint = endpoint;
        const auto result = socket.connectionPolicy.Connect(socket.socketData);

        if (!result) return std::unexpected{ result.error() };

        return socket;
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
    requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteCount ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(R& data) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));
        return transferPolicy.Send(socketData, buffer);
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
    requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteCount ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R& data) noexcept {
        std::span<const std::byte> buffer(std::data(data), std::ranges::ssize(data));
        return transferPolicy.Recv(socketData, buffer);
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<ByteLike Byte> typename TransferPolicy<SocketData>::template RecvRange<Byte>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvRange() noexcept {
        return typename TransferPolicy<SocketData>::template RecvRange<Byte>{socketData, transferPolicy};
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        connectionPolicy.Close(const_cast<SocketData&>(socketData));
    }

} // namespace Hermes
