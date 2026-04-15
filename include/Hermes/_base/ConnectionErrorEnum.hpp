#pragma once
#include <expected>
#include <vector>
#include <span>

namespace Hermes {
    using ByteData     = std::vector<std::byte>;
    using ByteDataSpan = std::span<std::byte>;

    enum class ConnectionErrorEnum {
        Unknown,

        InvalidRole,

        InvalidEndpoint,
        AddressInUse,

        SocketNotOpen,
        ConnectionFailed,
        ConnectionClosed,
        Interrupted,

        ConnectionTimeout,
        SendTimeout,
        ReceiveTimeout,

        ListenFailed,

        SendFailed,

        ReceiveFailed,

        HandshakeFailed,
        CertificateError,
        EncryptionFailed,
        DecryptionFailed,
        InvalidSecurityContext,

        IncompleteMessage,


        ResolveHostNotFound,
        ResolveServiceNotFound,
        ResolveTemporaryFailure,
        ResolveFailed,
        ResolveNoAddressFound,
        UnsupportedAddressFamily
    };

    template<class T>
    using ConnectionResult = std::expected<T, ConnectionErrorEnum>;
    using ConnectionResultOper = ConnectionResult<std::monostate>;


    using StreamByteOper = std::pair<size_t, ConnectionResultOper>;
}

#include <Hermes\_base\ConnectionErrorEnum.tpp>

namespace Hermes {
    static_assert(std::formattable<ConnectionErrorEnum, char>);
}