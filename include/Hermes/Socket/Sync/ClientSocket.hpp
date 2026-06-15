#pragma once
#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.hpp>


namespace Hermes {
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class ConnectionPolicy       = DefaultConnectPolicy<>,
        class TransferPolicy         = DefaultTransferPolicy<>>
        requires ClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    struct ClientSocket {
        using EndpointType = SocketData::EndpointType;


        ClientSocket(ClientSocket&&) noexcept;
        ClientSocket& operator=(ClientSocket&&) noexcept;
        ~ClientSocket();



        template<class = void>
        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(SocketData&& data) noexcept
            requires std::default_initializable<typename ConnectionPolicy::Options>;

        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(SocketData&& data, ConnectionPolicy::Options opt) noexcept;



        //! @return Returns the count of data sent on success, or an error on failure.
        template<ContiguousByteRange R>
        StreamByteOper Send(R&& data) noexcept;

        //! @return Returns the count of data filled on success, or an error on failure.
        template<WritableContiguousByteRange R>
        StreamByteOper Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        //! @return Returns a seamless input_range to the data received by the socket.
        //!
        //! The data is fetched in the `operator*()` to prevent getting stuck by keep-alive or other
        //! idle state triggered by calling ++ one more time. Because of this behavior, it potentially
        //! will discover that the transmission ended while trying to get a new value, so a 0x04 char
        //! (end of transmission) will be added as the last char.
        template<ByteLike Byte = std::byte>
        auto RecvStream() noexcept;

        //! @return Returns a seamless input_range to the data received by the socket.
        //!
        //! It delays the char retrieved from RecvStream() by 1 and discards the last char (0x04
        //! end of transmission in case of exhaustive read), working similar to what you get in
        //! other languages (retrieving the data on ++).
        //!
        //! @note If it isn't an exhaustive read, the next char that you was expecting will be
        //! discarded anyway so keep you aware of this.
        template<ByteLike Byte = std::byte>
        auto RecvRange() noexcept;


        
        void Close() noexcept;
        void Abort() noexcept;

    private:
        ClientSocket() = default;

        SocketData       socketData{};
        ConnectionPolicy connectionPolicy{};
        TransferPolicy   transferPolicy{};
    };

    using RawTcpClient = ClientSocket<>;
    using RawTlsClient = ClientSocket<TlsSocketData<>, TlsConnectPolicy<>, TlsTransferPolicy<>>;
    // using RawUdpClient = ClientSocket<
    //     DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultConnectPolicy<IpEndpoint, SocketTypeEnum::Dgram>,
    //     DefaultTransferPolicy<SocketTypeEnum::Dgram>
    // >;
}

#include <Hermes/Socket/Sync/ClientSocket.tpp>
