#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <utility>
#include <exec/create.hpp>
#include <ranges>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

namespace Hermes {
#pragma region Initialization

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::AsyncClientSocket(AsyncClientSocket&& other) noexcept
        : m_socketData      (std::move(other.m_socketData)),
          m_connectionPolicy(std::move(other.m_connectionPolicy)),
          m_transferPolicy  (std::move(other.m_transferPolicy)) { }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
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
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::~AsyncClientSocket() {
        Close();
    }

#pragma endregion


#pragma region Properties

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::GetEndpoint() noexcept -> EndpointType& {
         return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::GetEndpoint() const noexcept -> const EndpointType&{
         return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::GetSocketData() noexcept -> SocketData& {
         return m_socketData;
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::GetSocketData() const noexcept -> const SocketData& {
         return m_socketData;
    }

#pragma endregion


#pragma region Connection Methods

        template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
            requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
        template<class>
        auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData &&data) noexcept
            requires std::default_initializable<ConnOptions> {
            return Connect(std::move(data), {});
        }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Connect(SocketData&& data, ConnOptions opt) noexcept {
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

#pragma endregion


#pragma region Send Methods

    // TODO: FUTURE: Implement loop (3x? 5x? infinite? idk)
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<ContiguousByteRange R>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Send(R&& data) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Send(m_socketData, std::as_bytes(buffer));;
    }

#pragma endregion


#pragma region Recv Methods

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    template<WritableContiguousByteRange R>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Recv(R&& data, RecvModeEnum mode) noexcept {
        std::span buffer(std::data(data), std::ranges::ssize(data));

        return m_transferPolicy.Recv(m_socketData, std::as_writable_bytes(buffer), mode);
    }

#pragma endregion


#pragma region Close Methods

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    auto AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Shutdown() noexcept {
        return m_connectionPolicy.Shutdown(m_socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_connectionPolicy.Close(m_socketData);
    }

    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    void AsyncClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_connectionPolicy.Abort(m_socketData);
    }

#pragma endregion
}


#endif