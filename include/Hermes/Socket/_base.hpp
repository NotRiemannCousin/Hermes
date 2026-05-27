#pragma once
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>

#include <stdexec/execution.hpp>

#include <concepts>
#include <ranges>

namespace Hermes {
    namespace rg = std::ranges;

    template<class Type>
    concept ByteLike = std::same_as<Type, char> || std::same_as<Type, unsigned char> || std::same_as<Type, std::byte>;

    template<class R>
    concept ContiguousByteRange = std::ranges::contiguous_range<R>
        && ByteLike<std::ranges::range_value_t<R>>;

    template<class R>
    concept WritableContiguousByteRange = std::ranges::contiguous_range<R>
        && ByteLike<std::remove_const_t<std::ranges::range_value_t<R>>>;



    //! @brief With `All`, the entire span will be filled, with `Any` the function will stop at the block received.
    enum class RecvModeEnum : std::uint8_t {
        Any, All
    };

#pragma region SocketData

    //! @addtogroup SocketData
    //! @{

    //! @brief Concept for the context representing a socket's state and cross-policy coordination.
    //!
    //! @details A self-contained struct that aggregates the raw socket handle,
    //! its corresponding endpoint, static metadata (Family/Type), and any required
    //! cross-policy state (e.g., renegotiation callbacks). Enforces move-only
    //! semantics, as copying an underlying OS socket descriptor leads to double-close bugs.
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
    //! The underlying OS `SocketFd` handle.
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
            { data.socket   } -> std::same_as<SocketFd&>;

            []() constexpr { constexpr auto _ = SocketData::Family; }(); // forcing constexpr
            []() constexpr { constexpr auto _ = SocketData::Type;   }(); // forcing constexpr
        };

    //! @}

#pragma endregion


#pragma region Sync

    //! @addtogroup SyncSocketConcepts
    //! @{

#pragma region ConnectionPolicyConcept

    //! @brief Concept for a policy that manages the client-side connection lifecycle.
    //!
    //! @details Encompasses connect and close actions, operating on the socket's state
    //! (SocketData). It allows customizing behavior for blocking, non-blocking,
    //! timeouts, or performing client-side TLS handshakes.
    //!
    //! ### Configuration
    //! @code{.cpp} typename Policy::Options @endcode
    //! A struct defining settings to configure the socket (e.g., ONLY_IPV6, timeout, KEEP_ALIVE).
    //! These settings will be used to modify the Connect() behavior.
    //!
    //! ### Connection
    //! @code{.cpp} policy.Connect(data, opt) -> ConnectionResultOper @endcode
    //! Initiates a connection to a remote endpoint.
    //! Handles specific behaviors like client-side handshakes (e.g., TLS) or proxy negotiations.
    //!
    //! ### Cleanup
    //! @code{.cpp} policy.Close(data) -> void @endcode
    //! Gracefully closes the connection (e.g., sending TLS close alerts and cleanly shutting down).
    //!
    //! @code{.cpp} policy.Abort(data) -> void @endcode
    //! Terminates the connection abruptly.
    template<class Policy, class SocketData>
    concept ConnectionPolicyConcept = SocketDataConcept<SocketData>
        && Policy::Family == SocketData::Family
        && Policy::Type   == SocketData::Type

        && std::same_as<typename Policy::EndpointType, typename SocketData::EndpointType>

        && requires () { typename Policy::Options; }
        && requires (Policy policy, SocketData& data, typename Policy::Options opt) {
            { policy.Connect(data, opt) } -> std::same_as<ConnectionResultOper>;
            { policy.Close(data)        } -> std::same_as<void>;
            { policy.Abort(data)        } -> std::same_as<void>;
        };

#pragma endregion


#pragma region TransferPolicyConcept

    namespace _details {
        template<class Policy>
        using RecvStreamT = typename Policy::template RecvStream<std::byte>;
    }

    //! @brief Concept for a policy that manages data transfer operations.
    //!
    //! @details Defines how data is received and sent over the socket. It supports
    //! both modern, lazy, range-based reading and traditional buffer-based I/O.
    //! This allows encapsulation of specific protocols, such as reading length-prefixed
    //! messages, searching for delimiters, or handling TLS encryption transparently.
    //!
    //! ### Lazy Range-Based Receive
    //! @code{.cpp} _details::RecvStreamT<Policy, SocketData> @endcode
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
    template<class Policy, class SocketData>
    concept TransferPolicyConcept = SocketDataConcept<SocketData>
        && Policy::Type == SocketData::Type

        && requires(SocketData& data, Policy& policy) {
            { _details::RecvStreamT<Policy>{ data, policy } } -> std::ranges::input_range;
        }
        && requires(_details::RecvStreamT<Policy>& range) {
            { *range.begin() } -> std::same_as<std::byte>;
            { range.Error()  } -> std::same_as<ConnectionResultOper>;
        }
        && requires(
            Policy& policy     , SocketData& data,
            std::span<std::byte> bufferRecv, std::span<const std::byte> bufferSend,
            RecvModeEnum recvMode
        ) {
            { policy.Recv(data, bufferRecv, recvMode) } -> std::same_as<StreamByteOper>;
            { policy.Send(data, bufferSend)           } -> std::same_as<StreamByteOper>;
        };

#pragma endregion


#pragma region AcceptPolicyConcept


    //! @brief Concept for a policy that manages the server-side accept lifecycle.
    //!
    //! @details Separates the listening socket from the accepted socket, allowing
    //! custom accept flows (e.g., performing a TLS server handshake.
    //!
    //! ### Configuration
    //! @code{.cpp} typename Policy::Options @endcode
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
    template<class Policy, class SocketData>
    concept AcceptPolicyConcept = SocketDataConcept<SocketData>
        && Policy::Family == SocketData::Family
        && Policy::Type   == SocketData::Type

        && requires () { typename Policy::ListenOptions; }
        && requires () { typename Policy::AcceptOptions; }
        && requires(Policy policy, SocketData& data, int backlog, typename Policy::ListenOptions opt) {
            { policy.Listen(data, backlog, opt) } -> std::same_as<ConnectionResultOper>;
        }
        && requires(Policy policy, SocketData& data, typename Policy::AcceptOptions opt) {
            { policy.Accept(data, data, opt) } -> std::same_as<ConnectionResultOper>;
            { policy.Close(data)             } -> std::same_as<void>;
            { policy.Abort(data)             } -> std::same_as<void>;
        };

#pragma endregion


#pragma region Socket

    template<class SocketData, class ConnectionPolicy, class TransferPolicy>
    concept ClientSocketConcept = SocketDataConcept<SocketData> && ConnectionPolicyConcept<ConnectionPolicy, SocketData>
            && TransferPolicyConcept<TransferPolicy, SocketData>;

    template<class SocketData, class AcceptPolicy, class TransferPolicy>
    concept ServerSocketConcept = SocketDataConcept<SocketData> && AcceptPolicyConcept<AcceptPolicy, SocketData>
            && TransferPolicyConcept<TransferPolicy, SocketData>;

#pragma endregion

    //! @}

#pragma endregion


#pragma region Async

    //! @addtogroup AsyncSocketConcepts
//! @{

#pragma region AsyncConnectionPolicyConcept

    //! @brief Concept for a policy that manages the client-side connection lifecycle asynchronously.
    //!
    //! @details Async counterpart of `ConnectionPolicyConcept`. Connect and shutdown operations
    //! return senders instead of blocking, allowing integration with any `stdexec`-compatible
    //! scheduler. Cleanup operations (`Close`/`Abort`) remain synchronous, as resource
    //! release must be immediate and unconditional.
    //!
    //! ### Configuration
    //! @code{.cpp} typename Policy::Options @endcode
    //! A struct defining settings to configure the socket.
    //!
    //! ### Connection
    //! @code{.cpp} policy.Connect(data, opt) -> stdexec::sender @endcode
    //! Initiates a non-blocking connection. Delivers `ConnectionResultOper` through
    //! the sender's value channel on completion.
    //!
    //! ### Shutdown & Cleanup
    //! @code{.cpp} policy.Shutdown(data) -> stdexec::sender @endcode
    //! Performs graceful protocol shutdown via network I/O (e.g., sending TLS close alerts
    //! or TCP FIN).
    //!
    //! @code{.cpp} policy.Close(data) -> void @endcode
    //! Synchronously destroys the underlying handle and cancels any pending I/O.
    //! Does not block or perform network communication. Must be safe to call in destructors.
    //!
    //! @code{.cpp} policy.Abort(data) -> void @endcode
    //! Terminates the connection abruptly (e.g., triggering a TCP RST).
    //! Synchronous and immediate.
    template<class Policy, class SocketData>
    concept AsyncConnectionPolicyConcept = SocketDataConcept<SocketData>
        && Policy::Family == SocketData::Family
        && Policy::Type == SocketData::Type
    
        && std::same_as<typename Policy::EndpointType, typename SocketData::EndpointType>

        && requires () { typename Policy::Options; }
        && requires (Policy policy, SocketData& data, typename Policy::Options opt) {
            { opt.scheduler                 };

            { policy.Connect(data, opt) } -> stdexec::sender;// -> AsyncConnectionResultOperConcept;
            { policy.Shutdown(data)     } -> stdexec::sender;// -> AsyncConnectionResultOperConcept;
            { policy.Close(data)        } -> std::same_as<void>;
            { policy.Abort(data)        } -> std::same_as<void>;
        };

#pragma endregion


#pragma region AsyncTransferPolicyConcept

    //! @brief Concept for a policy that manages data transfer operations asynchronously.
    //!
    //! @details Async counterpart of `TransferPolicyConcept`. Send and receive operations
    //! return senders instead of blocking. We don't have `std::async_generator` neither
    //! async ranges support, so no stream alternative is proposed.
    //!
    //! ### Sender-Based Buffer I/O
    //! @code{.cpp} policy.Recv(data, bufferRecv) -> AsyncConnectionResultConcept @endcode
    //! Reads data from the socket into the provided `std::span<std::byte>` asynchronously.
    //! Delivers `StreamByteOper` through the sender's value channel on completion.
    //!
    //! @code{.cpp} policy.Send(data, bufferSend) -> AsyncConnectionResultConcept @endcode
    //! Writes data to the socket from the provided `std::span<const std::byte>` asynchronously.
    //! Delivers `StreamByteOper` through the sender's value channel on completion.
    template<class Policy, class SocketData>
    concept AsyncTransferPolicyConcept = SocketDataConcept<SocketData>
        && Policy::Type == SocketData::Type

        && requires(
            Policy& policy, SocketData& data, RecvModeEnum recvMode,
            std::span<std::byte> bufferRecv, std::span<const std::byte> bufferSend
        ) {
        { policy.Recv(data, bufferRecv, recvMode) } -> stdexec::sender;
        { policy.Send(data, bufferSend)           } -> stdexec::sender;
        };

#pragma endregion


#pragma region AsyncAcceptPolicyConcept

    //! @brief Concept for a policy that manages the server-side accept lifecycle asynchronously.
    //!
    //! @details Async counterpart of `AcceptPolicyConcept`. The accept operation returns a
    //! sender instead of blocking, enabling the server to process many concurrent connections
    //! without occupying a thread per accept call. `Listen()` intentionally remains synchronous,
    //! as `bind` and `listen` are non-blocking OS calls that complete instantly.
    //!
    //! ### Configuration
    //! @code{.cpp} typename Policy::ListenOptions @endcode
    //! A struct defining settings for the listening socket (e.g., REUSE_ADDRESS, buffer sizes).
    //! Used to modify `Listen()` behavior.
    //!
    //! @code{.cpp} typename Policy::AcceptOptions @endcode
    //! A struct defining per-connection settings (e.g., TCP_NODELAY, handshake timeout).
    //! Must also expose a `scheduler` field to select the execution context for `Accept()`.
    //!
    //! ### Initialization
    //! @code{.cpp} policy.Listen(data, backlog, opt) -> ConnectionResultOper @endcode
    //! Binds and begins listening. Synchronous — `bind` and `listen` are non-blocking OS calls
    //! that complete immediately and must not be deferred to a sender.
    //!
    //! ### Connection
    //! @code{.cpp} policy.Accept(data, acceptedData, opt) -> stdexec::sender @endcode
    //! Waits for and accepts an incoming connection without blocking. `acceptedData` is a child
    //! model (via `MakeChild()`) whose `socket` and `endpoint` will be filled on completion.
    //! Handles specific behaviors like server-side TLS handshakes or versioning.
    //! Delivers `ConnectionResultOper` through the sender's value channel on completion.
    //!
    //! ### Cleanup
    //! @code{.cpp} policy.Close(data) -> void @endcode
    //! Stops listening and gracefully closes the server socket.
    //! Synchronous — must not schedule work or block on a sender.
    //!
    //! @code{.cpp} policy.Abort(data) -> void @endcode
    //! Terminates an accepted connection abruptly.
    //! Synchronous — must not schedule work or block on a sender.
    template<class Policy, class SocketData>
    concept AsyncAcceptPolicyConcept = SocketDataConcept<SocketData>
        && Policy::Family == SocketData::Family
        && Policy::Type   == SocketData::Type

        && requires () { typename Policy::ListenOptions; }
        && requires () { typename Policy::AcceptOptions; }
        && requires(Policy policy, SocketData& data, int backlog, typename Policy::ListenOptions opt) {
            { policy.Listen(data, backlog, opt) } -> std::same_as<ConnectionResultOper>;
        }
        && requires(Policy policy, SocketData& data, typename Policy::AcceptOptions opt) {
            { opt.scheduler            };

            { policy.Accept(data, opt) } -> stdexec::sender;// -> AsyncConnectionResultOperConcept;
            { policy.Shutdown(data)    } -> stdexec::sender;// -> AsyncConnectionResultOperConcept;
            { policy.Close(data)       } -> std::same_as<void>;
            { policy.Abort(data)       } -> std::same_as<void>;
        };

#pragma endregion


#pragma region Async Socket

    template<class SocketData, class ConnectionPolicy, class TransferPolicy>
    concept AsyncClientSocketConcept = SocketDataConcept<SocketData> && AsyncConnectionPolicyConcept<ConnectionPolicy, SocketData>
            && AsyncTransferPolicyConcept<TransferPolicy, SocketData>;

    template<class SocketData, class AcceptPolicy, class TransferPolicy>
    concept AsyncServerSocketConcept = SocketDataConcept<SocketData> && AsyncAcceptPolicyConcept<AcceptPolicy, SocketData>
            && AsyncTransferPolicyConcept<TransferPolicy, SocketData>;

#pragma endregion

    //! @}

#pragma endregion


    namespace _details {
        inline void SetTimeout(const SocketFd socket, const int time) {
#if defined(_WIN32) || defined(_WIN64)
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&time), sizeof(time));
            setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&time), sizeof(time));
#else
            struct timeval tv;
            tv.tv_sec = time / 1000;
            tv.tv_usec = (time % 1000) * 1000;
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
            setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
        }
    }

}