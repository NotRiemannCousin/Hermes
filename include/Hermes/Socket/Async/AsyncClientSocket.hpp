#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>

#include <exec/task.hpp>

namespace Hermes {

#if HERMES_ENABLE_NATIVE_SCHEDULER
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class ConnectionPolicy       = DefaultAsyncConnectPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    struct AsyncClientSocket;
#endif

    //! @brief An asynchronous client socket that establishes an outgoing connection.
    //!
    //! AsyncClientSocket owns the connected SocketData and delegates connection establishment to ConnectionPolicy and
    //! data transfer to TransferPolicy. The policy types determine the protocol-specific behavior. Unlike the
    //! synchronous ClientSocket, every transfer and connection operation returns a sender that integrates with a
    //! stdexec execution graph instead of blocking the caller. See each policy to understand the exact protocol
    //! semantics they provide.
    //!
    //! Async sockets don't have Recv/Send options because the cancelation behavior is created using STDEXEC's
    //! cancelation. They don't have SendAndLift either because it doesn't compose well with senders.
    template<SocketDataConcept SocketData, class ConnectionPolicy, class TransferPolicy>
        requires AsyncClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    struct AsyncClientSocket final {
#pragma region Aliases

        //! @brief The result type returned when creating the client.
        using ClientSocketResult = ConnectionResult<AsyncClientSocket>;

        //! @brief The data policy used by this socket. Stores the info used by other policies.
        using DataType             = SocketData;
        //! @brief The connection policy used by this socket. Used to open/close the socket.
        using ConnectionPolicyType = ConnectionPolicy;
        //! @brief The transfer policy used by this socket. Used when transferring data.
        using TransferPolicyType   = TransferPolicy;

        //! @brief The endpoint type used by SocketData.
        using EndpointType = SocketData::EndpointType;

        //! @brief The options accepted by connect operations.
        using ConnOptions = ConnectionPolicy::Options;

#pragma endregion

        AsyncClientSocket(AsyncClientSocket&&) noexcept;
        AsyncClientSocket& operator=(AsyncClientSocket&&) noexcept;
        ~AsyncClientSocket();

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


#pragma region Connection Methods

        //! @brief Creates and connects a client socket.
        //! @details Connect returns a sender that, when started, establishes the connection and completes with the
        //! connected AsyncClientSocket on success or with a ConnectionErrorEnum on failure. For policies that perform
        //! a handshake, the handshake is carried out as part of establishing the connection. See the ConnectionPolicy
        //! to understand the exact protocol semantics it provides.
        //! @param data SocketData containing the endpoint to connect to.
        //! @return A sender whose value completion carries an AsyncClientSocket and whose error completion carries a
        //! ConnectionErrorEnum.
        template<class = void>
        [[nodiscard]] static auto Connect(SocketData&& data) noexcept
            requires std::default_initializable<ConnOptions>;

        //! @brief Creates and connects a client socket using explicit connection options.
        //! @copydetails Connect
        //! @param opt Connection-policy options including any deadline or timeout.
        //! @return A sender whose value completion carries an AsyncClientSocket and whose error completion carries a
        //! ConnectionErrorEnum.
        [[nodiscard]] static auto Connect(SocketData&& data, ConnOptions opt) noexcept;

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
        AsyncClientSocket() = default;

        SocketData       m_socketData{};
        ConnectionPolicy m_connectionPolicy{};
        TransferPolicy   m_transferPolicy{};
    };


    template<class T>
    constexpr bool IsAsyncClientSocket{ false };
    template<class... T>
    constexpr bool IsAsyncClientSocket<AsyncClientSocket<T...>>{ true };

    template<class T>
    concept AsyncClientSocketConcept = IsAsyncClientSocket<T>;


#if HERMES_ENABLE_NATIVE_SCHEDULER
    //! @brief Alias to the raw TCP client async socket.
    using RawTcpAsyncClient = AsyncClientSocket<>;
    extern template struct AsyncClientSocket<DefaultSocketData<>, DefaultAsyncConnectPolicy<>, DefaultAsyncTransferPolicy<>>;
#if HERMES_ENABLE_TLS
    //! @brief Alias to the raw TLS client async socket.
    using RawTlsAsyncClient = AsyncClientSocket<TlsSocketData<>, TlsAsyncConnectPolicy<>, TlsAsyncTransferPolicy<>>;
    extern template struct AsyncClientSocket<TlsSocketData<>, TlsAsyncConnectPolicy<>, TlsAsyncTransferPolicy<>>;
#endif
#endif
    // using RawUdpAsyncClient = AsyncClientSocket<DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>>;
}

#include <Hermes/Socket/Async/AsyncClientSocket.tpp>

#endif