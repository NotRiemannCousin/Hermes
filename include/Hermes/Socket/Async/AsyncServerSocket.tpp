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
    template<ContiguousByteRange R>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Send(R&& data) {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return transferPolicy.Send(socketData, std::as_bytes(buffer));
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<WritableContiguousByteRange R>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return transferPolicy.Recv(socketData, std::as_writable_bytes(buffer), mode);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>::Shutdown() noexcept {
        return acceptPolicy.Shutdown(socketData);
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