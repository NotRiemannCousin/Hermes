#pragma once

#include <utility>
#include <exec/create.hpp>
#include <ranges>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

namespace Hermes {
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>

    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncClientSocket(AsyncClientSocket&& other) noexcept
        : socketData      (std::move(other.socketData)),
          connectionPolicy(std::move(other.connectionPolicy)),
          transferPolicy  (std::move(other.transferPolicy)) { }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>&
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::operator=(AsyncClientSocket&& other) noexcept {
        if (this != &other) {
            Close();
            socketData       = std::move(other.socketData);
            connectionPolicy = std::move(other.connectionPolicy);
            transferPolicy   = std::move(other.transferPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::~AsyncClientSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncConnect(SocketData &&data) noexcept
        requires std::default_initializable<typename ConnectionPolicy::Options> {
        return AsyncConnect(std::move(data), {});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncConnect(SocketData&& data, typename ConnectionPolicy::Options opt) noexcept {
        Network::Initialize();

        return stdexec::just(AsyncClientSocket{})
             | stdexec::let_value(
                 [data = std::move(data), opt = std::move(opt)](AsyncClientSocket& socket) mutable {
                     socket.socketData = std::move(data);

                     return socket.connectionPolicy.AsyncConnect(socket.socketData, opt)
                            | stdexec::let_value(Utils::Overloaded{
                                [&socket]() mutable         { return stdexec::just(std::move(socket)); },
                                [](ConnectionErrorEnum err) { return stdexec::just_error(err); }
                            });
                 }
             );
    }

    // TODO: FUTURE: Implement loop (3x? 5x? infinite? idk)
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncSend(R&& data) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<const Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));

        return transferPolicy.AsyncSend(socketData, buffer);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<std::ranges::contiguous_range R>
        requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncRecv(R&& data, RecvModeEnum mode) noexcept {
        using Byte = std::remove_cv_t<std::ranges::range_value_t<R>>;
        std::span<Byte> buffer(std::ranges::data(data), std::ranges::ssize(data));

        return transferPolicy.AsyncRecv(socketData, buffer, mode);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncShutdown() noexcept {
        return connectionPolicy.AsyncShutdown(socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        connectionPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        connectionPolicy.Abort(socketData);
    }
}
