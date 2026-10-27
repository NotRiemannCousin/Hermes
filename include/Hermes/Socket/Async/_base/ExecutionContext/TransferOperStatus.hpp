#pragma once
#include <Hermes/_base/OsApi/OsApi.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#include <MSWSock.h>
#else
#include <sys/socket.h>
#endif

namespace Hermes {
    //! @brief Context structure for asynchronous I/O completion callbacks.
    struct TransferOperStatus;

#ifdef _WIN32
    struct TransferOperStatus : WSAOVERLAPPED {
        using Operation = void(void* context, LongIoCount transferedBytes, bool success);

        void* context{};
        Operation* callback{};
    };
#else
    struct TransferOperStatus {
        using Operation = void(void* context, size_t transferedBytes, bool success);

        void* context{};
        Operation* callback{};
    };
#endif

    //! @brief Platform extension functions required to submit an Accept operation.
    //! @details Fetched once per listener by the accept policy on Windows (AcceptEx function pointers). Empty on
    //! Linux, where the kernel fills the caller-provided address storage directly.
#ifdef _WIN32
    struct AcceptExtensions {
        LPFN_ACCEPTEX lpfnAcceptEx{ nullptr };
        LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs{ nullptr };
    };
#else
    struct AcceptExtensions {
    };
#endif

    enum class IoOperation : std::uint8_t { Accept, Connect, Send, Recv };

    //! @brief Platform-neutral description of one asynchronous I/O operation.
    //! @details The single vocabulary an execution context must understand: policies fill the fields of the
    //! operation being submitted and hand the request to the context, which translates it to its native backend
    //! (IOCP, io_uring, epoll, kqueue, ...). Buffers and @p status must remain valid until the completion callback
    //! fires. On success the callback receives the operation-specific count: transferred bytes for Send/Recv and
    //! the accepted socket handle for Accept.
    struct IoRequest {
        IoOperation operation{};
        SocketFd socket{ macroINVALID_SOCKET };

        void* buffer{};
        std::size_t length{};

        const sockaddr* destination{};
        socklen_t destinationLength{};

        sockaddr* acceptAddress{};
        socklen_t* acceptAddressLength{};

        SocketFd acceptedSocket{ macroINVALID_SOCKET };
        const AcceptExtensions* acceptExtensions{};

        TransferOperStatus* status{};
    };

    //! @brief The contract every execution context used by the async policies must satisfy.
    //! @details RegisterHandle associates a socket with the context backend. SubmitIo enqueues one request and
    //! returns false only when nothing will complete (context stopped, backend saturated, immediate failure) — the
    //! caller then owns failing the operation. When it returns true the request's completion callback fires exactly
    //! once, possibly inline before SubmitIo returns. PostWork enqueues a bare completion for scheduler wake-ups.
    //! GetScheduler exposes the context to stdexec execution graphs.
    template<class T>
    concept ExecutionContextConcept = requires(const T& ctx, SocketHandle handle, TransferOperStatus* status, const IoRequest& request) {
        { ctx.RegisterHandle(handle) } -> std::convertible_to<bool>;
        { ctx.SubmitIo(request) } -> std::convertible_to<bool>;
        { ctx.PostWork(status) } -> std::convertible_to<bool>;
        { ctx.GetScheduler() };
    };
}