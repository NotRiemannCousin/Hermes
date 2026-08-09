#pragma once

#include <utility>

namespace Hermes {

#pragma region Constructors

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenerSocket(ListenerSocket&& other) noexcept
        : m_socketData  (std::move(other.m_socketData)),
          m_acceptPolicy(std::move(other.m_acceptPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(ListenerSocket&& other) noexcept {
        if (this != &other) {
            Close();

            m_socketData   = std::move(other.m_socketData);
            m_acceptPolicy = std::move(other.m_acceptPolicy);

            other.m_socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::~ListenerSocket() {
        Close();
    }

#pragma endregion


#pragma region Listen
    
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData &&data, int backlog) noexcept -> ListenerSockerResult
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data,
            typename AcceptPolicy::ListenOptions opt, int backlog) noexcept -> ListenerSockerResult {
        Network::Initialize();

        ListenerSocket listener;
        listener.m_socketData = std::move(data);

        const auto result{ listener.m_acceptPolicy.Listen(listener.m_socketData, backlog, opt) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data) noexcept -> ListenerSockerResult
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return ListenOne(std::move(data), {});
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data, typename AcceptPolicy::ListenOptions opt) noexcept -> ListenerSockerResult {
        return Listen(std::move(data), opt);
    }


#pragma endregion


#pragma region Accept

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne() noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptOne(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOneConnection() noexcept requires std::
        default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptOne(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(typename AcceptPolicy::AcceptOptions opt) noexcept {
        return AcceptOne(m_socketData, opt);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(const SocketData& clientDataPrototype) noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptOne(clientDataPrototype, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(const SocketData& clientDataPrototype, typename AcceptPolicy::AcceptOptions opt) noexcept {
        SocketData clientData{ clientDataPrototype.MakeChild() };

        const auto result{ m_acceptPolicy.Accept(m_socketData, clientData, opt) };
        if (!result) return std::unexpected{ result.error() };

        return ServerSocketType::FromAccepted(std::move(clientData));
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::
    ServerSocketType>> ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll() noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptAll(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(typename AcceptPolicy::AcceptOptions opt) noexcept {
        return AcceptAll(m_socketData, opt);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::
    ServerSocketType>> ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(const SocketData& clientDataPrototype) noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptAll(clientDataPrototype, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(const SocketData& clientDataPrototype, typename AcceptPolicy::AcceptOptions opt) noexcept {
        while (m_socketData.socket != macroINVALID_SOCKET) {
            auto result{ AcceptOne(clientDataPrototype, opt) };
            const bool isOk{ result.has_value() };

            co_yield std::move(result);
            if (!isOk) break;
        }
    }

#pragma endregion


#pragma region Close

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_acceptPolicy.Close(m_socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_acceptPolicy.Abort(m_socketData);
    }

#pragma endregion

}
