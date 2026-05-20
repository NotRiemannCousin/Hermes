#pragma once
#include <Hermes/Socket/Sync/ServerSocket.hpp>

#include <generator>

namespace Hermes {

    //! @brief Blocking listener socket. Owns the listening SOCKET handle and produces
    //! ServerSocket instances via AcceptOne() / AcceptAll().
    //!
    //! @details The listening socket lives inside socketData.socket. AcceptPolicy drives
    //! both the bind/listen phase (Listen()) and the per-connection accept phase
    //! (AcceptOne()), including any protocol-level handshake (e.g. TLS ServerHandshake).
    //! TransferPolicy is forwarded as-is to every ServerSocket produced.
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAcceptPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    struct ListenerSocket {
        using ServerSocketType = ServerSocket<SocketData, AcceptPolicy, TransferPolicy>;


        ListenerSocket(ListenerSocket&&) noexcept;
        ListenerSocket& operator=(ListenerSocket&&) noexcept;
        ~ListenerSocket();

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketData whose endpoint field identifies the address/port to bind.
        //! @param backlog  Maximum pending-connection queue length (default: SOMAXCONN).
        template<class = void>
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            Listen(SocketData&& data, int backlog = SOMAXCONN) noexcept
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        //! @copydoc Listen
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            Listen(SocketData&& data, AcceptPolicy::ListenOptions opt, int backlog = SOMAXCONN) noexcept;

        template<class = void>
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            ListenOne(SocketData&& data) noexcept
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketData whose endpoint field identifies the address/port to bind.
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            ListenOne(SocketData&& data, AcceptPolicy::ListenOptions opt) noexcept;

        //! @brief Blocks until one client connects, then returns a ready ServerSocket.
        template<class = void>
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AcceptOne
        //!
        //! Used to desaguambigate pointer cast for functions with overloads.
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOneConnection() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AcceptOne
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne(AcceptPolicy::AcceptOptions opt) noexcept;


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
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AcceptAll
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll(AcceptPolicy::AcceptOptions opt) noexcept;

        void Close() noexcept;
        void Abort() noexcept;

    private:
        ListenerSocket() = default;

        SocketData   socketData{};
        AcceptPolicy acceptPolicy{};
    };

    using RawTcpListener = ListenerSocket<>;
    using RawTlsListener = ListenerSocket<TlsSocketData<>, TlsAcceptPolicy<>, TlsTransferPolicy<>>;

}

#include <Hermes/Socket/Sync/ListenerSocket.tpp>