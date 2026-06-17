#pragma once
#include <Hermes/Socket/Sync/ServerSocket.hpp>

#include <generator>

namespace Hermes {

    //! @brief Blocking listener socket. Owns the listening SocketFd handle and produces
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
    requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct ListenerSocket {
        using ServerSocketType = ServerSocket<SocketData, AcceptPolicy, TransferPolicy>;

        using ListenerSockerResult = ConnectionResult<ListenerSocket>;
        using ServerSockertResult = ConnectionResult<ServerSocketType>;

        ListenerSocket(ListenerSocket&&) noexcept;
        ListenerSocket& operator=(ListenerSocket&&) noexcept;
        ~ListenerSocket();



        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketData whose endpoint field identifies the address/port to bind.
        //! @param backlog  Maximum pending-connection queue length (default: SOMAXCONN).
        template<class = void>
        [[nodiscard]] static auto
            Listen(SocketData&& data, int backlog = SOMAXCONN) noexcept -> ListenerSockerResult
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        //! @copydoc Listen
        [[nodiscard]] static auto
            Listen(SocketData&& data, AcceptPolicy::ListenOptions opt, int backlog = SOMAXCONN) noexcept -> ListenerSockerResult;

        template<class = void>
        [[nodiscard]] static auto
            ListenOne(SocketData&& data) noexcept -> ListenerSockerResult
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketData whose endpoint field identifies the address/port to bind.
        [[nodiscard]] static auto
            ListenOne(SocketData&& data, AcceptPolicy::ListenOptions opt) noexcept -> ListenerSockerResult;



        //! @brief Blocks until one client connects, then returns a ready ServerSocket.
        template<class = void>
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AcceptOne
        //!
        //! Used to disambiguate pointer cast for functions with overloads.
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