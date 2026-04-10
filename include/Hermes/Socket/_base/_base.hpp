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
    //! @details A struct satisfying this concept is self-contained, exposing its
    //! associated Endpoint type and containing the socket handle and an endpoint instance.
    template<class SocketData>
    concept SocketDataConcept = EndpointConcept<typename SocketData::EndpointType>
        && std::movable<SocketData>
        && !std::copyable<SocketData> // coping a socket => 💀
        && requires(SocketData data) {
            { SocketData::Family } -> std::same_as<const AddressFamilyEnum&>;
            { SocketData::Type } -> std::same_as<const SocketTypeEnum&>;

            { data.endpoint } -> std::same_as<typename SocketData::EndpointType&>;
            { data.socket   } -> std::same_as<SOCKET&>;

            []() constexpr { constexpr auto _ = SocketData::Family; }(); // forcing constexpr
            []() constexpr { constexpr auto _ = SocketData::Type;   }(); // forcing constexpr
        };


    //! @brief Concept for a policy that manages the connection lifecycle.
    //!
    //! @details Encompasses connect and close actions, operating on the
    //! socket's state (SocketData). It allows customizing behavior for
    //! blocking, non-blocking, timeout, TLS operations, etc.
    template<template <class> class Policy, class SocketData>
    concept ConnectionPolicyConcept = SocketDataConcept<SocketData>
        && requires (Policy<SocketData> policy, SocketData data) {
            { policy.Connect(data) } -> std::same_as<ConnectionResultOper>;
            { policy.Close(data)   } -> std::same_as<void>;
            { policy.Abort(data)   } -> std::same_as<void>;
        };


    //! @brief Concept for a data transfer policy.
    //!
    //! @details Defines how data is received/sent in this socket.
    //! (e.g., reading a length prefix, searching for a delimiter, TLS, etc.).
    template<template <class> class Policy, class SocketData>
    concept TransferPolicyConcept = SocketDataConcept<SocketData>
        && std::ranges::input_range<typename Policy<SocketData>::template RecvLazyRange<std::byte>>
        && std::same_as<std::ranges::range_value_t<typename Policy<SocketData>::template RecvLazyRange<std::byte>>, std::byte>
        && std::constructible_from<typename Policy<SocketData>::template RecvLazyRange<std::byte>, SocketData&, Policy<SocketData>&>
        && requires(typename Policy<SocketData>::template RecvLazyRange<std::byte>& range) {
             { range.Error() } -> std::same_as<ConnectionResultOper>;
        }
        && requires(
            Policy<SocketData>& policy,
            SocketData& data,
            std::span<std::byte> bufferRecv,
            std::span<const std::byte> bufferSend
        ) {
            { policy.Recv(data, bufferRecv) } -> std::same_as<StreamByteOper>;
            { policy.Send(data, bufferSend) } -> std::same_as<StreamByteOper>;
        };


    //! @brief Concept for a policy that manages the server-side accept lifecycle.
    //!
    //! @details Separates the listening socket (bind/listen) from the accepted socket,
    //! allowing customization of the accept flow — e.g. performing a TLS server
    //! handshake after accept(). Both the listening socket and each accepted connection
    //! are represented by SocketData; the policy distinguishes them via Close
    //! vs Close (no TLS alert needed on the listening socket).
    template<template <class> class Policy, class SocketData>
    concept AcceptPolicyConcept = SocketDataConcept<SocketData>
        && requires(Policy<SocketData> policy, SocketData listenData, SocketData acceptedData, int backlog) {
            //! bind() + listen(); on success, listenData.socket is the listening socket.
            { policy.Listen(listenData, backlog)       } -> std::same_as<ConnectionResultOper>;
            //! accept(); on success, acceptedData.socket and acceptedData.endpoint are filled.
            //! For TLS, also performs the server-side handshake.
            { policy.Accept(listenData, acceptedData)  } -> std::same_as<ConnectionResultOper>;
            //! Closes the listening socket (no protocol-level shutdown needed).
            { policy.Close(listenData)                 } -> std::same_as<void>;
            //! Closes an accepted connection (sends TLS shutdown alert if applicable).
            { policy.Close(acceptedData)               } -> std::same_as<void>;
            { policy.Abort(acceptedData)               } -> std::same_as<void>;
        };
}