#pragma once

#include <utility>

namespace Hermes {

#pragma region Constructors

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenerSocket(ListenerSocket&& other) noexcept
        : socketData  (std::move(other.socketData)),
          acceptPolicy(std::move(other.acceptPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(ListenerSocket&& other) noexcept {
        if (this != &other) {
            Close();

            socketData   = std::move(other.socketData);
            acceptPolicy = std::move(other.acceptPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
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
        listener.socketData = std::move(data);

        const auto result{ listener.acceptPolicy.Listen(listener.socketData, backlog, opt) };
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
        return AcceptOne({});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOneConnection() noexcept requires std::
        default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptOne();
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(typename AcceptPolicy::AcceptOptions opt) noexcept {
        SocketData clientData{ socketData.MakeChild() };

        const auto result{ acceptPolicy.Accept(socketData, clientData, opt) };
        if (!result) return std::unexpected{ result.error() };

        return ServerSocketType::FromAccepted(std::move(clientData));
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::
    ServerSocketType>> ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll() noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        co_yield std::ranges::elements_of(AcceptAll({}));
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(typename AcceptPolicy::AcceptOptions opt) noexcept {
        while (socketData.socket != macroINVALID_SOCKET)
            co_yield AcceptOne(opt);
    }


#pragma endregion


#pragma region Close

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Abort(socketData);
    }

#pragma endregion

}
