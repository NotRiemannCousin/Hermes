#pragma once
#include <Hermes/Socket/Async/AsyncServerSocket.hpp>

namespace Hermes {

    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAsyncAcceptPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
    requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    struct AsyncListenerSocket {
        using ServerSocketType = AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>;
        using EndpointType = SocketData::EndpointType;

        AsyncListenerSocket(AsyncListenerSocket&&) noexcept;
        AsyncListenerSocket& operator=(AsyncListenerSocket&&) noexcept;
        ~AsyncListenerSocket();

        template<class = void>
        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            Listen(SocketData&& data, int backlog = SOMAXCONN) noexcept
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            Listen(SocketData&& data, typename AcceptPolicy::ListenOptions opt, int backlog = SOMAXCONN) noexcept;

        template<class = void>
        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            ListenOne(SocketData&& data) noexcept
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            ListenOne(SocketData&& data, typename AcceptPolicy::ListenOptions opt) noexcept;

        //! @brief Asynchronously accepts a new client connection.
        //! @return A sender that yields an AsyncServerSocket on success.
        template<class = void>
        [[nodiscard]] auto AsyncAcceptOne() noexcept
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AsyncAcceptOne
        [[nodiscard]] auto AsyncAcceptOne(typename AcceptPolicy::AcceptOptions opt) noexcept;

        [[nodiscard]] const EndpointType& GetEndpoint() const noexcept { return socketData.endpoint; }

        // Note: AsyncAcceptAll() generator is intentionally omitted as std::async_generator
        // is not yet in the C++ standard. Use `repeat_effect` or `while` inside coroutines.

        void Close() noexcept;
        void Abort() noexcept;

    private:
        AsyncListenerSocket() = default;

        SocketData   socketData{};
        AcceptPolicy acceptPolicy{};
    };

    using RawTcpAsyncListener = AsyncListenerSocket<>;
    using RawTlsAsyncListener = AsyncListenerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;

}

#include <Hermes/Socket/Async/AsyncListenerSocket.tpp>