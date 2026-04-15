#pragma once
#include <Hermes/Socket/ServerSocket.hpp>

#include <generator>

namespace Hermes {

    //! @brief Blocking listener socket. Owns the listening SOCKET handle and produces
    //! ServerSocket instances via AcceptOne() / AcceptAll().
    //!
    //! @details The listening socket lives inside socketData.socket. AcceptPolicyType drives
    //! both the bind/listen phase (Listen()) and the per-connection accept phase
    //! (AcceptOne()), including any protocol-level handshake (e.g. TLS ServerHandshake).
    //! TransferPolicyType is forwarded as-is to every ServerSocket produced.
    template<
        SocketDataConcept SocketDataType             = DefaultSocketData<>,
        template <class> class AcceptPolicyType      = DefaultAcceptPolicy,
        template <class> class TransferPolicyType    = DefaultTransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicyType, SocketDataType> && TransferPolicyConcept<TransferPolicyType, SocketDataType>
    struct ListenerSocket {
        using ServerSocketType = ServerSocket<SocketDataType, AcceptPolicyType, TransferPolicyType>;


        ListenerSocket(ListenerSocket&&) noexcept;
        ListenerSocket& operator=(ListenerSocket&&) noexcept;
        ~ListenerSocket();

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketDataType whose endpoint field identifies the address/port to bind.
        //! @param backlog  Maximum pending-connection queue length (default: SOMAXCONN).
        template<class = void>
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            Listen(SocketDataType&& data, int backlog = SOMAXCONN) noexcept
            requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::ListenOptions>;

        //! @copydoc Listen
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            Listen(SocketDataType&& data, AcceptPolicyType<SocketDataType>::ListenOptions opt, int backlog = SOMAXCONN) noexcept;

        template<class = void>
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            ListenOne(SocketDataType&& data) noexcept
            requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::ListenOptions>;

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketDataType whose endpoint field identifies the address/port to bind.
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            ListenOne(SocketDataType&& data, AcceptPolicyType<SocketDataType>::ListenOptions opt) noexcept;

        //! @brief Blocks until one client connects, then returns a ready ServerSocket.
        template<class = void>
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne() noexcept
            requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::AcceptOptions>;

        //! @copydoc AcceptOne
        //!
        //! Used to desaguambigate pointer cast for functions with overloads.
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOneConnection() noexcept
            requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::AcceptOptions>;

        //! @copydoc AcceptOne
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne(AcceptPolicyType<SocketDataType>::AcceptOptions opt) noexcept;


        //! @brief Coroutine generator that yields a ServerSocket for each accepted client.
        //! Runs until Close() is called or a non-recoverable error occurs.
        //! @example
        //!   for (auto&& result : listener.AcceptAll()) {
        //!       if (!result) { /* log error */ continue; }
        //!       handle(std::move(*result));
        //!   }
        template<class = void>
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll() noexcept
            requires std::default_initializable<typename AcceptPolicyType<SocketDataType>::AcceptOptions>;

        //! @copydoc AcceptAll
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll(AcceptPolicyType<SocketDataType>::AcceptOptions opt) noexcept;

        void Close() noexcept;
        void Abort() noexcept;

    private:
        ListenerSocket() = default;

        SocketDataType                   socketData{};
        AcceptPolicyType<SocketDataType> acceptPolicy{};
    };

    using RawTcpListener = ListenerSocket<>;
    using RawTlsListener = ListenerSocket<TlsSocketData<>, TlsAcceptPolicy, TlsTransferPolicy>;

}

#include <Hermes/Socket/ListenerSocket.tpp>