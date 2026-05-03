#pragma once

#include <utility>
#include <ranges>

#include <Hermes/Utils/DropLast.hpp>

namespace Hermes {

    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocket(ServerSocket&& other) noexcept
        : socketData    (std::move(other.socketData)),
          acceptPolicy  (std::move(other.acceptPolicy)),
          transferPolicy(std::move(other.transferPolicy)) { }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(ServerSocket&& other) noexcept {
        if (this != &other) {
            socketData     = std::move(other.socketData);
            acceptPolicy   = std::move(other.acceptPolicy);
            transferPolicy = std::move(other.transferPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::~ServerSocket() {
        Close();
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ServerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::FromAccepted(SocketData&& data) noexcept {
        ServerSocket socket;
        socket.socketData = std::move(data);
        return socket;
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(R&& data) noexcept {
        using Byte = std::add_const_t<std::ranges::range_value_t<R>>;

        std::span<Byte> buffer(std::data(data), std::ranges::ssize(data));
        return transferPolicy.Send(socketData, buffer);
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
    StreamByteOper ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(R&& data) noexcept {
        std::span buffer(std::ranges::data(data), std::ranges::ssize(data));

        return transferPolicy.Recv(socketData, buffer);
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<ByteLike Byte>
    typename TransferPolicy<SocketData>::template RecvLazyRange<Byte>
    ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvLazyRange() noexcept {
        return typename TransferPolicy<SocketData>::template RecvLazyRange<Byte>{ socketData, transferPolicy };
    }

    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<ByteLike Byte>
    auto ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::RecvRange() noexcept {
        return RecvLazyRange<Byte>() | Utils::dropLast;
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Close(socketData);
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Abort(socketData);
    }

}
