#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <utility>
#include <exec/create.hpp>
#include <ranges>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

namespace Hermes {
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>

    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncClientSocket(AsyncClientSocket&& other) noexcept
        : m_socketData      (std::move(other.m_socketData)),
          m_connectionPolicy(std::move(other.m_connectionPolicy)),
          m_transferPolicy  (std::move(other.m_transferPolicy)) { }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>&
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::operator=(AsyncClientSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socketData       = std::move(other.m_socketData);
            m_connectionPolicy = std::move(other.m_connectionPolicy);
            m_transferPolicy   = std::move(other.m_transferPolicy);

            other.m_socketData.socket = macroINVALID_SOCKET;
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
                     socket.m_socketData = std::move(data);

                     return socket.m_connectionPolicy.Connect(socket.m_socketData, opt)
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

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer));;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Shutdown() noexcept {
        return m_connectionPolicy.Shutdown(m_socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_connectionPolicy.Close(m_socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_connectionPolicy.Abort(m_socketData);
    }
}


#endif