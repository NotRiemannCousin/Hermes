#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy;

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type{ Data::Type };
        using EndpointType = typename Data::EndpointType;

        struct ListenOptions : DefaultAcceptPolicy<EndpointType, SocketTypeEnum::Stream, Family>::ListenOptions {
            bool requireValidClientCertificate{ true };
        };
        struct AcceptOptions : DefaultAcceptPolicy<EndpointType, SocketTypeEnum::Stream, Family>::AcceptOptions {
            std::chrono::milliseconds handshakeTimeout{};
            bool requestClientCertificate{};
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;
        static ConnectionResultOper Accept(Data& data, Data& outData, AcceptOptions options) noexcept;
        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        static ConnectionResultOper ServerHandshake(Data& data, AcceptOptions options) noexcept;

        friend struct TlsAsyncAcceptPolicy<Data>;
    };

}

#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AcceptPolicyConcept<TlsAcceptPolicy<>, TlsSocketData<>>);
}