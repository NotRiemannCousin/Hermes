#pragma once
#include <Hermes/Socket/ServerSocket.hpp>

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
        SocketDataConcept SocketData             = DefaultSocketData<>,
        template <class> class AcceptPolicy      = DefaultAcceptPolicy,
        template <class> class TransferPolicy    = DefaultTransferPolicy>
    requires AcceptPolicyConcept<AcceptPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    struct ListenerSocket {
        using ServerSocketType = ServerSocket<SocketData, AcceptPolicy, TransferPolicy>;


        ListenerSocket(ListenerSocket&&) noexcept;
        ListenerSocket& operator=(ListenerSocket&&) noexcept;
        ~ListenerSocket();

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketData whose endpoint field identifies the address/port to bind.
        //! @param backlog  Maximum pending-connection queue length (default: SOMAXCONN).
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            Listen(SocketData&& data, int backlog = SOMAXCONN) noexcept;

        //! @brief Creates the listener: socket() + bind() + listen().
        //! @param data  SocketData whose endpoint field identifies the address/port to bind.
        [[nodiscard]] static ConnectionResult<ListenerSocket>
            ListenOne(SocketData&& data) noexcept;

        //! @brief Blocks until one client connects, then returns a ready ServerSocket.
        [[nodiscard]] ConnectionResult<ServerSocketType> AcceptOne() noexcept;

        //! @brief Coroutine generator that yields a ServerSocket for each accepted client.
        //! Runs until Close() is called or a non-recoverable error occurs.
        //! @example
        //!   for (auto&& result : listener.AcceptAll()) {
        //!       if (!result) { /* log error */ continue; }
        //!       handle(std::move(*result));
        //!   }
        [[nodiscard]] std::generator<ConnectionResult<ServerSocketType>>
            AcceptAll() noexcept;

        void Close() noexcept;
        void Abort() noexcept;

    private:
        ListenerSocket() = default;

        SocketData               socketData{};
        AcceptPolicy<SocketData> acceptPolicy{};
    };

    using RawTcpListener = ListenerSocket<>;
    using RawTlsListener = ListenerSocket<TlsSocketData<>, TlsAcceptPolicy, TlsTransferPolicy>;

}

#include <Hermes/Socket/ListenerSocket.tpp>