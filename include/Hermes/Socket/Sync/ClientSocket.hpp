#pragma once
#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.hpp>
#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.hpp>

#include <ranges>

namespace Hermes {
    template<
        SocketDataConcept SocketData            = DefaultSocketData<>,
        template <class> class ConnectionPolicy = DefaultConnectPolicy,
        template <class> class TransferPolicy   = DefaultTransferPolicy>
        requires ConnectionPolicyConcept<ConnectionPolicy, SocketData> && TransferPolicyConcept<TransferPolicy, SocketData>
    struct ClientSocket {
        using EndpointType         = typename SocketData::EndpointType;
        using ConnectionPolicyType = ConnectionPolicy<SocketData>;
        using TransferPolicyType   = TransferPolicy<SocketData>;


        ClientSocket(ClientSocket&&) noexcept;
        ClientSocket& operator=(ClientSocket&&) noexcept;
        ~ClientSocket();


        template<class = void>
        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(SocketData&& data) noexcept
            requires std::default_initializable<typename ConnectionPolicyType::Options>;

        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(SocketData&& data, ConnectionPolicyType::Options opt) noexcept;

        //! @return Returns the count of data sent on success, or an error on failure.
        template<std::ranges::contiguous_range R>
            requires ByteLike<std::ranges::range_value_t<R>>
        StreamByteOper Send(R&& data) noexcept;

        //! @return Returns the count of data filled on success, or an error on failure.
        template<std::ranges::contiguous_range R>
            requires ByteLike<std::ranges::range_value_t<R>>
        StreamByteOper Recv(R&& data) noexcept;

        //! @return Returns a seamlessly input_range to the data received by the socket.
        //!
        //! The data is fetched in the `operator*()` to prevent getting stuck by keep-alive or other
        //! idle state triggered by calling ++ one more time. Because of this behavior, it potentially
        //! will discover that the transmission ended while trying to get a new value, so a 0x04 char
        //! (end of transmission) will be added as the last char.
        template<ByteLike Byte = std::byte>
        typename TransferPolicy<SocketData>::template RecvLazyRange<Byte> RecvLazyRange() noexcept;

        //! @return Returns a seamlessly input_range to the data received by the socket.
        //!
        //! It delays the char retrieved from RecvLazyRange() by 1 and discards the last char (0x04
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
        ClientSocket() = default;

        SocketData           socketData{};
        ConnectionPolicyType connectionPolicy{};
        TransferPolicyType   transferPolicy{};
    };

    using RawTcpClient = ClientSocket<>;
    using RawTlsClient = ClientSocket<TlsSocketData<>, TlsConnectPolicy, TlsTransferPolicy>;
    using RawUdpClient = ClientSocket<DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>>;
}

#include <Hermes/Socket/Sync/ClientSocket.tpp>
