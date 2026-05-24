#pragma once

#include <utility>
#include <exec/create.hpp>
#include <ranges>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

namespace Hermes {
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>

    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncClientSocket(AsyncClientSocket&& other) noexcept
        : socketData      (std::move(other.socketData)),
          connectionPolicy(std::move(other.connectionPolicy)),
          transferPolicy  (std::move(other.transferPolicy)) { }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
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
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::~AsyncClientSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<class>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData &&data) noexcept
        requires std::default_initializable<typename ConnectionPolicy::Options> {
        return Connect(std::move(data), {});
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData&& data, typename ConnectionPolicy::Options opt) noexcept {
        Network::Initialize();

        return stdexec::just(AsyncClientSocket{})
             | stdexec::let_value(
                 [data = std::move(data), opt = std::move(opt)](AsyncClientSocket& socket) mutable {
                     socket.socketData = std::move(data);

                     return socket.connectionPolicy.Connect(socket.socketData, opt)
                            | stdexec::let_value(Utils::Overloaded{
                                [&socket]() mutable         { return stdexec::just(std::move(socket)); },
                                [](ConnectionErrorEnum err) { return stdexec::just_error(err); }
                            });
                 }
             );
    }

    // TODO: FUTURE: Implement loop (3x? 5x? infinite? idk)
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(R&& data) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return transferPolicy.Send(socketData, std::as_bytes(buffer));;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return transferPolicy.Recv(socketData, std::as_writable_bytes(buffer), mode);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Shutdown() noexcept {
        return connectionPolicy.Shutdown(socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        connectionPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        connectionPolicy.Abort(socketData);
    }
}
