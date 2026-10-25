#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/AsyncServerSocket.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAsyncAcceptPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct AsyncListenerSocket;
#endif

    //! @brief An asynchronous listener that accepts incoming connections.
    //!
    //! AsyncListenerSocket owns the listening SocketData and uses AcceptPolicy for socket creation, binding, listening
    //! and per-connection acceptance. Unlike the synchronous ListenerSocket, the listen and accept operations return
    //! senders that integrate with a stdexec execution graph instead of blocking the caller. Any protocol-specific
    //! work required while accepting a connection, such as a protocol handshake, is delegated to AcceptPolicy. Each
    //! accepted connection is returned as an AsyncServerSocket carrying the same SocketData, AcceptPolicy and
    //! TransferPolicy types.
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct AsyncListenerSocket final {
#pragma region Aliases

        //! @brief A sender that carries the result of AsyncListenerSocket::Listen().
        struct ListenSender;

        //! @brief The server-socket type produced by this listener.
        using ServerSocketType = AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>;

        //! @brief The result type returned when creating the listener.
        using ListenerSocketResult = ConnectionResult<AsyncListenerSocket>;
        //! @brief The result type returned when accepting a connection.
        using ServerSocketResult = ConnectionResult<ServerSocketType>;


        //! @brief The data policy used by this socket. Stores the info used by other policies.
        using DataType           = SocketData;
        //! @brief The accept policy used by this socket. Used to listen/close the socket.
        using AcceptPolicyType   = AcceptPolicy;
        //! @brief The transfer policy used by this socket. Used when transferring data.
        using TransferPolicyType = TransferPolicy;

        //! @brief The endpoint type used by SocketData.
        using EndpointType = SocketData::EndpointType;


        //! @brief The options accepted by listening operations.
        using ListenOptions = AcceptPolicy::ListenOptions;
        //! @brief The options accepted by accept operations.
        using AcceptOptions = AcceptPolicy::AcceptOptions;

#pragma endregion

        AsyncListenerSocket(AsyncListenerSocket&&) noexcept;
        AsyncListenerSocket& operator=(AsyncListenerSocket&&) noexcept;
        ~AsyncListenerSocket();

#pragma region Properties

        //! @brief Returns a reference to the underlying endpoint (unsafe).
        [[nodiscard]] auto GetEndpoint()       noexcept ->       EndpointType&;
        //! @brief Returns a const reference to the underlying endpoint.
        [[nodiscard]] auto GetEndpoint() const noexcept -> const EndpointType&;

        //! @brief Returns a reference to the underlying SocketData (unsafe).
        [[nodiscard]] auto GetSocketData()       noexcept ->       SocketData&;
        //! @brief Returns a const reference to the underlying SocketData.
        [[nodiscard]] auto GetSocketData() const noexcept -> const SocketData&;

#pragma endregion


#pragma region Listen Methods

        //! @brief Creates and starts the listener.
        //! @details The returned ListenSender performs the socket creation, bind and listen syscalls synchronously
        //! when started. It is wrapped in a sender so it can participate in a stdexec execution graph, but the work
        //! itself does not yield to the scheduler because listening is a cheap local operation.
        //! @param data SocketData whose endpoint identifies the local address and port.
        //! @param backlog Maximum number of pending connections (default: SOMAXCONN).
        //! @return A ListenSender whose value completion carries an AsyncListenerSocket and whose error completion
        //! carries a ConnectionErrorEnum.
        template<class = void>
        [[nodiscard]] static ListenSender Listen(SocketData&& data, int backlog = SOMAXCONN)
            requires std::default_initializable<ListenOptions>;

        //! @brief Creates and starts the listener using explicit listen options.
        //! @copydetails Listen
        //! @param opt Options used while creating, binding or listening on the socket.
        //! @return A ListenSender whose value completion carries an AsyncListenerSocket and whose error completion
        //! carries a ConnectionErrorEnum.
        [[nodiscard]] static ListenSender Listen(SocketData&& data, ListenOptions opt, int backlog = SOMAXCONN);

#pragma endregion


#pragma region Accept Methods

        //! @brief Asynchronously accepts one incoming connection.
        //! @details The returned sender completes when a client has connected and any protocol handshake required by
        //! AcceptPolicy has finished. For each accepted connection a new SocketData is created from the listener's own
        //! SocketData via MakeChild(), then wrapped into an AsyncServerSocket.
        //! @return A sender whose value completion carries an AsyncServerSocket, whose error completion
        //! carries a ConnectionErrorEnum, and whose stopped completion signals cancellation.
        template<class = void>
        [[nodiscard]] auto AsyncAcceptOne()
            requires std::default_initializable<AcceptOptions>;

        //! @copydoc AsyncAcceptOne
        [[nodiscard]] auto AsyncAcceptOne(AcceptOptions opt);

        //! @brief Asynchronously accepts one connection using a SocketData prototype.
        //! @copydetails AsyncAcceptOne
        //! @param clientDataPrototype The prototype used to instantiate the client's SocketData via MakeChild().
        template<class = void>
        [[nodiscard]] auto AsyncAcceptOne(const SocketData& clientDataPrototype)
            requires std::default_initializable<AcceptOptions>;

        //! @brief Asynchronously accepts one connection using a prototype and explicit options.
        //! @copydetails AsyncAcceptOne
        //! @param clientDataPrototype The prototype used to instantiate the client's SocketData via MakeChild().
        //! @param opt Options used by AcceptPolicy while accepting the connection.
        [[nodiscard]] auto AsyncAcceptOne(const SocketData& clientDataPrototype, AcceptOptions opt);

#pragma endregion


#pragma region Close Methods

        //! @brief Synchronously destroys the underlying handle and cancels pending I/O.
        //! @details This is a synchronous operation that immediately releases the OS socket handle. Any pending
        //! asynchronous operations that reference this handle will complete with an error.
        void Close() noexcept;

        //! @brief Terminates the connection abruptly.
        //! @details Sends a TCP RST or equivalent and closes the socket without a protocol-level graceful shutdown.
        //! Use this when graceful shutdown is not possible or not desired.
        void Abort() noexcept;

#pragma endregion

    private:
        friend struct ListenSender;
        AsyncListenerSocket() = default;

        //! @brief Creates the listening socket and performs the blocking listen syscall.
        //! @details Used internally by ListenSender to carry out the synchronous socket creation, bind and listen
        //! steps. Returns the fully initialized listener on success or a ConnectionErrorEnum on failure.
        [[nodiscard]] static auto InternalCreateAndListen(SocketData&& data, ListenOptions opt, int backlog) noexcept -> ListenerSocketResult;

        SocketData   m_socketData{};
        AcceptPolicy m_acceptPolicy{};
    };


    template<class T>
    constexpr bool IsAsyncListenerSocket{ false };
    template<class... T>
    constexpr bool IsAsyncListenerSocket<AsyncListenerSocket<T...>>{ true };

    template<class T>
    concept AsyncListenerSocketConcept = IsAsyncListenerSocket<T>;


#if HERMES_ENABLE_NATIVE_SCHEDULER
    //! @brief Alias to the raw TCP listener async socket.
    using RawTcpAsyncListener = AsyncListenerSocket<>;
    extern template struct AsyncListenerSocket<DefaultSocketData<>, DefaultAsyncAcceptPolicy<>, DefaultAsyncTransferPolicy<>>;
#if HERMES_ENABLE_TLS
    //! @brief Alias to the raw TLS listener async socket.
    using RawTlsAsyncListener = AsyncListenerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
    extern template struct AsyncListenerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
#endif
#endif

}

#include <Hermes/Socket/Async/AsyncListenerSocket.tpp>

#endif