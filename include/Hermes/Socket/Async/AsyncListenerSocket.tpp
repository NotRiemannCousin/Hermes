#pragma once

#include <utility>
#include <stdexec/execution.hpp>

namespace Hermes {

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::AsyncListenerSocket(AsyncListenerSocket&& other) noexcept
        : socketData  (std::move(other.socketData)),
          acceptPolicy(std::move(other.acceptPolicy)) { }


    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>&
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::operator=(AsyncListenerSocket&& other) noexcept {
        if (this != &other) {
            Close();
            socketData   = std::move(other.socketData);
            acceptPolicy = std::move(other.acceptPolicy);
            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::~AsyncListenerSocket() {
        Close();
    }

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    template<class>
    ConnectionResult<AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::ListenOne(SocketDataType &&data) noexcept
        requires std::default_initializable<typename AsyncAcceptPolicyType<SocketDataType>::ListenOptions> {
        return ListenOne(std::move(data), {});
    }

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    ConnectionResult<AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::ListenOne(SocketDataType &&data, AsyncAcceptPolicyType<SocketDataType>::ListenOptions opt) noexcept {
        return Listen(std::move(data), opt);
    }

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    template<class>
    ConnectionResult<AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::Listen(SocketDataType &&data, int backlog) noexcept
        requires std::default_initializable<typename AsyncAcceptPolicyType<SocketDataType>::ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    ConnectionResult<AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>>
    AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::Listen(SocketDataType&& data, AsyncAcceptPolicyType<SocketDataType>::ListenOptions opt, int backlog) noexcept {
        Network::Initialize();

        AsyncListenerSocket listener;
        listener.socketData = std::move(data);

        const auto result{ listener.acceptPolicy.Listen(listener.socketData, backlog, opt) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }


    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    template<class>
    auto AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::AsyncAcceptOne() noexcept
        requires std::default_initializable<typename AsyncAcceptPolicyType<SocketDataType>::AcceptOptions> {
        return AsyncAcceptOne({});
    }

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    auto AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::AsyncAcceptOne(AsyncAcceptPolicyType<SocketDataType>::AcceptOptions opt) noexcept {

        // We inject the newly created child socketData into the sender's operation state,
        // so it safely lives while AsyncAccept completes.
        return stdexec::just(socketData.MakeChild())
             | stdexec::let_value([this, opt = std::move(opt)](SocketDataType& clientData) mutable {
                   return acceptPolicy.AsyncAccept(socketData, clientData, opt)
                        | stdexec::then([&clientData]() mutable {
                              return ServerSocketType::FromAccepted(std::move(clientData));
                          });
               });
    }


    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    void AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketDataType, template <class> class AsyncAcceptPolicyType, template <class> class AsyncTransferPolicyType>
		requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    void AsyncListenerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Abort(socketData);
    }

}