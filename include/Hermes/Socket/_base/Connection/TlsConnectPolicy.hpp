#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/Data/TLSSocketData.hpp>
#include <Hermes/Socket/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsConnectPolicy {
        struct Options : DefaultConnectPolicy<Data>::Options {
            std::chrono::milliseconds handshakeTimeout{}; // std::chrono::seconds{ 10 }

            bool ignoreCertificateErrors{};
            bool requestMutualAuth{};

            // no ALPN/mTLS for now
            // std::span<const std::string_view> alpnProtocols{};
        };

        static ConnectionResultOper Connect(Data& data, Options options) noexcept;
        static void                 Close(Data& data);
        static void                 Abort(Data& data);

    private:
        static ConnectionResultOper ClientHandshake(Data& data, Options options);
    };
}

#include <Hermes/Socket/_base/Connection/TlsConnectPolicy.tpp>

namespace Hermes {
    static_assert(ConnectionPolicyConcept<TlsConnectPolicy, TlsSocketData<>>);
}