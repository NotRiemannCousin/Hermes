#pragma once
#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.hpp>


namespace Hermes {

    //! @brief A blocking client socket that establishes an outgoing connection.
    //!
    //! ClientSocket owns the connected SocketData and delegates connection establishment to ConnectionPolicy and data
    //! transfer to TransferPolicy.
    //
    //! The policy types determine the protocol-specific behavior. For example, the default policies provide TCP support
    //! and the TLS policies provide the TLS handshake and encrypted transfer.
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class ConnectionPolicy       = DefaultConnectPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
        requires ClientSocketPoliciesConcept<SocketData, ConnectionPolicy, TransferPolicy>
    struct ClientSocket final {
#pragma region Aliases

        //! @brief The result type returned when creating the client.
        using ClientSocketResult = ConnectionResult<ClientSocket>;

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
        //! @brief The options accepted by receive operations.
        using RecvOptions = TransferPolicy::RecvOptions;
        //! @brief The options accepted by send operations.
        using SendOptions = TransferPolicy::SendOptions;

#pragma endregion

        ClientSocket(ClientSocket&&) noexcept;
        ClientSocket& operator=(ClientSocket&&) noexcept;
        ~ClientSocket();

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
        //! @param data SocketData containing the endpoint to connect to.
        //! @return The connected socket on success, or the connection error on failure.
        template<class = void>
        [[nodiscard]] static auto Connect(SocketData&& data) noexcept -> ClientSocketResult
            requires std::default_initializable<ConnOptions>;

        //! @brief Creates and connects a client socket using explicit connection options.
        //! @param data SocketData containing the endpoint to connect to.
        //! @param opt Connection-policy options, including any handshake deadline.
        //! @return The connected socket on success, or the connection error on failure.
        [[nodiscard]] static auto Connect(SocketData&& data, ConnOptions opt) noexcept -> ClientSocketResult;

#pragma endregion


#pragma region Send Methods

        //! @brief Sends data using the transfer policy.
        //! @param data A contiguous range of bytes to send.
        //! @return The number of bytes sent and an empty error result on success, or an error describing the failure.
        //! A successful call may send fewer bytes than the range contains.
        template<ContiguousByteRange R>
        [[nodiscard]] StreamByteOper Send(R&& data) noexcept
            requires std::default_initializable<SendOptions>;

        //! @brief Sends data using explicit transfer options.
        //! @param data A contiguous range of bytes to send.
        //! @param options Transfer options for this operation.
        //! When a deadline is provided, it is an absolute time point shared by every partial send belonging to this
        //! call. It is not restarted after a partial write.
        //! @return The number of bytes sent and an empty error result on success, or an error describing the failure.
        template<ContiguousByteRange R>
        [[nodiscard]] StreamByteOper Send(R&& data, SendOptions options) noexcept;


        //! @brief Sends data and returns the socket on success, all-or-nothing.
        //! @param data A contiguous range of bytes to send.
        //! @return A lambda with the `(ClientSocket&& self) -> ClientSocketResult` signature that receives a socket.
        //! Tries to send the data and forward it on success, or send the connection error.
        //! error on failure.
        template<ContiguousByteRange R>
        [[nodiscard]] static auto SendAndLift(R&& data) noexcept
            requires std::default_initializable<SendOptions>;

        //! @brief Sends data and returns the socket on success, all-or-nothing.
        //! @param data A contiguous range of bytes to send.
        //! @param options Transfer options for this operation.
        //! @return A lambda with the `(ClientSocket&& self) -> ClientSocketResult` signature that receives a socket.
        //! Tries to send the data and forward it on success, or send the connection error.
        //! error on failure.
        template<ContiguousByteRange R>
        [[nodiscard]] static auto SendAndLift(R&& data, SendOptions options) noexcept;

#pragma endregion


#pragma region Recv Methods

        //! @brief Receives data using the selected receive mode.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param mode Whether the operation receives any available data or fills
        //! the range before completing.
        //! @return The number of bytes received and an empty error result on success,
        //! or an error describing the failure.
        template<WritableContiguousByteRange R>
        [[nodiscard]] StreamByteOper Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept
            requires std::default_initializable<RecvOptions>;

        //! @brief Receives data using explicit receive options.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param mode Whether the operation receives any available data or fills
        //! the range before completing.
        //! @param options Receive options for this operation.
        //! @return The number of bytes received and an empty error result on success, or an error describing the
        //! failure.
        //! @details A provided deadline is absolute and applies to the complete receive operation, including all
        //! partial reads.
        template<WritableContiguousByteRange R>
        [[nodiscard]] StreamByteOper Recv(R&& data, RecvModeEnum mode, RecvOptions options) noexcept;

        //! @brief Receives a complete range using explicit receive options.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param options Receive options for this operation.
        //! @return The number of bytes received and an empty error result on success,
        //! or an error describing the failure.
        template<WritableContiguousByteRange R>
        [[nodiscard]] StreamByteOper Recv(R&& data, RecvOptions options) noexcept;

        //! @brief Returns a lazy input range over the bytes received by the socket.
        //!
        //! Data is fetched by `operator*()` rather than by `operator++()`. This prevents advancing the range from
        //! blocking while the peer is keeping the connection alive without sending data. If the transmission ends
        //! while a new value is being fetched, the range appends `0x04` as its end marker.
        template<ByteLike Byte = std::byte>
        [[nodiscard]] auto RecvStream() noexcept
            requires std::default_initializable<RecvOptions>;

        //! @brief Returns a lazy input range bounded by one absolute receive deadline.
        //! @param options Receive options retained by the range for every internal read.
        //! @details The deadline applies to the complete lazy operation and is not
        //! restarted for each byte or block obtained from the range.
        template<ByteLike Byte = std::byte>
        [[nodiscard]] auto RecvStream(RecvOptions options) noexcept;

        //! @brief Returns a lazy input range with conventional increment semantics.
        //!
        //! RecvRange() obtains the next value on increment rather than on dereference. It is implemented on top of
        //! RecvStream(), omits the stream end marker `0x04`, and therefore behaves like a conventional input range for
        //! exhaustive reads.
        //!
        //! @note For a non-exhaustive read, the value following the requested range may be consumed while detecting
        //! the end of the stream.
        template<ByteLike Byte = std::byte>
        [[nodiscard]] auto RecvRange() noexcept
            requires std::default_initializable<RecvOptions>;

        //! @brief Returns a lazy input range with explicit receive options.
        //! @param options Receive options retained for the complete range operation.
        template<ByteLike Byte = std::byte>
        [[nodiscard]] auto RecvRange(RecvOptions options) noexcept;

#pragma endregion


#pragma region Close Methods

        //! @brief Performs the protocol-level graceful shutdown and closes the socket.
        void Close() noexcept;
        //! @brief Immediately closes the socket without a protocol-level shutdown.
        void Abort() noexcept;

#pragma endregion

    private:
        ClientSocket() = default;

        SocketData m_socketData{};
        [[no_unique_address]] ConnectionPolicy m_connectionPolicy{};
        [[no_unique_address]] TransferPolicy   m_transferPolicy{};
    };


    template<class T>
    constexpr bool IsClientSocket{ false };
    template<class... T>
    constexpr bool IsClientSocket<ClientSocket<T...>>{ true };

    template<class T>
    concept ClientSocketConcept = IsClientSocket<T>;

    //! @brief Alias to the raw TCP client socket.
    using RawTcpClient = ClientSocket<>;
    extern template struct ClientSocket<DefaultSocketData<>, DefaultConnectPolicy<>, DefaultTransferPolicy<>>;
#if HERMES_ENABLE_TLS
    //! @brief Alias to the raw TLS client socket.
    using RawTlsClient = ClientSocket<TlsSocketData<>, TlsConnectPolicy<>, TlsTransferPolicy<>>;
    extern template struct ClientSocket<TlsSocketData<>, TlsConnectPolicy<>, TlsTransferPolicy<>>;
#endif
    // using RawUdpClient = ClientSocket<
    //     DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultConnectPolicy<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultTransferPolicy<SocketTypeEnum::Dgram>
    // >;
}

#include <Hermes/Socket/Sync/ClientSocket.tpp>
