#pragma once

#include <utility>

namespace Hermes {

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenerSocket(ListenerSocket&& other) noexcept
        : socketData  (std::move(other.socketData)),
          acceptPolicy(std::move(other.acceptPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
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
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::~ListenerSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    ConnectionResult<ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>> ListenerSocket<SocketData
    , AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data) noexcept
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return ListenOne(std::move(data), {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne() noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptOne({});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class
        TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy,
        SocketData>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOneConnection() noexcept requires std::
        default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AcceptOne();
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>> ListenerSocket<SocketData, AcceptPolicy,
    TransferPolicy>::ListenOne(SocketData &&data, typename AcceptPolicy::ListenOptions opt) noexcept {
        return Listen(std::move(data), opt);
    }
    
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    ConnectionResult<ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>> ListenerSocket<SocketData
    , AcceptPolicy, TransferPolicy>::Listen(SocketData &&data, int backlog) noexcept
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }
    
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data, typename AcceptPolicy::ListenOptions opt, int backlog) noexcept {
        Network::Initialize();

        ListenerSocket listener;
        listener.socketData = std::move(data);

        const auto result{ listener.acceptPolicy.Listen(listener.socketData, backlog, opt) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne(typename AcceptPolicy::AcceptOptions opt) noexcept {
        SocketData clientData{ socketData.MakeChild() };

        const auto result{ acceptPolicy.Accept(socketData, clientData, opt) };
        if (!result) return std::unexpected{ result.error() };

        return ServerSocketType::FromAccepted(std::move(clientData));
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::
    ServerSocketType>> ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll() noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        co_yield std::ranges::elements_of(AcceptAll({}));
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll(typename AcceptPolicy::AcceptOptions opt) noexcept {
        while (socketData.socket != macroINVALID_SOCKET)
            co_yield AcceptOne(opt);
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Abort(socketData);
    }

}
