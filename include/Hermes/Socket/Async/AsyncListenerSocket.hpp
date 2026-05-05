#pragma once
#include <Hermes/Socket/Async/AsyncServerSocket.hpp>

namespace Hermes {

    template<
        SocketDataConcept SocketDataType               = DefaultSocketData<>,
        template <class> class AsyncAcceptPolicyType   = DefaultAsyncAcceptPolicy,
        template <class> class AsyncTransferPolicyType = DefaultAsyncTransferPolicy>
    requires AsyncAcceptPolicyConcept<AsyncAcceptPolicyType, SocketDataType> && AsyncTransferPolicyConcept<AsyncTransferPolicyType, SocketDataType>
    struct AsyncListenerSocket {
        using ServerSocketType = AsyncServerSocket<SocketDataType, AsyncAcceptPolicyType, AsyncTransferPolicyType>;


        AsyncListenerSocket(AsyncListenerSocket&&) noexcept;
        AsyncListenerSocket& operator=(AsyncListenerSocket&&) noexcept;
        ~AsyncListenerSocket();

        template<class = void>
        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            Listen(SocketDataType&& data, int backlog = SOMAXCONN) noexcept
            requires std::default_initializable<typename AsyncAcceptPolicyType<SocketDataType>::ListenOptions>;

        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            Listen(SocketDataType&& data, AsyncAcceptPolicyType<SocketDataType>::ListenOptions opt, int backlog = SOMAXCONN) noexcept;

        template<class = void>
        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            ListenOne(SocketDataType&& data) noexcept
            requires std::default_initializable<typename AsyncAcceptPolicyType<SocketDataType>::ListenOptions>;

        [[nodiscard]] static ConnectionResult<AsyncListenerSocket>
            ListenOne(SocketDataType&& data, AsyncAcceptPolicyType<SocketDataType>::ListenOptions opt) noexcept;

        //! @brief Asynchronously accepts a new client connection.
        //! @return A sender that yields an AsyncServerSocket on success.
        template<class = void>
        [[nodiscard]] auto AsyncAcceptOne() noexcept
            requires std::default_initializable<typename AsyncAcceptPolicyType<SocketDataType>::AcceptOptions>;

        //! @copydoc AsyncAcceptOne
        [[nodiscard]] auto AsyncAcceptOne(AsyncAcceptPolicyType<SocketDataType>::AcceptOptions opt) noexcept;

        // Note: AsyncAcceptAll() generator is intentionally omitted as std::async_generator
        // is not yet in the C++ standard. Use `repeat_effect` or `while` inside coroutines.

        void Close() noexcept;
        void Abort() noexcept;

    private:
        AsyncListenerSocket() = default;

        SocketDataType                   socketData{};
        AsyncAcceptPolicyType<SocketDataType> acceptPolicy{};
    };

    using RawTcpAsyncListener = AsyncListenerSocket<>;
    using RawTlsAsyncListener = AsyncListenerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy, TlsAsyncTransferPolicy>;

}

#include <Hermes/Socket/Async/AsyncListenerSocket.tpp>