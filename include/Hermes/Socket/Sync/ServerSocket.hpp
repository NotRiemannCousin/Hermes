#pragma once
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.hpp>


#include <ranges>

namespace Hermes {

    //! @brief A connected socket produced by ListenerSocket::AcceptOne / AcceptAll.
    //!
    //! @details Structurally identical to ClientSocket in terms of data transfer
    //! (Send / Recv / RecvRange / Close), but carries AcceptPolicy instead of
    //! ConnectionPolicy because:
    //!   - It never calls Connect() — the connection was established by accept().
    //!   - Close() must follow the same protocol-level teardown as the accept side
    //!     (e.g. sending a TLS close_notify alert for TlsAcceptPolicy).
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAcceptPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
        requires ServerSocketConcept<SocketData, AcceptPolicy, TransferPolicy>
    struct ServerSocket {
        using EndpointType = SocketData::EndpointType;


        ServerSocket(ServerSocket&&) noexcept;
        ServerSocket& operator=(ServerSocket&&) noexcept;
        ~ServerSocket();

        //! @brief Wraps an already-accepted SocketData into a ServerSocket.
        //! Called internally by ListenerSocket after a successful Accept().
        [[nodiscard]] static ConnectionResult<ServerSocket> FromAccepted(SocketData&& data) noexcept;



        //! @return Returns the count of data sent on success, or an error on failure.
        template<ContiguousByteRange R>
        StreamByteOper Send(R&& data) noexcept;

        //! @return Returns the count of data filled on success, or an error on failure.
        template<WritableContiguousByteRange R>
        StreamByteOper Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        //! @return Returns a seamlessly input_range to the data received by the socket.
        //!
        //! The data is fetched in the `operator*()` to prevent getting stuck by keep-alive or other
        //! idle state triggered by calling ++ one more time. Because of this behavior, it potentially
        //! will discover that the transmission ended while trying to get a new value, so a 0x04 char
        //! (end of transmission) will be added as the last char.
        template<ByteLike Byte = std::byte>
        auto RecvStream() noexcept;

        //! @return Returns a seamlessly input_range to the data received by the socket.
        //!
        //! It delays the char retrieved from RecvStream() by 1 and discards the last char (0x04
        //! end of transmission in cause of exhaustive read), working similar to what you get in
        //! other languages (retrieving the data on ++).
        //!
        //! @note If it isn't an exhaustive read, the next char that you was expecting will be
        //! discarded anyway so keep you aware of this.
        template<ByteLike Byte = std::byte>
        auto RecvRange() noexcept;



        void Close() noexcept;
        void Abort() noexcept;

    private:
        ServerSocket() = default;

        SocketData     socketData{};
        AcceptPolicy   acceptPolicy{};
        TransferPolicy transferPolicy{};
    };


    using RawTcpServer = ServerSocket<>;
#ifdef _WIN32
    using RawTlsServer = ServerSocket<TlsSocketData<>, TlsAcceptPolicy<>, TlsTransferPolicy<>>;
#endif
    // using RawUdpServer = ServerSocket<
    //     DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultAcceptPolicy<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultTransferPolicy<SocketTypeEnum::Dgram>
    // >;
}

#include <Hermes/Socket/Sync/ServerSocket.tpp>
