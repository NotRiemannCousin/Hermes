#pragma once
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

#include <concepts>
#include <ranges>

namespace Hermes {
    namespace rg = std::ranges;

    template<class Type>
    concept ByteLike = std::same_as<Type, char> || std::same_as<Type, unsigned char> || std::same_as<Type, std::byte>;


    //! @brief Concept for the data representing a socket's state.
    //!
    //! @details A self-contained struct that aggregates the raw socket handle,
    //! its corresponding endpoint, and static metadata (Family/Type). Enforces
    //! move-only semantics, as copying an underlying OS socket descriptor leads
    //! to double-close bugs and state corruption.
    //!
    //! ### Type Requirements
    //! - Must define `EndpointType` which satisfies `EndpointConcept`.
    //! - Must be `std::movable` and strictly **not** `std::copyable`.
    //!
    //! ### Static Metadata
    //! @code{.cpp} SocketData::Family @endcode
    //! A `constexpr AddressFamilyEnum` representing the address family (e.g., IPv4, IPv6).
    //!
    //! @code{.cpp} SocketData::Type @endcode
    //! A `constexpr SocketTypeEnum` representing the socket type (e.g., Stream, Datagram).
    //!
    //! ### Instance Members
    //! @code{.cpp} data.socket @endcode
    //! The underlying OS `SOCKET` handle.
    //!
    //! @code{.cpp} data.endpoint @endcode
    //! The instance of `EndpointType` bound or connected to this socket.
    template<class SocketData>
    concept SocketDataConcept = EndpointConcept<typename SocketData::EndpointType>
        && std::movable<SocketData> && !std::copyable<SocketData> // move-only, copying a socket is a bad idea
        && requires(SocketData data) {
            { SocketData::Family } -> std::same_as<const AddressFamilyEnum&>;
            { SocketData::Type   } -> std::same_as<const SocketTypeEnum&>;

            { data.endpoint } -> std::same_as<typename SocketData::EndpointType&>;
            { data.socket   } -> std::same_as<SOCKET&>;

            []() constexpr { constexpr auto _ = SocketData::Family; }(); // forcing constexpr
            []() constexpr { constexpr auto _ = SocketData::Type;   }(); // forcing constexpr
        };


    //! @brief Concept for a policy that manages the client-side connection lifecycle.
    //!
    //! @details Encompasses connect and close actions, operating on the socket's state
    //! (SocketData). It allows customizing behavior for blocking, non-blocking,
    //! timeouts, or performing client-side TLS handshakes.
    //!
    //! ### Configuration
    //! @code{.cpp} typename Policy<SocketData>::Options @endcode
    //! A struct defining settings to configure the socket (e.g., ONLY_IPV6, timeout, KEEP_ALIVE).
    //! These settings will be used to modify the Connect() behavior.
    //!
    //! ### Connection
    //! @code{.cpp} policy.Connect(data) -> ConnectionResultOper @endcode
    //! Initiates a connection to a remote endpoint.
    //! Handles specific behaviors like client-side handshakes (e.g., TLS) or proxy negotiations.
    //!
    //! ### Cleanup
    //! @code{.cpp} policy.Close(data) -> void @endcode
    //! Gracefully closes the connection (e.g., sending TLS close alerts and cleanly shutting down).
    //!
    //! @code{.cpp} policy.Abort(data) -> void @endcode
    //! Terminates the connection abruptly.
    template<template <class> class Policy, class SocketData>
    concept ConnectionPolicyConcept = SocketDataConcept<SocketData>
        && requires () { typename Policy<SocketData>::Options; }
        && requires (Policy<SocketData> policy, SocketData data, typename Policy<SocketData>::Options opt) {
            { policy.Connect(data, opt) } -> std::same_as<ConnectionResultOper>;
            { policy.Close(data)        } -> std::same_as<void>;
            { policy.Abort(data)        } -> std::same_as<void>;
        };

    namespace _details {
        template<class Policy>
        using RecvLazyRangeT = typename Policy::template RecvLazyRange<std::byte>;
    }

    //! @brief Concept for a policy that manages data transfer operations.
    //!
    //! @details Defines how data is received and sent over the socket. It supports
    //! both modern, lazy, range-based reading and traditional buffer-based I/O.
    //! This allows encapsulation of specific protocols, such as reading length-prefixed
    //! messages, searching for delimiters, or handling TLS encryption transparently.
    //!
    //! ### Lazy Range-Based Receive
    //! @code{.cpp} _details::RecvLazyRangeT<Policy<SocketData>> @endcode
    //! Must be constructible from `SocketData` and `Policy`, and satisfy `std::ranges::input_range`.
    //! It iteratively yields `std::byte` elements.
    //!
    //! @code{.cpp} range.Error() -> ConnectionResultOper @endcode
    //! A custom method on the range to check for underlying socket or protocol errors
    //! that may occur during the iteration process.
    //!
    //! ### Buffer-Based I/O
    //! @code{.cpp} policy.Recv(data, bufferRecv) -> StreamByteOper @endcode
    //! Reads data from the socket directly into the provided `std::span<std::byte>`.
    //!
    //! @code{.cpp} policy.Send(data, bufferSend) -> StreamByteOper @endcode
    //! Writes data to the socket directly from the provided `std::span<const std::byte>`.
    template<template <class> class Policy, class SocketData>
    concept TransferPolicyConcept = SocketDataConcept<SocketData>
        && requires(SocketData& data, Policy<SocketData>& policy) {
            { _details::RecvLazyRangeT<Policy<SocketData>>{ data, policy } } -> std::ranges::input_range;
        }
        && requires(_details::RecvLazyRangeT<Policy<SocketData>>& range) {
            { *range.begin() } -> std::same_as<std::byte>;
            { range.Error()  } -> std::same_as<ConnectionResultOper>;
        }
        && requires(
            Policy<SocketData>& policy     , SocketData& data,
            std::span<std::byte> bufferRecv, std::span<const std::byte> bufferSend
        ) {
            { policy.Recv(data, bufferRecv) } -> std::same_as<StreamByteOper>;
            { policy.Send(data, bufferSend) } -> std::same_as<StreamByteOper>;
        };


    //! @brief Concept for a policy that manages the server-side accept lifecycle.
    //!
    //! @details Separates the listening socket from the accepted socket, allowing
    //! custom accept flows (e.g., performing a TLS server handshake.
    //!
    //! ### Configuration
    //! @code{.cpp} typename Policy<SocketData>::Options @endcode
    //! A struct defining settings to configure the socket (e.g., ONLY_IPV6, timeout, KEEP_ALIVE).
    //! This settings will be used to modify Listen() behavior.
    //!
    //! ### Initialization
    //! @code{.cpp} policy.Listen(data, backlog, opt) -> ConnectionResultOper @endcode
    //! Binds and listens.
    //!
    //! ### Connection
    //! @code{.cpp} policy.Accept(data, acceptedData) -> ConnectionResultOper @endcode
    //! Accepts a connection. `acceptedData` it's a model used to construct the new socket returned
    //! (via `MakeChild()`). `acceptedData.socket` and `endpoint` will be filled.
    //! Handles specific behaviors like server-side handshakes or versioning.
    //!
    //! ### Cleanup
    //! @code{.cpp} policy.Close(data) -> void @endcode
    //! Stops listening and (gracefuly) closes the server socket.
    //!
    //! @code{.cpp} policy.Abort(data) -> void @endcode
    //! Terminates abruptly an accepted connection.
    template<template <class> class Policy, class SocketData>
    concept AcceptPolicyConcept = SocketDataConcept<SocketData>
        && requires () { typename Policy<SocketData>::ListenOptions; }
        && requires () { typename Policy<SocketData>::AcceptOptions; }
        && requires(Policy<SocketData> policy, SocketData data, int backlog, typename Policy<SocketData>::ListenOptions opt) {
            { policy.Listen(data, backlog, opt) } -> std::same_as<ConnectionResultOper>;
        }
        && requires(Policy<SocketData> policy, SocketData data, int backlog, typename Policy<SocketData>::AcceptOptions opt) {
            { policy.Accept(data, data, opt) } -> std::same_as<ConnectionResultOper>;
        }
        && requires(Policy<SocketData> policy, SocketData data) {
            { policy.Close(data) } -> std::same_as<void>;
            { policy.Abort(data) } -> std::same_as<void>;
        };
}