#pragma once

#include <utility>

namespace Hermes {
#pragma region Initialization

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenerSocket(ListenerSocket&& other) noexcept
        : m_socketData  (std::move(other.m_socketData)),
          m_acceptPolicy(std::move(other.m_acceptPolicy)) { }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
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
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::~ListenerSocket() {
        Close();
    }

#pragma endregion


#pragma region Properties

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy> requires
        ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetEndpoint() noexcept -> EndpointType& {
        return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy> requires
        ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetEndpoint() const noexcept -> const EndpointType& {
        return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy> requires
        ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetSocketData() noexcept -> SocketData& {
        return m_socketData;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy> requires
        ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetSocketData() const noexcept -> const SocketData& {
        return m_socketData;
    }

#pragma endregion


#pragma region Listen Methods
    
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData &&data, const int backlog) noexcept -> ListenerSocketResult
        requires std::default_initializable<ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data,
            ListenOptions opt, int backlog) noexcept -> ListenerSocketResult {
        Network::Initialize();

        ListenerSocket listener;
        listener.m_socketData = std::move(data);

        const auto result{ listener.m_acceptPolicy.Listen(listener.m_socketData, backlog, opt) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data) noexcept -> ListenerSocketResult
        requires std::default_initializable<ListenOptions> {
        return ListenOne(std::move(data), {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data, ListenOptions opt) noexcept -> ListenerSocketResult {
        return Listen(std::move(data), opt);
    }


#pragma endregion


#pragma region Accept Methods

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne() noexcept
        -> ServerSocketResult
        requires std::default_initializable<AcceptOptions> {
        return AcceptOne(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOneConnection() noexcept
        -> ServerSocketResult
        requires std::default_initializable<AcceptOptions> {
        return AcceptOne(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(AcceptOptions opt) noexcept
        -> ServerSocketResult {
        return AcceptOne(m_socketData, opt);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(const SocketData& clientDataPrototype) noexcept
        -> ServerSocketResult
        requires std::default_initializable<AcceptOptions> {
        return AcceptOne(clientDataPrototype, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(const SocketData& clientDataPrototype, AcceptOptions opt) noexcept
        -> ServerSocketResult {
        SocketData clientData{ clientDataPrototype.MakeChild() };

        const auto result{ m_acceptPolicy.Accept(m_socketData, clientData, opt) };
        if (!result) {
            m_acceptPolicy.Close(clientData);
            return std::unexpected{ result.error() };
        }

        return ServerSocketType::FromAccepted(std::move(clientData));
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll() noexcept
        -> std::generator<ServerSocketResult>
        requires std::default_initializable<AcceptOptions> {
        return AcceptAll(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(AcceptOptions opt) noexcept
        -> std::generator<ServerSocketResult> {
        return AcceptAll(m_socketData, opt);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(const SocketData& clientDataPrototype) noexcept
        -> std::generator<ServerSocketResult>
        requires std::default_initializable<AcceptOptions> {
        return AcceptAll(clientDataPrototype, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(const SocketData& clientDataPrototype, AcceptOptions opt) noexcept
        -> std::generator<ServerSocketResult> {
        while (m_socketData.socket != macroINVALID_SOCKET) {
            auto result{ AcceptOne(clientDataPrototype, opt) };
            const bool isOk{ result.has_value() };

            co_yield std::move(result);
            if (!isOk) break;
        }
    }

#pragma endregion


#pragma region Close Methods

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_acceptPolicy.Close(m_socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;

        m_acceptPolicy.Abort(m_socketData);
    }

#pragma endregion

}
