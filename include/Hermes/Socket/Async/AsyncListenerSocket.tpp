#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <utility>
#include <stdexec/execution.hpp>

namespace Hermes {
#pragma region Initialization
    
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::InternalCreateAndListen(SocketData&& data, ListenOptions opt, int backlog) noexcept
        -> ListenerSocketResult {

        Network::Initialize();
        AsyncListenerSocket listener;
        listener.m_socketData = std::move(data);
        const auto result{ listener.m_acceptPolicy.Listen(listener.m_socketData, backlog, opt) };

        if (!result) {
            return std::unexpected(result.error());
        }
        return std::move(listener);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>),
            stdexec::set_error_t(ConnectionErrorEnum)
        >;
        SocketData data;
        ListenOptions opt;
        int backlog;

        template<class Receiver>
        struct OperationState;

        template<class Receiver>
        static void ExecuteStart(OperationState<Receiver>& self) {
            auto result{ AsyncListenerSocket::InternalCreateAndListen(std::move(self.data), std::move(self.opt), self.backlog) };
            if (!result) {
                stdexec::set_error(std::move(self.receiver), result.error());
            } else {
                stdexec::set_value(std::move(self.receiver), std::move(*result));
            }
        }

        template<class Receiver>
        struct OperationState {
            SocketData data;
            ListenOptions opt;
            int backlog;
            Receiver receiver;

            void start() & noexcept {
                ListenSender::ExecuteStart(*this);
            }
        };

        template<class Receiver>
        OperationState<Receiver> connect(Receiver r) && {
            return { std::move(data), std::move(opt), backlog, std::move(r) };
        }
    };

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncListenerSocket(AsyncListenerSocket&& other) noexcept
        : m_socketData  (std::move(other.m_socketData)),
          m_acceptPolicy(std::move(other.m_acceptPolicy)) { }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>&
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::operator=(AsyncListenerSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socketData   = std::move(other.m_socketData);
            m_acceptPolicy = std::move(other.m_acceptPolicy);
            other.m_socketData.socket = macroINVALID_SOCKET;
        }
        return *this;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::~AsyncListenerSocket() {
        Close();
    }

#pragma endregion


#pragma region Properties

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetEndpoint() noexcept -> EndpointType& {
        return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetEndpoint() const noexcept -> const EndpointType& {
        return m_socketData.endpoint;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetSocketData() noexcept -> SocketData& {
        return m_socketData;
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::GetSocketData() const noexcept -> const SocketData& {
        return m_socketData;
    }

#pragma endregion


#pragma region Listen Methods

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    typename AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData &&data, int backlog)
        requires std::default_initializable<ListenOptions> {
        return Listen(std::move(data), {}, backlog);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    typename AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::ListenSender
    AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Listen(SocketData&& data, ListenOptions opt, int backlog) {
        return ListenSender{ std::move(data), std::move(opt), backlog };
    }

#pragma endregion


#pragma region Accept Methods

template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne()
        requires std::default_initializable<AcceptOptions> {
        return AsyncAcceptOne(m_socketData, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne(AcceptOptions opt) {
        return AsyncAcceptOne(m_socketData, opt);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    template<class>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne(const SocketData& clientDataPrototype)
        requires std::default_initializable<AcceptOptions> {
        return AsyncAcceptOne(clientDataPrototype, {});
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    auto AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::AsyncAcceptOne(const SocketData& clientDataPrototype, AcceptOptions opt) {
        SocketData clientData{ clientDataPrototype.MakeChild() };

        return m_acceptPolicy.Accept(m_socketData, std::move(clientData), opt)
                | stdexec::then([](SocketData data) {
                      return ServerSocketType::FromAccepted(std::move(data));
                });
    }

#pragma endregion
    
    
#pragma region Close Methods
    
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    void AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Close() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_acceptPolicy.Close(m_socketData);
    }

    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
		requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    void AsyncListenerSocket<SocketData, AcceptPolicy, TransferPolicy>::Abort() noexcept {
        if (m_socketData.socket == macroINVALID_SOCKET) return;
        m_acceptPolicy.Abort(m_socketData);
    }
    
#pragma endregion
}

#endif