#pragma once
#include <Hermes/Config.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/TransferOperStatus.hpp>
#if HERMES_ENABLE_NATIVE_SCHEDULER

#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <thread>
#include <memory>
#include <stdexec/execution.hpp>

namespace Hermes {
    enum class ConnectionErrorEnum;

    class FastIoLoop;
    struct FastIoScheduler;
    struct FastIoScheduleSender;

    class FastIoLoop {
        struct Impl;
        std::unique_ptr<Impl> m_impl;

    public:
        explicit FastIoLoop(unsigned int threadCount = std::thread::hardware_concurrency());
        ~FastIoLoop();

        FastIoLoop(const FastIoLoop&) = delete;
        FastIoLoop& operator=(const FastIoLoop&) = delete;

        //! @brief Associates a socket handle with this loop's I/O backend.
        //! @details
        //! | Windows | attaches the handle to this IOCP loop (required before any overlapped I/O is issued on the socket) |
        //! | Linux   | io_uring binds the fd at SQE-preparation time, so this is a no-op kept for API symmetry            |
        //!
        //! Policies call it right after Connect/Accept on both platforms.
        bool RegisterHandle(SocketHandle handle) const noexcept;

        void Stop() noexcept;

        [[nodiscard]] FastIoScheduler GetScheduler() const noexcept;

        //! @brief Enqueues a bare completion for @p status (drives the scheduler's schedule()).
        bool PostWork(TransferOperStatus* status) const noexcept;

        //! @brief Translates one platform-neutral @p request into the native backend and enqueues it.
        //! @details The single submission path for every async policy on every platform; the context is obtained
        //! from SocketData::executionContext, never from a registry. Thread-safe. Returns false only when nothing
        //! will complete (loop stopped, ring full, immediate failure) and the caller must fail the operation
        //! itself; when it returns true the request's completion callback fires exactly once, possibly inline
        //! before this call returns.
        [[nodiscard]] bool SubmitIo(const IoRequest& request) const noexcept;

    private:
        void WorkerLoop() const noexcept;

#ifndef _WIN32
        template <typename F>
        bool SubmitSqe(F&& prep) const noexcept;
#endif
    };

    struct FastIoScheduler {
        using scheduler_concept = stdexec::scheduler_t;

        const FastIoLoop* m_loop;

        bool operator==(const FastIoScheduler& other) const noexcept = default;

        [[nodiscard]] FastIoScheduleSender schedule() const noexcept;
    };

    struct FastIoScheduleSender {
        using sender_concept = stdexec::sender_t;

        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_stopped_t()
        >;

        const FastIoLoop* m_loop;

        template <class Receiver>
        struct OperationState {
            const FastIoLoop* m_loop;
            Receiver m_receiver;
            TransferOperStatus m_status{};

            OperationState(const FastIoLoop* loop, Receiver r);

            void start() & noexcept;

            static void Callback(void* context, LongIoCount bytesTransferred, bool success);
        };

        template <class Receiver>
        OperationState<Receiver>
        connect(Receiver r) const noexcept;
    };

#if HERMES_ENABLE_TLS

    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = TlsSocketData<FastIoLoop>>
    struct TlsAsyncAcceptPolicy;
    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = TlsSocketData<FastIoLoop>>
    struct TlsAsyncConnectPolicy;
    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = TlsSocketData<FastIoLoop>>
    struct TlsAsyncTransferPolicy;

#endif

    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = DefaultSocketData<FastIoLoop>>
    struct DefaultAsyncAcceptPolicy;
    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = DefaultSocketData<FastIoLoop>>
    struct DefaultAsyncConnectPolicy;
    template<class ExecutionContext = FastIoLoop, SocketDataConcept Data = DefaultSocketData<FastIoLoop>>
    struct DefaultAsyncTransferPolicy;

    static_assert(ExecutionContextConcept<FastIoLoop>);
    static_assert(stdexec::scheduler<FastIoScheduler>);
    static_assert(stdexec::sender<FastIoScheduleSender>);
    static_assert(std::same_as<stdexec::schedule_result_t<FastIoScheduler>, FastIoScheduleSender>);
}

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.tpp>
#else

namespace Hermes {

#if HERMES_ENABLE_TLS

    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncAcceptPolicy;
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncConnectPolicy;
    template<class ExecutionContext, SocketDataConcept Data>
    struct TlsAsyncTransferPolicy;

#endif

    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncAcceptPolicy;
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncConnectPolicy;
    template<class ExecutionContext, SocketDataConcept Data>
    struct DefaultAsyncTransferPolicy;

}

#endif