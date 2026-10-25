#pragma once
#if HERMES_ENABLE_TLS

#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Connection/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
#if HERMES_ENABLE_ASYNC
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncConnectPolicy;
#endif

    //! @brief Plain TCP connect followed by a blocking TLS client handshake.
    //! @details Renegotiate() re-enters the handshake state machine on an open connection.
    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsConnectPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = typename Data::EndpointType;

        static constexpr bool IsServer{};

        using DataType = TlsSocketData<EndpointType, Type, Family>;

        struct Options : DefaultConnectPolicy<EndpointType, SocketTypeEnum::Stream, Family>::Options {
            std::chrono::milliseconds handshakeTimeout{};  //!< Budget time for the whole handshake.

            bool ignoreCertificateErrors{};                //!< Skip peer certificate validation (dev only).
            bool requestMutualAuth{};                      //!< Request mutual TLS.

            // no ALPN/mTLS for now
            // std::span<const std::string_view> alpnProtocols{};
        };

        ConnectionResultOper Connect(Data& data, Options options) noexcept;

        void Close(Data& data);

        void Abort(Data& data);

        ConnectionResultOper Renegotiate(Data& data);

    private:
        ConnectionResultOper ClientHandshake(Data& data);

#if HERMES_ENABLE_ASYNC
        template<class ExecutionContext, SocketDataConcept AsyncData>
        friend struct TlsAsyncConnectPolicy;
#endif

    };
}

#include <Hermes/Socket/Sync/_base/Connection/TlsConnectPolicy.tpp>

namespace Hermes {
    static_assert(ConnectionPolicyConcept<TlsConnectPolicy<>, TlsSocketData<>>);
}

#endif
