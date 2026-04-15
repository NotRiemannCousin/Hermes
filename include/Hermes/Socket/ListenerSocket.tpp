#pragma once

#include <utility>

namespace Hermes {

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::ListenerSocket(ListenerSocket&& other) noexcept
        : socketData  (std::move(other.socketData)),
          acceptPolicy(std::move(other.acceptPolicy)) { }


    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>&
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::operator=(ListenerSocket&& other) noexcept {
        if (this != &other) {
            Close();

            socketData   = std::move(other.socketData);
            acceptPolicy = std::move(other.acceptPolicy);

            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::~ListenerSocket() {
        Close();
    }

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    template<class>
    ConnectionResult<ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>> ListenerSocket<SocketDataType
    , AcceptPolicyType, TransferPolicyType>::ListenOne(SocketDataType &&data) noexcept
        requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::ListenOptions> {
        return ListenOne(std::move(data), {});
    }

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
        requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    template<class>
    ConnectionResult<typename ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::ServerSocketType>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::AcceptOne() noexcept
        requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::AcceptOptions> {
        return AcceptOne({});
    }

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class
        TransferPolicyType>
        requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType,
        SocketDataType>
    ConnectionResult<typename ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::ServerSocketType>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::AcceptOneConnection() noexcept requires std::
        default_initializable<typename AcceptPolicyType<SocketDataType>::AcceptOptions> {
        return AcceptOne();
    }

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    ConnectionResult<ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>> ListenerSocket<SocketDataType, AcceptPolicyType,
    TransferPolicyType>::ListenOne(SocketDataType &&data, AcceptPolicyType<SocketDataType>::ListenOptions opt) noexcept {
        return Listen(std::move(data), opt);
    }
    
    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    template<class>
    ConnectionResult<ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>> ListenerSocket<SocketDataType
    , AcceptPolicyType, TransferPolicyType>::Listen(SocketDataType &&data, int backlog) noexcept
        requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }
    
    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    ConnectionResult<ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::Listen(SocketDataType&& data, AcceptPolicyType<SocketDataType>::ListenOptions opt, int backlog) noexcept {
        Network::Initialize();

        ListenerSocket listener;
        listener.socketData = std::move(data);

        const auto result{ listener.acceptPolicy.Listen(listener.socketData, backlog, opt) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }


    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    ConnectionResult<typename ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::ServerSocketType>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::AcceptOne(AcceptPolicyType<SocketDataType>::AcceptOptions opt) noexcept {
        SocketDataType clientData{ socketData.MakeChild() };

        const auto result{ acceptPolicy.Accept(socketData, clientData, opt) };
        if (!result) return std::unexpected{ result.error() };

        return ServerSocketType::FromAccepted(std::move(clientData));
    }

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
        requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    template<class>
    std::generator<ConnectionResult<typename ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::
    ServerSocketType>> ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::AcceptAll() noexcept
        requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::AcceptOptions> {
        co_yield std::ranges::elements_of(AcceptAll({}));
    }


    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    std::generator<ConnectionResult<typename ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::ServerSocketType>>
    ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::AcceptAll(AcceptPolicyType<SocketDataType>::AcceptOptions opt) noexcept {
        while (socketData.socket != macroINVALID_SOCKET)
            co_yield AcceptOne(opt);
    }


    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    void ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketDataType, template <class> class AcceptPolicyType, template <class> class TransferPolicyType>
		requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    void ListenerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;

        acceptPolicy.Abort(socketData);
    }

}
