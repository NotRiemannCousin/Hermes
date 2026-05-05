#pragma once
#include <Hermes/Socket/Async/_base/Connection/DefaultAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>

#include <stdexec/execution.hpp>
#include <ranges>

namespace Hermes {
    template<
        SocketDataConcept SocketData                 = DefaultSocketData<>,
        template <class> class AsyncConnectionPolicy = DefaultAsyncConnectPolicy,
        template <class> class AsyncTransferPolicy   = DefaultAsyncTransferPolicy>
        requires AsyncConnectionPolicyConcept<AsyncConnectionPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    struct AsyncClientSocket {
        using EndpointType         = typename SocketData::EndpointType;
        using ConnectionPolicyType = AsyncConnectionPolicy<SocketData>;
        using TransferPolicyType   = AsyncTransferPolicy<SocketData>;


        AsyncClientSocket(AsyncClientSocket&&) noexcept;
        AsyncClientSocket& operator=(AsyncClientSocket&&) noexcept;
        ~AsyncClientSocket();

        template<class = void>
        [[nodiscard]] static auto AsyncConnect(SocketData&& data) noexcept
            requires std::default_initializable<typename ConnectionPolicyType::Options>;

        [[nodiscard]] static auto AsyncConnect(SocketData&& data, ConnectionPolicyType::Options opt) noexcept;

        //! @brief Asynchronously writes data to the socket.
        //! @return A sender that yields the number of bytes sent or a TransferError on failure.
        template<std::ranges::contiguous_range R>
            requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
        auto AsyncSend(R&& data) noexcept;

        //! @brief Asynchronously reads data from the socket.
        //! @return A sender that yields the number of bytes received or a TransferError on failure.
        template<std::ranges::contiguous_range R>
            requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
        auto AsyncRecv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        //! @brief Initiates asynchronous protocol-level shutdown (e.g., TLS close_notify or TCP FIN).
        //! @return A sender that yields on successful shutdown.
        auto AsyncShutdown() noexcept;

        //! @brief Synchronously destroys the underlying handle and cancels pending I/O.
        void Close() noexcept;

        //! @brief Terminates the connection abruptly.
        void Abort() noexcept;

    private:
        AsyncClientSocket() = default;

        SocketData           socketData{};
        ConnectionPolicyType connectionPolicy{};
        TransferPolicyType   transferPolicy{};
    };

    using RawTcpAsyncClient = AsyncClientSocket<>;
    using RawTlsAsyncClient = AsyncClientSocket<TlsSocketData<>, TlsAsyncConnectPolicy, TlsAsyncTransferPolicy>;
    using RawUdpAsyncClient = AsyncClientSocket<DefaultSocketData<IpEndpoint, SocketTypeEnum::Dgram>>;
}

#include <Hermes/Socket/Async/AsyncClientSocket.tpp>