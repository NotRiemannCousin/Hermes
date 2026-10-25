#pragma once
#if HERMES_ENABLE_TLS

#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/Sync/_base/Accept/DefaultAcceptPolicy.hpp>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {

#if HERMES_ENABLE_ASYNC
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy;
#endif

    //! @brief Plain TCP accept (DefaultAcceptPolicy) followed by a blocking TLS server handshake on the accepted
    //! connection.
    //! @details Listen() is pure TCP (forwarded to DefaultAcceptPolicy). Close()/Abort() run the TLS
    //! close-notify / abrupt teardown through the accept state machine when one exists, falling back to plain socket
    //! close otherwise.
    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsAcceptPolicy {
        static constexpr auto Family{ Data::Family };
        static constexpr auto Type  { Data::Type   };
        using EndpointType = typename Data::EndpointType;



        struct ListenOptions : DefaultAcceptPolicy<EndpointType, SocketTypeEnum::Stream, Family>::ListenOptions {
            bool requireValidClientCertificate{ true }; //!< Reject clients without a valid certificate.
        };
        struct AcceptOptions : DefaultAcceptPolicy<EndpointType, SocketTypeEnum::Stream, Family>::AcceptOptions {
            std::chrono::milliseconds handshakeTimeout{}; //!< Budget time for the whole server handshake.
            bool requestClientCertificate{};              //!< Ask for a client certificate (mutual TLS).
        };

        static ConnectionResultOper Listen(Data& data, int backlog, ListenOptions options) noexcept;
        static ConnectionResultOper Accept(Data& data, Data& outData, AcceptOptions options) noexcept;
        static void Close(Data& data) noexcept;
        static void Abort(Data& data) noexcept;

    private:
        static ConnectionResultOper ServerHandshake(Data& data, AcceptOptions options) noexcept;

#if HERMES_ENABLE_ASYNC
        template<class ExecutionContext, SocketDataConcept AsyncData>
        friend struct TlsAsyncAcceptPolicy;
#endif
    };

}

#include <Hermes/Socket/Sync/_base/Accept/TlsAcceptPolicy.tpp>

namespace Hermes {
    static_assert(AcceptPolicyConcept<TlsAcceptPolicy<>, TlsSocketData<>>);
}

#endif
