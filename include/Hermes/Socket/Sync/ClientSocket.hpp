#pragma once
#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.hpp>


namespace Hermes {

    //! @brief A blocking client socket that establishes an outgoing connection.
    //!
    //! @details ClientSocket owns the connected SocketData and delegates connection
    //! establishment to ConnectionPolicy and data transfer to TransferPolicy. The
    //! policy types determine the protocol-specific behavior; for example, the
    //! default policies provide TCP support and the TLS policies provide the TLS
    //! handshake and encrypted transfer.
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class ConnectionPolicy       = DefaultConnectPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    struct ClientSocket {
        //! @brief The endpoint type used by SocketData.
        using EndpointType       = SocketData::EndpointType;
        //! @brief The transfer policy used by this socket.
        using TransferPolicyType = TransferPolicy;


        //! @brief The options accepted by connect operations.
        using ConnOptions = typename ConnectionPolicy::Options;
        //! @brief The options accepted by receive operations.
        using RecvOptions = typename TransferPolicy::RecvOptions;
        //! @brief The options accepted by send operations.
        using SendOptions = typename TransferPolicy::SendOptions;


        ClientSocket(ClientSocket&&) noexcept;
        ClientSocket& operator=(ClientSocket&&) noexcept;
        ~ClientSocket();


#pragma region Connection Methods

        //! @brief Creates and connects a client socket.
        //! @param data SocketData containing the endpoint to connect to.
        //! @return The connected socket on success, or the connection error on failure.
        template<class = void>
        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(SocketData&& data) noexcept
            requires std::default_initializable<ConnOptions>;

        //! @brief Creates and connects a client socket using explicit connection options.
        //! @param data SocketData containing the endpoint to connect to.
        //! @param opt Connection-policy options, including any handshake deadline.
        //! @return The connected socket on success, or the connection error on failure.
        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(SocketData&& data, ConnOptions opt) noexcept;

#pragma endregion


#pragma region Send Methods

        //! @brief Sends data using the transfer policy.
        //! @param data A contiguous range of bytes to send.
        //! @return The number of bytes sent and an empty error result on success, or
        //! an error describing the failure. A successful call may send fewer bytes
        //! than the range contains.
        template<ContiguousByteRange R>
        StreamByteOper Send(R&& data) noexcept
            requires std::default_initializable<SendOptions>;

        //! @brief Sends data using explicit transfer options.
        //! @param data A contiguous range of bytes to send.
        //! @param options Transfer options for this operation.
        //! @details When a deadline is provided, it is an absolute time point shared
        //! by every partial send belonging to this call. It is not restarted after
        //! a partial write.
        //! @return The number of bytes sent and an empty error result on success, or
        //! an error describing the failure.
        template<ContiguousByteRange R>
        StreamByteOper Send(R&& data, SendOptions options) noexcept;


        //! @brief Sends data and returns the socket on success, all-or-nothing.
        //! @param data A contiguous range of bytes to send.
        //! @return A lambda that receives a socket, try to send the data and re-trhow it on success, or the connection
        //! error on failure.
        template<ContiguousByteRange R>
        [[nodiscard]] static auto SendAndLift(R&& data) noexcept
            requires std::default_initializable<SendOptions>;

        //! @brief Sends data and returns the socket on success, all-or-nothing.
        //! @param data A contiguous range of bytes to send.
        //! @param options Transfer options for this operation.
        //! @return A lambda that receives a socket, try to send the data and re-trhow it on success, or the connection
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
        StreamByteOper Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept
            requires std::default_initializable<RecvOptions>;

        //! @brief Receives data using explicit receive options.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param mode Whether the operation receives any available data or fills
        //! the range before completing.
        //! @param options Receive options for this operation.
        //! @details A provided deadline is absolute and applies to the complete
        //! receive operation, including all partial reads.
        //! @return The number of bytes received and an empty error result on success,
        //! or an error describing the failure.
        template<WritableContiguousByteRange R>
        StreamByteOper Recv(R&& data, RecvModeEnum mode, RecvOptions options) noexcept;

        //! @brief Receives a complete range using explicit receive options.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param options Receive options for this operation.
        //! @return The number of bytes received and an empty error result on success,
        //! or an error describing the failure.
        template<WritableContiguousByteRange R>
        StreamByteOper Recv(R&& data, RecvOptions options) noexcept;

        //! @brief Returns a lazy input range over the bytes received by the socket.
        //!
        //! Data is fetched by `operator*()` rather than by `operator++()`. This
        //! prevents advancing the range from blocking while the peer is keeping the
        //! connection alive without sending data. If the transmission ends while a
        //! new value is being fetched, the range appends `0x04` as its end marker.
        template<ByteLike Byte = std::byte>
        auto RecvStream() noexcept
            requires std::default_initializable<RecvOptions>;

        //! @brief Returns a lazy input range bounded by one absolute receive deadline.
        //! @param options Receive options retained by the range for every internal read.
        //! @details The deadline applies to the complete lazy operation and is not
        //! restarted for each byte or block obtained from the range.
        template<ByteLike Byte = std::byte>
        auto RecvStream(RecvOptions options) noexcept;

        //! @brief Returns a lazy input range with conventional increment semantics.
        //!
        //! RecvRange() obtains the next value on increment rather than on dereference.
        //! It is implemented on top of RecvStream(), omits the stream end marker `0x04`,
        //! and therefore behaves like a conventional input range for exhaustive reads.
        //!
        //! @note For a non-exhaustive read, the value following the requested range
        //! may be consumed while detecting the end of the stream.
        template<ByteLike Byte = std::byte>
        auto RecvRange() noexcept
            requires std::default_initializable<RecvOptions>;

        //! @brief Returns a lazy input range with explicit receive options.
        //! @param options Receive options retained for the complete range operation.
        template<ByteLike Byte = std::byte>
        auto RecvRange(RecvOptions options) noexcept;

#pragma endregion


#pragma region Close Methods

        //! @brief Performs the protocol-level graceful shutdown and closes the socket.
        void Close() noexcept;
        //! @brief Immediately closes the socket without a protocol-level shutdown.
        void Abort() noexcept;

#pragma endregion

    private:
        ClientSocket() = default;

        SocketData       m_socketData{};
        ConnectionPolicy m_connectionPolicy{};
        TransferPolicy   m_transferPolicy{};
    };

    //! @brief Raw TCP client socket.
    using RawTcpClient = ClientSocket<>;
    //! @brief Raw TLS client socket.
    using RawTlsClient = ClientSocket<TlsSocketData<>, TlsConnectPolicy<>, TlsTransferPolicy<>>;
    // using RawUdpClient = ClientSocket<
    //     DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultConnectPolicy<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultTransferPolicy<SocketTypeEnum::Dgram>
    // >;
}

#include <Hermes/Socket/Sync/ClientSocket.tpp>
