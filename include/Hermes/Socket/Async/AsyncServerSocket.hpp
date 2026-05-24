#pragma once
#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
// #include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

#include <ranges>

namespace Hermes {

    template<
        SocketDataConcept SocketData = DefaultSocketData<>,
        class AcceptPolicy           = DefaultAsyncAcceptPolicy<>,
        class TransferPolicy         = DefaultAsyncTransferPolicy<>>
        requires AsyncAcceptPolicyConcept<AcceptPolicy, SocketData> && AsyncTransferPolicyConcept<TransferPolicy, SocketData>
    struct AsyncServerSocket {
        using EndpointType   = SocketData::EndpointType;

        AsyncServerSocket(AsyncServerSocket&&) noexcept;
        AsyncServerSocket& operator=(AsyncServerSocket&&) noexcept;
        ~AsyncServerSocket();

        /**
         * @brief Creates a server socket from already accepted socket data.
         * @param data The socket data, typically from a listener.
         * @return A new AsyncServerSocket instance.
         */
        [[nodiscard]] static AsyncServerSocket FromAccepted(SocketData&& data) noexcept;

        [[nodiscard]] EndpointType& GetEndpoint() noexcept { return socketData.endpoint; }
        [[nodiscard]] const EndpointType& GetEndpoint() const noexcept { return socketData.endpoint; }
        [[nodiscard]] SocketData& GetSocketData() noexcept { return socketData; }

        template<ContiguousByteRange R>
        auto Send(R&& data);

        template<WritableContiguousByteRange R>
        auto Recv(R&& data, RecvModeEnum mode = RecvModeEnum::All);

        auto Shutdown() noexcept;
        void Close() noexcept;
        void Abort() noexcept;

    private:
        AsyncServerSocket() = default;

        SocketData     socketData{};
        AcceptPolicy   acceptPolicy{};
        TransferPolicy transferPolicy{};
    };

    using RawTcpAsyncServer = AsyncServerSocket<>;
    // using RawTlsAsyncServer = AsyncServerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
}

#include <Hermes/Socket/Async/AsyncServerSocket.tpp>