#pragma once
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.hpp>


#include <ranges>

namespace Hermes {

    //! @brief A blocking socket accepted by a ListenerSocket.
    //!
    //! @details ServerSocket is produced by ListenerSocket::AcceptOne() or
    //! ListenerSocket::AcceptAll() after a peer connection has been accepted.
    //! It provides the same transfer operations as ClientSocket, but retains
    //! AcceptPolicy because the accepted side is responsible for the protocol-level
    //! teardown associated with the accept policy, such as a TLS close-notify alert.
    //! ServerSocket never establishes a connection with Connect(); the connection
    //! already exists when the object is created.
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAcceptPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct ServerSocket {
        //! @brief The endpoint type used by SocketData.
        using EndpointType      = SocketData::EndpointType;
        //! @brief The transfer policy used by this socket.
        using TransferPolicyType = TransferPolicy;
        //! @brief The options accepted by receive operations.
        using RecvOptions        = typename TransferPolicy::RecvOptions;
        //! @brief The options accepted by send operations.
        using SendOptions        = typename TransferPolicy::SendOptions;


        ServerSocket(ServerSocket&&) noexcept;
        ServerSocket& operator=(ServerSocket&&) noexcept;
        ~ServerSocket();

        //! @brief Wraps an already-accepted SocketData into a ServerSocket.
        //! @param data SocketData containing the accepted socket and its endpoint.
        //! @return A server socket on success, or the connection error on failure.
        //! @details This factory is used by ListenerSocket after a successful
        //! accept operation. The returned object owns the accepted socket.
        [[nodiscard]] static ConnectionResult<ServerSocket> FromAccepted(SocketData&& data) noexcept;



        //! @brief Sends data using the transfer policy.
        //! @param data A contiguous range of bytes to send.
        //! @return The number of bytes sent and an empty error result on success, or
        //! an error describing the failure. A successful call may send fewer bytes
        //! than the range contains.
        template<ContiguousByteRange R>
        StreamByteOper Send(R&& data) noexcept
            requires std::default_initializable<typename TransferPolicy::SendOptions>;

        //! @brief Sends data using explicit transfer options.
        //! @param data A contiguous range of bytes to send.
        //! @param options Transfer options for this operation.
        //! @details A provided deadline is absolute and is shared by every partial
        //! send belonging to this call; it is not restarted after a partial write.
        //! @return The number of bytes sent and an empty error result on success, or
        //! an error describing the failure.
        template<ContiguousByteRange R>
        StreamByteOper Send(R&& data, typename TransferPolicy::SendOptions options) noexcept;

        //! @brief Receives data using the selected receive mode.
        //! @param data A writable contiguous range that receives the bytes.
        //! @param mode Whether the operation receives any available data or fills
        //! the range before completing.
        //! @return The number of bytes received and an empty error result on success,
        //! or an error describing the failure.
        template<WritableContiguousByteRange R>
        StreamByteOper Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept
            requires std::default_initializable<typename TransferPolicy::RecvOptions>;

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
        StreamByteOper Recv(
            R&& data,
            RecvModeEnum mode,
            typename TransferPolicy::RecvOptions options
        ) noexcept;

        template<WritableContiguousByteRange R>
        StreamByteOper Recv(R&& data, typename TransferPolicy::RecvOptions options) noexcept;

        //! @brief Returns a lazy input range over the bytes received by the socket.
        //!
        //! Data is fetched by `operator*()` rather than by `operator++()`. This
        //! prevents advancing the range from blocking while the peer is keeping the
        //! connection alive without sending data. If the transmission ends while a
        //! new value is being fetched, the range appends `0x04` as its end marker.
        template<ByteLike Byte = std::byte>
        auto RecvStream() noexcept
            requires std::default_initializable<typename TransferPolicy::RecvOptions>;

        //! @brief Returns a lazy input range bounded by one absolute receive deadline.
        //! @param options Receive options retained by the range for every internal read.
        //! @details The deadline applies to the complete lazy operation and is not
        //! restarted for each byte or block obtained from the range.
        template<ByteLike Byte = std::byte>
        auto RecvStream(typename TransferPolicy::RecvOptions options) noexcept;

        //! @brief Returns a lazy input range with conventional increment semantics.
        //!
        //! RecvRange() obtains the next value on increment rather than on dereference.
        //! It is implemented on top of RecvStream(), omits the stream end marker `0x04`,
        //! and behaves like a conventional input range for exhaustive reads.
        //!
        //! @note For a non-exhaustive read, the value following the requested range
        //! may be consumed while detecting the end of the stream.
        template<ByteLike Byte = std::byte>
        auto RecvRange() noexcept
            requires std::default_initializable<typename TransferPolicy::RecvOptions>;

        template<ByteLike Byte = std::byte>
        auto RecvRange(typename TransferPolicy::RecvOptions options) noexcept;



        //! @brief Performs the protocol-level graceful shutdown and closes the socket.
        void Close() noexcept;
        //! @brief Immediately closes the socket without a protocol-level shutdown.
        void Abort() noexcept;

    private:
        ServerSocket() = default;

        SocketData     m_socketData{};
        AcceptPolicy   m_acceptPolicy{};
        TransferPolicy m_transferPolicy{};
    };


    using RawTcpServer = ServerSocket<>;
    using RawTlsServer = ServerSocket<TlsSocketData<>, TlsAcceptPolicy<>, TlsTransferPolicy<>>;
    // using RawUdpServer = ServerSocket<
    //     DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultAcceptPolicy<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultTransferPolicy<SocketTypeEnum::Dgram>
    // >;
}

#include <Hermes/Socket/Sync/ServerSocket.tpp>
