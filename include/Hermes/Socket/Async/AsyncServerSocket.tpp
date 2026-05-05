#pragma once

#include <utility>
#include <ranges>

namespace Hermes {

    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::AsyncServerSocket(AsyncServerSocket&& other) noexcept
        : socketData    (std::move(other.socketData)),
          acceptPolicy  (std::move(other.acceptPolicy)),
          transferPolicy(std::move(other.transferPolicy)) { }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>&
    AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::operator=(AsyncServerSocket&& other) noexcept {
        if (this != &other) {
            Close();
            socketData     = std::move(other.socketData);
            acceptPolicy   = std::move(other.acceptPolicy);
            transferPolicy = std::move(other.transferPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::~AsyncServerSocket() {
        Close();
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    ConnectionResult<AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>>
    AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::FromAccepted(SocketData&& data) noexcept {
        AsyncServerSocket socket;
        socket.socketData = std::move(data);
        return socket;
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::AsyncSend(R&& data) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<const Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));
        return transferPolicy.AsyncSend(socketData, buffer);
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::AsyncRecv(R&& data, RecvModeEnum mode) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));
        return transferPolicy.AsyncRecv(socketData, buffer, mode);
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    auto AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::AsyncShutdown() noexcept {
        return acceptPolicy.AsyncShutdown(socketData);
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    void AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Close(socketData);
    }


    template<SocketDataConcept SocketData, template <class> class AsyncAcceptPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    void AsyncServerSocket<SocketData, AsyncAcceptPolicy, AsyncTransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Abort(socketData);
    }

}