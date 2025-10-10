#pragma once
#include <experimental/generator>
#include <expected>
#include <vector>
#include <string>
#include <span>

namespace Hermes {
    using ByteData     = std::vector<std::byte>;
    using ByteDataSpan = std::span<std::byte>;

    enum class ConnectionErrorEnum {
        UNKNOWN,

        INVALID_ROLE,

        INVALID_ENDPOINT,
        ADDRESS_IN_USE,

        SOCKET_NOT_OPEN,
        CONNECTION_FAILED,
        CONNECTION_CLOSED,
        INTERRUPTED,

        CONNECTION_TIMEOUT,
        SEND_TIMEOUT,
        RECEIVE_TIMEOUT,

        LISTEN_FAILED,

        SEND_FAILED,

        RECEIVE_FAILED,

        HANDSHAKE_FAILED,
        CERTIFICATE_ERROR,
        ENCRYPTION_FAILED,
        DECRYPTION_FAILED,
        INVALID_SECURITY_CONTEXT,

        INCOMPLETE_MESSAGE,


        RESOLVE_HOST_NOT_FOUND,
        RESOLVE_SERVICE_NOT_FOUND,
        RESOLVE_TEMPORARY_FAILURE,
        RESOLVE_FAILED,
        RESOLVE_NO_ADDRESS_FOUND,
        UNSUPPORTED_ADDRESS_FAMILY
    };

    template<class T>
    using ConnectionResult = std::expected<T, ConnectionErrorEnum>;
    using ConnectionResultOper = ConnectionResult<std::monostate>;


    using StreamByteCount = ConnectionResult<size_t>;
    using DataStream = std::experimental::generator<ConnectionResult<ByteData>>;
    using DataStringStream = std::experimental::generator<ConnectionResult<std::string>>;
}