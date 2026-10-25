#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAsyncAcceptPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct AsyncServerSocket;
#endif

    //! @brief An asynchronous socket accepted by an AsyncListenerSocket.
    //!
    //! AsyncServerSocket is produced by AsyncListenerSocket::AsyncAcceptOne() after a peer connection has been
    //! accepted. It provides the same transfer operations as AsyncClientSocket, but retains AcceptPolicy because the
    //! accepted side is responsible for the protocol-level teardown associated with the accept policy.
    //! AsyncServerSocket never establishes a connection with Connect(), the connection already exists when the object
    //! is created via FromAccepted().
    //!
    //! Async sockets don't have Recv/Send options because the cancelation behavior is created using STDEXEC's
    //! cancelation. They don't have SendAndLift either because it doesn't compose well with senders.
    template<SocketDataConcept SocketData, class AcceptPolicy, class TransferPolicy>
        requires AsyncServerSocketPoliciesConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct AsyncServerSocket final {
#pragma region Aliases

        //! @brief The data policy used by this socket. Stores the info used by other policies.
        using DataType           = SocketData;
        //! @brief The accept policy used by this socket. Used to open/close the socket.
        using AcceptPolicyType   = AcceptPolicy;
        //! @brief The transfer policy used by this socket. Used when transferring data.
        using TransferPolicyType = TransferPolicy;


        //! @brief The endpoint type used by SocketData.
        using EndpointType = SocketData::EndpointType;

#pragma endregion

        AsyncServerSocket(AsyncServerSocket&&) noexcept;
        AsyncServerSocket& operator=(AsyncServerSocket&&) noexcept;
        ~AsyncServerSocket();

        //! @brief Wraps an already-accepted SocketData into a server socket.
        //! @details This factory is used by AsyncListenerSocket after a successful accept operation. The returned
        //! object owns the accepted socket.
        //! @param data SocketData containing the accepted socket and its endpoint.
        //! @return A new AsyncServerSocket instance owning the accepted connection.
        [[nodiscard]] static AsyncServerSocket FromAccepted(SocketData&& data) noexcept;


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


#pragma region Send Methods

        //! @brief Asynchronously writes data to the socket.
        //! @details The returned sender initiates the transfer when started. On completion it yields the number of
        //! bytes written. A successful call may send fewer bytes than the range contains, exactly as the underlying
        //! operating system delivers. To send the full range, invoke Send again with the remaining portion.
        //! @param data A contiguous range of bytes to send.
        //! @return A sender whose value completion carries the number of bytes sent, whose error completion carries a
        //! TransferError (containing the partial byte count and the underlying ConnectionErrorEnum), and whose stopped
        //! completion signals cancellation.
        template<ContiguousByteRange R>
        [[nodiscard]] auto Send(R&& data) noexcept;

#pragma endregion


#pragma region Recv Methods

        //! @brief Asynchronously reads data from the socket.
        //! @details The returned sender initiates the read when started. In RecvModeEnum::All mode the sender
        //! completes only after the buffer is fully filled or an end-of-stream is encountered. In RecvModeEnum::Any
        //! mode it completes after the first available data. A successful call may receive fewer bytes than the buffer
        //! can hold, even in RecvModeEnum::All mode, if the peer closes the connection before the buffer is full.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param mode Whether the operation receives any available data or fills the range before completing.
        //! @return A sender whose value completion carries the number of bytes received, whose error completion
        //! carries a TransferError (containing the partial byte count and the underlying ConnectionErrorEnum), and
        //! whose stopped completion signals cancellation.
        template<WritableContiguousByteRange R>
        [[nodiscard]] auto Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept;

#pragma endregion


#pragma region Close Methods

        //! @brief Initiates asynchronous protocol-level shutdown.
        //! @details The returned sender completes with no value on success or with a ConnectionErrorEnum if the
        //! shutdown fails. The underlying socket is not closed by this call. Use Close() afterwards to release the
        //! handle.
        //! @return A sender whose value completion completes with no arguments, whose error completion carries a
        //! ConnectionErrorEnum, and whose stopped completion signals cancellation.
        [[nodiscard]] auto Shutdown() noexcept;

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
        AsyncServerSocket() = default;

        SocketData     m_socketData{};
        AcceptPolicy   m_acceptPolicy{};
        TransferPolicy m_transferPolicy{};
    };


    template<class T>
    constexpr bool IsAsyncServerSocket{ false };
    template<class... T>
    constexpr bool IsAsyncServerSocket<AsyncServerSocket<T...>>{ true };

    template<class T>
    concept AsyncServerSocketConcept = IsAsyncServerSocket<T>;


#if HERMES_ENABLE_NATIVE_SCHEDULER
    //! @brief Alias to the raw TCP server async socket.
    using RawTcpAsyncServer = AsyncServerSocket<>;
    extern template struct AsyncServerSocket<DefaultSocketData<>, DefaultAsyncAcceptPolicy<>, DefaultAsyncTransferPolicy<>>;
#if HERMES_ENABLE_TLS
    //! @brief Alias to the raw TLS server async socket.
    using RawTlsAsyncServer = AsyncServerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
    extern template struct AsyncServerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
#endif
#endif
}

#include <Hermes/Socket/Async/AsyncServerSocket.tpp>

#endif