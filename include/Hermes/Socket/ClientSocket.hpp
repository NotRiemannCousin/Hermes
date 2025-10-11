#pragma once
#include <Hermes/Socket/_base/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/_base/DefaultTransferPolicy.hpp>
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>

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

        [[nodiscard]] static ConnectionResult<ClientSocket> Connect(const EndpointType& endpoint) noexcept;

        //! @return Returns the count of data sent on success, or an error on failure.
        template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
        StreamByteCount Send(R& data) noexcept;

        //! @return Returns the count of data filled on success, or an error on failure.
        template<std::ranges::contiguous_range R>
        requires ByteLike<std::ranges::range_value_t<R>>
        StreamByteCount Recv(R& data) noexcept;

        //! @return Returns a seamlessly input_range to the data received by the socket.
        template<ByteLike Byte = std::byte>
        typename TransferPolicy<SocketData>::template RecvRange<Byte> RecvRange() noexcept;

        void Close() noexcept;

    private:
        ClientSocket() = default;

        SocketData           socketData{};
        ConnectionPolicyType connectionPolicy{};
        TransferPolicyType   transferPolicy{};
    };

    using RawTcpClient = ClientSocket<>;
    using RawCharTcpClient = ClientSocket<>;
    using RawUdpClient = ClientSocket<DefaultSocketData<IpEndpoint, SocketTypeEnum::DGRAM>>;
}

#include <Hermes/Socket/ClientSocket.tpp>
