#pragma once
#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>

#include <stdexec/execution.hpp>
#include <ranges>
#include <exec/task.hpp>

namespace Hermes {
    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class ConnectionPolicy       = DefaultAsyncConnectPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
        requires AsyncClientSocketConcept<SocketData, ConnectionPolicy, TransferPolicy>
    struct AsyncClientSocket {
        using EndpointType = typename SocketData::EndpointType;

        AsyncClientSocket(AsyncClientSocket&&) noexcept;
        AsyncClientSocket& operator=(AsyncClientSocket&&) noexcept;
        ~AsyncClientSocket();

        template<class = void>
        [[nodiscard]] static auto Connect(SocketData&& data) noexcept
            requires std::default_initializable<typename ConnectionPolicy::Options>;

        [[nodiscard]] static auto Connect(SocketData&& data, ConnectionPolicy::Options opt) noexcept;

        //! @brief Asynchronously writes data to the socket.
        //! @return A sender that yields the number of bytes sent or a TransferError on failure.
        template<ContiguousByteRange R>
        auto Send(R&& data) noexcept;

        //! @brief Asynchronously reads data from the socket.
        //! @return A sender that yields the number of bytes received or a TransferError on failure.
        template<WritableContiguousByteRange R>
        auto Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        //! @brief Initiates asynchronous protocol-level shutdown (e.g., TLS close_notify or TCP FIN).
        //! @return A sender that yields on successful shutdown.
        auto Shutdown() noexcept;

        //! @brief Synchronously destroys the underlying handle and cancels pending I/O.
        void Close() noexcept;

        //! @brief Terminates the connection abruptly.
        void Abort() noexcept;

    private:
        AsyncClientSocket() = default;

        SocketData       socketData{};
        ConnectionPolicy connectionPolicy{};
        TransferPolicy   transferPolicy{};
    };



    using RawTcpAsyncClient = AsyncClientSocket<>;
    using RawTlsAsyncClient = AsyncClientSocket<TlsSocketData<>, TlsAsyncConnectPolicy<>, TlsAsyncTransferPolicy<>>;
    // using RawUdpAsyncClient = AsyncClientSocket<DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>>;
}

#include <Hermes/Socket/Async/AsyncClientSocket.tpp>