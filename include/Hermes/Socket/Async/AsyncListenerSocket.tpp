#pragma once

#include <utility>
#include <stdexec/execution.hpp>

namespace Hermes {

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncListenerSocket(AsyncListenerSocket&& other) noexcept
        : socketData  (std::move(other.socketData)),
          acceptPolicy(std::move(other.acceptPolicy)) { }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(AsyncListenerSocket&& other) noexcept {
        if (this != &other) {
            Close();
            socketData   = std::move(other.socketData);
            acceptPolicy = std::move(other.acceptPolicy);
            other.socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::~AsyncListenerSocket() {
        Close();
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    ConnectionResult<AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data) noexcept
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return ListenOne(std::move(data), {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data, AcceptPolicy::ListenOptions opt) noexcept {
        return Listen(std::move(data), opt);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    ConnectionResult<AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData &&data, int backlog) noexcept
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data, AcceptPolicy::ListenOptions opt, int backlog) noexcept {
        Network::Initialize();

        AsyncListenerSocket listener;
        listener.socketData = std::move(data);

        const auto result{ listener.acceptPolicy.Listen(listener.socketData, backlog, opt) };
        if (!result) return std::unexpected{ result.error() };

        return listener;
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne() noexcept
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AsyncAcceptOne({});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne(AcceptPolicy::AcceptOptions opt) noexcept {

        // We inject the newly created child socketData into the sender's operation state,
        // so it safely lives while AsyncAccept completes.
        return stdexec::just(socketData.MakeChild())
             | stdexec::let_value([this, opt = std::move(opt)](SocketData& clientData) mutable {
                   // The accept policy sender completes with set_value_t() on success.
                   return acceptPolicy.AsyncAccept(socketData, clientData, opt)
                        // On success, transform the void result into the final ServerSocket object.
                        | stdexec::then([&clientData]() mutable {
                              return ServerSocketType::FromAccepted(std::move(clientData));
                          });
               });
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Close(socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    void AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (socketData.socket == macroINVALID_SOCKET) return;
        acceptPolicy.Abort(socketData);
    }
}