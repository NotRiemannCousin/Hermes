#pragma once
#include <Hermes/Socket/Async/AsyncServerSocket.hpp>
#include <Hermes/Socket/_base.hpp>
#include <stdexec/execution.hpp>

namespace Hermes {

    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAsyncAcceptPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
    requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    struct AsyncListenerSocket {
    public:
        struct ListenSender;
        using ServerSocketType = AsyncServerSocket<SocketData, AcceptPolicy, TransferPolicy>;
        using EndpointType = SocketData::EndpointType;

        AsyncListenerSocket(AsyncListenerSocket&&) noexcept;
        AsyncListenerSocket& operator=(AsyncListenerSocket&&) noexcept;
        ~AsyncListenerSocket();

        /**
         * @brief Creates a listener socket. This is a synchronous operation.
         * @return A sender that yields an AsyncListenerSocket on success.
         */
        template<class = void>
        [[nodiscard]] static ListenSender Listen(SocketData&& data, int backlog = SOMAXCONN)
            requires std::default_initializable<typename AcceptPolicy::ListenOptions>;

        /**
         * @brief Creates a listener socket. This is a synchronous operation.
         * @return A sender that yields an AsyncListenerSocket on success.
         */
        [[nodiscard]] static ListenSender Listen(SocketData&& data, typename AcceptPolicy::ListenOptions opt, int backlog = SOMAXCONN);

        //! @brief Asynchronously accepts a new client connection.
        //! @return A sender that yields an AsyncServerSocket on success.
        template<class = void>
        [[nodiscard]] auto AsyncAcceptOne()
            requires std::default_initializable<typename AcceptPolicy::AcceptOptions>;

        //! @copydoc AsyncAcceptOne
        [[nodiscard]] auto AsyncAcceptOne(typename AcceptPolicy::AcceptOptions opt);

        [[nodiscard]] const EndpointType& GetEndpoint() const noexcept { return socketData.endpoint; }

        void Close() noexcept;
        void Abort() noexcept;

    private:
        friend struct ListenSender;
        AsyncListenerSocket() = default;

        static ConnectionResult<AsyncListenerSocket> InternalCreateAndListen(
            SocketData&& data,
            typename AcceptPolicy::ListenOptions opt,
            int backlog) noexcept;

        SocketData   socketData{};
        AcceptPolicy acceptPolicy{};
    };

    using RawTcpAsyncListener = AsyncListenerSocket<>;
    using RawTlsAsyncListener = AsyncListenerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;

}

#include <Hermes/Socket/Async/AsyncListenerSocket.tpp>