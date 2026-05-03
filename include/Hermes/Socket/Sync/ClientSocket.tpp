#pragma once

#include <Hermes/Utils/DropLast.hpp>

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
    template<class>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData &&data) noexcept
        requires std::default_initializable<typename ConnectionPolicyType::Options> {
        return Connect(std::move(data), {});
    }


    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData&& data, ConnectionPolicyType::Options opt) noexcept {
        Network::Initialize();

        ClientSocket socket;
        socket.socketData = std::move(data);

        const auto result{ socket.connectionPolicy.Connect(socket.socketData, opt) };

        if (!result) return std::unexpected{ result.error() };

        return socket;
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(R&& data) noexcept {
        using Byte = std::add_const_t<std::ranges::range_value_t<R>>;

        std::span<const Byte> buffer(std::data(data), std::ranges::ssize(data));
        return transferPolicy.Send(socketData, buffer);
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteOper ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R&& data) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return transferPolicy.Recv(socketData, buffer);
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<ByteLike Byte> typename TransferPolicy<SocketData>::template RecvLazyRange<Byte>
    ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::RecvLazyRange() noexcept {
        return typename TransferPolicy<SocketData>::template RecvLazyRange<Byte>{ socketData, transferPolicy };
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class
        TransferPolicy>
    requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy,
        SocketData>
    template<ByteLike Byte>
    auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>
    ::RecvRange() noexcept {
        return RecvLazyRange<Byte>() | Utils::dropLast;
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        connectionPolicy.Close(const_cast<SocketData&>(socketData));
    }

    template<SocketDataConcept SocketData, template <class> class ConnectionPolicy, template <class> class
        TransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy,
        SocketData>
    void ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        connectionPolicy.Abort(const_cast<SocketData&>(socketData));
    }
}
