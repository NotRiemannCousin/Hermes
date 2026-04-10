#pragma once

#include <utility>

namespace Hermes {

    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenerSocket(ListenerSocket&& other) noexcept
        : socketData  (std::move(other.socketData)),
          acceptPolicy(std::move(other.acceptPolicy)) { }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
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


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::~ListenerSocket() {
        Close();
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data, int backlog) noexcept {
        ListenerSocket listener;
        listener.socketData = std::move(data);

        const auto result{ listener.acceptPolicy.Listen(listener.socketData, backlog) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }

    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
        requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>> ListenerSocket<SocketData, AcceptPolicy,
    TransferPolicy>::ListenOne(SocketData &&data) noexcept {
        return Listen(std::move(data));
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptOne() noexcept {
        SocketData clientData{ socketData.MakeChild() };

        const auto result{ acceptPolicy.Accept(socketData, clientData) };
        if (!result) return std::unexpected{ result.error() };

        return ServerSocketType::FromAccepted(std::move(clientData));
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    std::generator<ConnectionResult<typename ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ServerSocketType>>
    ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AcceptAll() noexcept {
        while (socketData.socket != macroINVALID_SOCKET)
            co_yield AcceptOne();
    }


    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, template <class> class AcceptPolicy, template <class> class TransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    void ListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Abort(socketData);
    }

}
