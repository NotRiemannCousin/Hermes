#pragma once
#include <expected>
#include <vector>
#include <span>
#include <stdexec/execution.hpp>

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
        UnsupportedAddressFamily,

        RenegotiationRequired,

        NoScheduler
    };

    struct ConnectionErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override;

        [[nodiscard]] std::string message(int ev) const override;
    };

    std::error_code make_error_code(ConnectionErrorEnum e) noexcept;



    template<class T>
    using ConnectionResult = std::expected<T, ConnectionErrorEnum>;
    using ConnectionResultOper = ConnectionResult<std::monostate>;

    using StreamByteOper = std::pair<size_t, ConnectionResultOper>;

    struct TransferError {
        size_t bytesTransferred;
        ConnectionErrorEnum error;

        [[nodiscard]] TransferError Accumulate(TransferError other) const noexcept;
        [[nodiscard]] TransferError Substitute(ConnectionErrorEnum err) const noexcept;
    };
    // using TransferResult = std::expected<size_t, TransferError>;
    // // TODO: change all StreamByteOper occurrences to TransferResult
    // ? ok i think maybe not

#pragma region Async Definitions

    // template <typename T, typename Variant>
    // constexpr bool InVariant = false;
    //
    // template <typename T, typename... Types>
    // constexpr bool InVariant<T, std::variant<Types...>> = (std::same_as<T, Types> || ...);
    //
    // template <class Sender, class Error, class Env = stdexec::env<>>
    // concept SenderCanFailWithConcept =
    //     stdexec::sender_in<Sender, Env> &&
    //     InVariant<Error, stdexec::error_types_of_t<Sender, Env, std::variant>>;
    //
    // template<class Sender, class... Ts>
    // concept AsyncConnectionResultConcept =
    //     stdexec::sender_of<Sender, stdexec::set_value_t(Ts...)> &&
    //     SenderCanFailWithConcept<Sender, ConnectionErrorEnum>;
    //
    // template <class Sender>
    // concept AsyncConnectionResultOperConcept = AsyncConnectionResultConcept<Sender>;

    // template <class Sender>
    // concept AsyncTransferResultConcept =
    //     stdexec::sender_of<Sender, stdexec::set_value_t(size_t)> &&
    //     SenderCanFailWithConcept<Sender, TransferError>;


#pragma endregion
}

#include <Hermes/_base/ConnectionErrorEnum.tpp>

namespace Hermes {
    static_assert(std::formattable<ConnectionErrorEnum, char>);
}