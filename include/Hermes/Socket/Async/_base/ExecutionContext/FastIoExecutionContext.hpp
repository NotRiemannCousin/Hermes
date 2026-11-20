#pragma once
#include <Hermes/Config.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/TransferOperStatus.hpp>
#include <Hermes/Socket/_base.hpp>
#if HERMES_ENABLE_NATIVE_SCHEDULER

#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/_base/Transfer/TransferIo.hpp>
#include <chrono>
#include <thread>
#include <memory>
#include <stdexec/execution.hpp>
#include <exec/timed_scheduler.hpp>

namespace Hermes {
    enum class ConnectionErrorEnum;

    class FastIoLoop;
    struct FastIoScheduler;
    struct FastIoScheduleSender;
    struct FastIoScheduleAfterSender;

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

        //! @brief Enqueues a cancellation operation.
        //! @details Asks the backend to abort the request with the given status — the pointer handed to SubmitIo
        //! as IoRequest::status, which io_uring uses as user_data and IOCP as the overlapped. Returns true when
        //! the cancellation was requested or nothing was pending to cancel; either way the operation's callback
        //! fires exactly once more, with success=false when aborted, or with the original result if the operation
        //! had already completed. Implementing it makes the context satisfy details_::CancelIoCapableContext and
        //! enables stop-token cancellation (set_stopped) in the async policies. Thread-safe.
        [[nodiscard]] bool CancelIo(SocketHandle handle, const TransferOperStatus* status) const noexcept;

        //! @brief Arms one timed completion for @p status to fire at the absolute @p deadline.
        //! @details Schedules `status` with a fixed time. Uses IORING_OP_TIMEOUT SQE on linux and a custom min-heap on
        //! Windows (IOCP doesn't have timed events, libuv and ASIO does that).
        [[nodiscard]] bool ScheduleTimedOp(TransferOperStatus* status, DeadlineTimepoint deadline) const noexcept;

        //! @brief Cancels the entry of @p status.
        [[nodiscard]] bool CancelTimedOp(TransferOperStatus* status) const noexcept;

    private:
        void WorkerLoop() const noexcept;

#ifndef _WIN32
        template <class F>
        bool SubmitSqe(F&& prep) const noexcept;
#else
        void ProcessExpiredTimers() const noexcept;
#endif
    };

    struct FastIoScheduler {
        using scheduler_concept = stdexec::scheduler_t;

        using time_point = DeadlineTimepoint;
        using duration   = DeadlineDuration;

        const FastIoLoop* m_loop;

        bool operator==(const FastIoScheduler& other) const noexcept = default;

        [[nodiscard]] FastIoScheduleSender schedule() const noexcept;

        [[nodiscard]] time_point now() const noexcept;

        [[nodiscard]] FastIoScheduleAfterSender schedule_after(duration d) const noexcept;

        template <class Clock, class Duration>
        [[nodiscard]] FastIoScheduleAfterSender
        schedule_at(const std::chrono::time_point<Clock, Duration>& when) const noexcept;
    };

    struct FastIoScheduleEnv {
        [[nodiscard]] FastIoScheduler query(stdexec::get_completion_scheduler_t<stdexec::set_value_t>) const noexcept;

        FastIoScheduler m_scheduler;
    };

    struct FastIoScheduleSender {
        using sender_concept = stdexec::sender_t;

        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum)
        >;

        const FastIoLoop* m_loop;

        [[nodiscard]] FastIoScheduleEnv get_env() const noexcept;

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
        OperationState<Receiver> connect(Receiver r) const noexcept;
    };

    struct FastIoScheduleAfterSender {
        using sender_concept = stdexec::sender_t;

        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(ConnectionErrorEnum),
            stdexec::set_stopped_t()
        >;

        const FastIoLoop* m_loop;
        std::chrono::steady_clock::duration m_duration;

        [[nodiscard]] FastIoScheduleEnv get_env() const noexcept;


        template <class Receiver>
        struct OperationState {
            using StopToken = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver&>>;

            struct CancelHandler {
                OperationState* m_self;

                void operator()() const noexcept { m_self->CancelPath(); }
            };

            using StopCallback = stdexec::stop_callback_for_t<StopToken, CancelHandler>;

            const FastIoLoop* m_loop;
            Receiver m_receiver;
            TransferOperStatus m_status{};
            DeadlineTimepoint m_deadline{};
            std::atomic<bool> m_cancelRequested{ false };
            std::optional<StopCallback> m_stopCb{};

            OperationState(const FastIoLoop* loop, std::chrono::steady_clock::duration duration, Receiver r);

            void start() & noexcept;

            void CancelPath() noexcept;

            static void Callback(void* context, LongIoCount bytesTransferred, bool success);
        };

        template <class Receiver>
        OperationState<Receiver> connect(Receiver r) const noexcept;
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
    static_assert(exec::timed_scheduler<FastIoScheduler, DeadlineClock>);
    static_assert(stdexec::sender<FastIoScheduleSender>);
    static_assert(stdexec::sender<FastIoScheduleAfterSender>);
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
