#pragma once
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

#include <concepts>
#include <ranges>

namespace Hermes {
    namespace rg = std::ranges;

    template<class Type>
    concept ByteLike = sizeof(Type) == 1;


    //! @brief Concept for the data representing a socket's state.
    //!
    //! @details A struct satisfying this concept is self-contained, exposing its
    //! associated Endpoint type and containing the socket handle and an endpoint instance.
    template<class SocketData>
    concept SocketDataConcept = EndpointConcept<typename SocketData::EndpointType>
        && requires(SocketData data) {
            { SocketData::Family } -> std::same_as<const AddressFamilyEnum&>;
            { data.endpoint } -> std::same_as<typename SocketData::EndpointType&>;
            { data.socket   } -> std::same_as<SOCKET&>;

            []() constexpr { constexpr auto _ = SocketData::Family; }(); // forcing constexpr
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
            { policy.Connect(data) } -> std::same_as<ConnectionResultOper>;
            { policy.Close(data)   } -> std::same_as<ConnectionResultOper>;
        };


    //! @brief Concept for a range that receives data from a socket.
    //!
    //! @details This range lazily reads bytes from the socket upon iteration.
    //! The range itself must provide an `Error()` method to check the status
    //! of the stream.
    template<class Range, class SocketData>
    concept RecvRangeConcept = SocketDataConcept<SocketData>
        && std::constructible_from<Range, SocketData&>
        && std::ranges::input_range<Range>
        && std::same_as<std::ranges::range_value_t<Range>, std::byte>
        && requires(const Range& range) {
            { range.Error() } -> std::same_as<ConnectionResultOper>;
        };


    //! @brief Concept for a data transfer policy.
    //!
    //! @details Defines how data is received/sent in this socket.
    //! (e.g., reading a length prefix, searching for a delimiter, TLS, etc.).
    template<class Policy, class SocketData>
    concept TransferPolicyConcept = SocketDataConcept<SocketData>
        && RecvRangeConcept<typename Policy::RecvRange, SocketData>
        && requires(Policy policy, SocketData& data, std::span<std::byte> bufferRecv, std::span<const std::byte> bufferSend) {
            { policy.Recv(data, bufferRecv) } -> std::same_as<ConnectionResultOper>;
            { policy.Send(data, bufferSend) } -> std::same_as<ConnectionResultOper>;
        };
}