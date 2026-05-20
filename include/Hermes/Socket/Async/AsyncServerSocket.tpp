#pragma once

#include <utility>
#include <ranges>

namespace Hermes {

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncServerSocket(AsyncServerSocket&& other) noexcept
        : socketData    (std::move(other.socketData)),
          acceptPolicy  (std::move(other.acceptPolicy)),
          transferPolicy(std::move(other.transferPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(AsyncServerSocket&& other) noexcept {
        if (this != &other) {
            Close();
            socketData     = std::move(other.socketData);
            acceptPolicy   = std::move(other.acceptPolicy);
            transferPolicy = std::move(other.transferPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
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
        socket.socketData = std::move(data);
        return socket;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncSend(R&& data) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<const Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));

        return transferPolicy.AsyncSend(socketData, buffer);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncRecv(R&& data, RecvModeEnum mode) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));

        return transferPolicy.AsyncRecv(socketData, buffer, mode);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncShutdown() noexcept {
        return acceptPolicy.AsyncShutdown(socketData);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Close(socketData);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Abort(socketData);
    }

}