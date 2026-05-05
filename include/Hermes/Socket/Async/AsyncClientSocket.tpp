#pragma once

#include <utility>
#include <ranges>

namespace Hermes {
    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::AsyncClientSocket(AsyncClientSocket&& other) noexcept
        : socketData      (std::move(other.socketData)),
          connectionPolicy(std::move(other.connectionPolicy)),
          transferPolicy  (std::move(other.transferPolicy)) { }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>&
    AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::operator=(AsyncClientSocket&& other) noexcept {
        if (this != &other) {
            Close();
            socketData       = std::move(other.socketData);
            connectionPolicy = std::move(other.connectionPolicy);
            transferPolicy   = std::move(other.transferPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::~AsyncClientSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    template<class>
    auto AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::AsyncConnect(SocketData &&data) noexcept
        requires std::default_initializable<typename ConnectionPolicyType::Options> {
        return AsyncConnect(std::move(data), {});
    }


    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    auto AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::AsyncConnect(SocketData&& data, ConnectionPolicyType::Options opt) noexcept {
        Network::Initialize();

        // stdexec::just injects the uninitialized socket into the operation state to guarantee lifetime
        return stdexec::just(AsyncClientSocket{})
             | stdexec::let_value([data = std::move(data), opt = std::move(opt)](AsyncClientSocket& socket) mutable {
                   socket.socketData = std::move(data);
                   return socket.connectionPolicy.AsyncConnect(socket.socketData, opt)
                        | stdexec::then([&socket]() mutable {
                              return std::move(socket);
                          });
               });
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::AsyncSend(R&& data) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<const Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));
        return transferPolicy.AsyncSend(socketData, buffer);
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::AsyncRecv(R&& data, RecvModeEnum mode) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));
        return transferPolicy.AsyncRecv(socketData, buffer, mode);
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    auto AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::AsyncShutdown() noexcept {
        return connectionPolicy.AsyncShutdown(socketData);
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    void AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        connectionPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, template <class> class AsyncConnectionPolicy, template <class> class AsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    void AsyncClientSocket<SocketData, AsyncConnectionPolicy, AsyncTransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        connectionPolicy.Abort(socketData);
    }
}