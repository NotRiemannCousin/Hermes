#pragma once

#include <utility>
#include <stdexec/execution.hpp>

namespace Hermes {

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    ConnectionResult<AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::InternalCreateAndListen(
        SocketData&& data, typename AcceptPolicy::ListenOptions opt, int backlog) noexcept {

        Network::Initialize();
        AsyncListenerSocket listener;
        listener.socketData = std::move(data);
        const auto result = listener.acceptPolicy.Listen(listener.socketData, backlog, opt);

        if (!result) {
            return std::unexpected(result.error());
        }
        return std::move(listener);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    struct AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>),
            stdexec::set_error_t(ConnectionErrorEnum)
        >;
        SocketData data;
        typename AcceptPolicy::ListenOptions opt;
        int backlog;

        template<class Receiver>
        struct OperationState;

        template<class Receiver>
        static void ExecuteStart(OperationState<Receiver>& self) {
            auto result = AsyncListenerSocket::InternalCreateAndListen(std::move(self.data), std::move(self.opt), self.backlog);
            if (!result) {
                stdexec::set_error(std::move(self.receiver), result.error());
            } else {
                stdexec::set_value(std::move(self.receiver), std::move(*result));
            }
        }

        template<class Receiver>
        struct OperationState {
            SocketData data;
            typename AcceptPolicy::ListenOptions opt;
            int backlog;
            Receiver receiver;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                ListenSender::ExecuteStart(self);
            }
        };

        template<class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, ListenSender&& self, Receiver r) {
            return { std::move(self.data), std::move(self.opt), self.backlog, std::move(r) };
        }
    };

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
    typename AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data)
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return ListenOne(std::move(data), {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    typename AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenOne(SocketData &&data, AcceptPolicy::ListenOptions opt) {
        return Listen(std::move(data), std::move(opt), 1);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    typename AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData &&data, int backlog)
        requires std::default_initializable<typename AcceptPolicy::ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    typename AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data, AcceptPolicy::ListenOptions opt, int backlog) {
        return ListenSender{ std::move(data), std::move(opt), backlog };
    }


    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    template<class>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne()
        requires std::default_initializable<typename AcceptPolicy::AcceptOptions> {
        return AsyncAcceptOne({});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne(AcceptPolicy::AcceptOptions opt) {

        return acceptPolicy.AsyncAccept(socketData, opt)
                | stdexec::then([](SocketData data) {
                      return ServerSocketType::FromAccepted(std::move(data));
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