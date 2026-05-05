#pragma once
#include <Hermes/Socket/Async/_base/Accept/DefaultAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Accept/TlsAsyncAcceptPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/DefaultAsyncTransferPolicy.hpp>
#include <Hermes/Socket/Async/_base/Transfer/TlsAsyncTransferPolicy.hpp>

#include <ranges>

namespace Hermes {

    template<
        SocketDataConcept SocketData             = DefaultSocketData<>,
        template <class> class AsyncAcceptPolicy   = DefaultAsyncAcceptPolicy,
        template <class> class AsyncTransferPolicy = DefaultAsyncTransferPolicy>
        requires AsyncAcceptPolicyConcept<AsyncAcceptPolicy, SocketData> && AsyncTransferPolicyConcept<AsyncTransferPolicy, SocketData>
    struct AsyncServerSocket {
        using EndpointType       = SocketData::EndpointType;
        using AcceptPolicyType   = AsyncAcceptPolicy<SocketData>;
        using TransferPolicyType = AsyncTransferPolicy<SocketData>;


        AsyncServerSocket(AsyncServerSocket&&) noexcept;
        AsyncServerSocket& operator=(AsyncServerSocket&&) noexcept;
        ~AsyncServerSocket();

        [[nodiscard]] static ConnectionResult<AsyncServerSocket> FromAccepted(SocketData&& data) noexcept;

        template<std::ranges::contiguous_range R>
            requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
        auto AsyncSend(R&& data) noexcept;

        template<std::ranges::contiguous_range R>
            requires ByteLike<std::remove_cv_t<std::ranges::range_value_t<R>>>
        auto AsyncRecv(R&& data, RecvModeEnum mode = RecvModeEnum::All) noexcept;

        auto AsyncShutdown() noexcept;
        void Close() noexcept;
        void Abort() noexcept;

    private:
        AsyncServerSocket() = default;

        SocketData         socketData{};
        AcceptPolicyType   acceptPolicy{};
        TransferPolicyType transferPolicy{};
    };

    using RawTcpAsyncServer = AsyncServerSocket<>;
    using RawTlsAsyncServer = AsyncServerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy, TlsAsyncTransferPolicy>;
}

#include <Hermes/Socket/Async/AsyncServerSocket.tpp>