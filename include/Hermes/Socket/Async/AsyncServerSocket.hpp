#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAsyncAcceptPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    struct AsyncServerSocket {
        using EndpointType = SocketData::EndpointType;

        AsyncServerSocket(AsyncServerSocket&&) noexcept;
        AsyncServerSocket& operator=(AsyncServerSocket&&) noexcept;
        ~AsyncServerSocket();

        //! @brief Creates a server socket from already accepted socket data.
        //! @param data The socket data, typically from a listener.
        //! @return A new AsyncServerSocket instance.
        [[nodiscard]] static AsyncServerSocket FromAccepted(SocketData&& data) noexcept;

        [[nodiscard]] EndpointType& GetEndpoint() noexcept { return m_socketData.endpoint; }
        [[nodiscard]] const EndpointType& GetEndpoint() const noexcept { return m_socketData.endpoint; }
        [[nodiscard]] SocketData& GetSocketData() noexcept { return m_socketData; }

        template<ContiguousByteRange R>
        auto Send(R&& data);

        template<WritableContiguousByteRange R>
        auto Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All);

        auto Shutdown() noexcept;
        void Close() noexcept;
        void Abort() noexcept;

    private:
        AsyncServerSocket() = default;

        SocketData     m_socketData{};
        AcceptPolicy   m_acceptPolicy{};
        TransferPolicy m_transferPolicy{};
    };

    //! @brief Alias to the raw TCP server async socket.
    using RawTcpAsyncServer = AsyncServerSocket<>;
#if HERMES_ENABLE_TLS && HERMES_ENABLE_NATIVE_SCHEDULER
    //! @brief Alias to the raw TLS server async socket.
    using RawTlsAsyncServer = AsyncServerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
#endif
}

#include <Hermes/Socket/Async/AsyncServerSocket.tpp>

#endif