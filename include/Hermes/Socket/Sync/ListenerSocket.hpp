#pragma once
#include <Hermes/Socket/Sync/ServerSocket.hpp>

#include <generator>

namespace Hermes {

    //! @brief A blocking listener that accepts incoming connections.
    //!
    //! @details ListenerSocket owns the listening SocketData and uses AcceptPolicy
    //! for socket creation, binding, listening and per-connection acceptance. Any
    //! protocol-specific work required while accepting a connection, such as a TLS
    //! server handshake, is also delegated to AcceptPolicy. Each accepted connection
    //! is returned as a ServerSocket carrying the same SocketData, AcceptPolicy and
    //! TransferPolicy types.
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAcceptPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
    requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct ListenerSocket {
        //! @brief The server-socket type produced by this listener.
        using ServerSocketType   = ServerSocket<SocketData, AcceptPolicy, TransferPolicy>;
        //! @brief The transfer policy carried by every accepted server socket.
        using TransferPolicyType = TransferPolicy;
        //! @brief The options accepted by receive operations on an accepted socket.
        using RecvOptions        = typename TransferPolicy::RecvOptions;
        //! @brief The options accepted by send operations on an accepted socket.
        using SendOptions        = typename TransferPolicy::SendOptions;

        //! @brief The result type returned when creating the listener.
        using ListenerSockerResult = ConnectionResult<ListenerSocket>;
        //! @brief The result type returned when accepting a connection.
        using ServerSockertResult = ConnectionResult<ServerSocketType>;

        ListenerSocket(ListenerSocket&&) noexcept;
        ListenerSocket& operator=(ListenerSocket&&) noexcept;
        ~ListenerSocket();



        //! @brief Creates and starts the listener.
        //! @param data SocketData whose endpoint identifies the local address and port.
        //! @param backlog Maximum number of pending connections (default: SOMAXCONN).
        //! @return The listening socket on success, or the connection error on failure.
        //! @details The operation creates the socket, binds it to the endpoint and
        //! starts listening. The options used by this overload are default-constructed
        //! by AcceptPolicy.
        template<class = void>
        [[nodiscard]] static auto
            Listen(SocketData&& data, int backlog = SOMAXCONN) noexcept -> ListenerSockerResult
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        //! @brief Creates and starts the listener using explicit listen options.
        //! @param data SocketData whose endpoint identifies the local address and port.
        //! @param opt Options used while creating, binding or listening on the socket.
        //! @param backlog Maximum number of pending connections (default: SOMAXCONN).
        //! @return The listening socket on success, or the connection error on failure.
        [[nodiscard]] static auto
            Listen(SocketData&& data, AcceptPolicy::ListenOptions opt, int backlog = SOMAXCONN) noexcept -> ListenerSockerResult;

        template<class = void>
        [[nodiscard]] static auto
            ListenOne(SocketData&& data) noexcept -> ListenerSockerResult
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        //! @brief Creates and starts the listener using explicit listen options.
        //! @param data SocketData whose endpoint identifies the local address and port.
        //! @param opt Options used while creating, binding or listening on the socket.
        //! @return The listening socket on success, or the connection error on failure.
        [[nodiscard]] static auto
            ListenOne(SocketData&& data, AcceptPolicy::ListenOptions opt) noexcept -> ListenerSockerResult;



        //! @brief Blocks until one client connects and returns its ServerSocket.
        //! @return The accepted server socket on success, or the accept error on failure.
        //! @details Transfer options are applied later to operations on the returned
        //! ServerSocket. They are not accept options and therefore do not belong to
        //! this call.

        template<class = void>
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @brief Accepts one connection through the non-overloaded entry point.
        //! @return The accepted server socket on success, or the accept error on failure.
        //! @details This entry point is provided to disambiguate taking a function
        //! pointer when overloads are present.
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOneConnection() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @brief Accepts one connection using explicit accept options.
        //! @param opt Options used by AcceptPolicy while accepting the connection,
        //! including any protocol-specific handshake options.
        //! @return The accepted server socket on success, or the accept error on failure.
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne(AcceptPolicy::AcceptOptions opt) noexcept;

        //! @brief Accepts one connection using a SocketData prototype.
        //! @param clientDataPrototype The prototype used by MakeChild() to create the
        //! accepted socket's SocketData.
        //! @return The accepted server socket on success, or the accept error on failure.
        template<class = void>
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne(const SocketData& clientDataPrototype) noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @brief Accepts one connection using a SocketData prototype and explicit options.
        //! @param clientDataPrototype The prototype used by MakeChild() to create the
        //! accepted socket's SocketData.
        //! @param opt Options used by AcceptPolicy while accepting the connection.
        //! @return The accepted server socket on success, or the accept error on failure.
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne(const SocketData& clientDataPrototype, AcceptPolicy::AcceptOptions opt) noexcept;


        //! @brief Returns a generator that accepts connections until the listener closes.
        //! @details Each yielded value contains either an accepted ServerSocket or an
        //! accept error. Transfer options are applied later to operations on each
        //! yielded ServerSocket; they are not options for AcceptAll().
        //!
        //! The generator runs until Close() is called or a non-recoverable error occurs.
        //! @example
        //!   for (auto&& result : listener.AcceptAll()) {
        //!       if (!result) { /* log error */ continue; }
        //!       handle(std::move(*result));
        //!   }
        template<class = void>
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AcceptAll
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll(AcceptPolicy::AcceptOptions opt) noexcept;

        //! @brief Returns a generator that accepts connections using a SocketData prototype.
        //! @param clientDataPrototype The prototype used by MakeChild() to create each
        //! accepted socket's SocketData.
        //! @return A generator yielding accepted server sockets or accept errors.
        template<class = void>
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll(const SocketData& clientDataPrototype) noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @brief Accepts connections using a SocketData prototype and explicit options.
        //! @param clientDataPrototype The prototype used by MakeChild() to create each
        //! accepted socket's SocketData.
        //! @param opt Options used by AcceptPolicy for every accepted connection.
        //! @return A generator yielding accepted server sockets or accept errors.
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll(const SocketData& clientDataPrototype, AcceptPolicy::AcceptOptions opt) noexcept;



        //! @brief Performs the protocol-level graceful shutdown and closes the listener.
        void Close() noexcept;
        //! @brief Immediately closes the listening socket without a graceful shutdown.
        void Abort() noexcept;

    private:
        ListenerSocket() = default;

        SocketData   m_socketData{};
        AcceptPolicy m_acceptPolicy{};
    };

    using RawTcpListener = ListenerSocket<>;
    using RawTlsListener = ListenerSocket<TlsSocketData<>, TlsAcceptPolicy<>, TlsTransferPolicy<>>;
}

#include <Hermes/Socket/Sync/ListenerSocket.tpp>