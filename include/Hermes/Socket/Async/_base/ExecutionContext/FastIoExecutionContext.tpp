#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_NATIVE_SCHEDULER

#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <stdexcept>
#include <format>
#include <print>
#include <vector>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <mutex>
#include <utility>

#ifndef _WIN32
#include <liburing.h>
#include <queue>
#include <condition_variable>
#endif

namespace Hermes {
#ifdef _WIN32
    struct FastIoLoop::Impl {
        SocketHandle m_iocpHandle{ nullptr };
        std::vector<std::jthread> m_workers;
        std::atomic<bool> m_isRunning{ false };

        // (libuv/ASIO pattern for the timed scheduler, io_uring has support to it but IOCP don't)
        struct TimedEntry {
            TransferOperStatus*                   status;
            std::chrono::steady_clock::time_point deadline;
            std::uint64_t                         sequence;
        };

        struct TimedEntryGreater {
            bool operator()(const TimedEntry& lhs, const TimedEntry& rhs) const noexcept {
                if (lhs.deadline != rhs.deadline)
                    return lhs.deadline > rhs.deadline;
                return lhs.sequence > rhs.sequence;
            }
        };

        bool PushTimed(TransferOperStatus* status, const std::chrono::steady_clock::time_point deadline,
                       bool& wake) noexcept {
            std::lock_guard lock{ m_timerMutex };
            if (!m_isRunning.load(std::memory_order_acquire))
                return false;

            wake = m_timers.empty() || deadline < m_timers.front().deadline;
            m_timers.emplace_back(status, deadline, m_nextTimedSequence++);
            std::ranges::push_heap(m_timers, TimedEntryGreater{});
            return true;
        }

        bool RemoveTimed(const TransferOperStatus* status) noexcept {
            std::lock_guard lock{ m_timerMutex };
            for (auto entry{ m_timers.begin() }; entry != m_timers.end(); ++entry) {
                if (entry->status == status) {
                    m_timers.erase(entry);
                    std::ranges::make_heap(m_timers, TimedEntryGreater{});
                    return true;
                }
            }
            return false;
        }

        void PopDueTo(std::vector<TransferOperStatus*>& due) noexcept {
            const auto now{ std::chrono::steady_clock::now() };
            std::lock_guard lock{ m_timerMutex };
            while (!m_timers.empty() && m_timers.front().deadline <= now) {
                std::ranges::pop_heap(m_timers, TimedEntryGreater{});
                due.push_back(m_timers.back().status);
                m_timers.pop_back();
            }
        }

        DWORD TimedWaitMs() noexcept {
            const std::lock_guard lock{ m_timerMutex };
            if (m_timers.empty())
                return INFINITE;

            const auto remaining{ m_timers.front().deadline - std::chrono::steady_clock::now() };
            if (remaining <= std::chrono::steady_clock::duration::zero())
                return 0;

            constexpr long long maxWait{ static_cast<long long>(INFINITE) - 1 };
            const long long waitMs{ std::chrono::ceil<std::chrono::milliseconds>(remaining).count() };
            return waitMs >= maxWait ? static_cast<DWORD>(maxWait) : static_cast<DWORD>(waitMs);
        }

        std::mutex               m_timerMutex{};
        std::vector<TimedEntry>  m_timers{};
        std::uint64_t            m_nextTimedSequence{};
    };
#else
    struct FastIoLoop::Impl {
        io_uring m_ring{};
        std::mutex m_ringMutex{};
        std::vector<std::jthread> m_workers;
        std::atomic<bool> m_isRunning{ false };

        struct WorkItem {
            void* context;
            TransferOperStatus::Operation* callback;
            size_t bytesTransferred;
            bool success;
        };
        std::queue<WorkItem> m_workQueue;
        std::mutex m_workMutex;
        std::condition_variable m_workCv;

        bool m_kernelTimers{ false };
    };
#endif

    inline FastIoLoop::FastIoLoop(const unsigned int threadCount) : m_impl{ std::make_unique<Impl>() } {
        const unsigned int workerCount{ std::max(1u, threadCount) };

#ifdef _WIN32
        m_impl->m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, workerCount);
        if (!m_impl->m_iocpHandle)
            throw std::runtime_error{ std::format("Not able to create IOCP. GetLastError: {}", GetLastError()) };

        m_impl->m_isRunning.store(true, std::memory_order_release);

        m_impl->m_workers.reserve(workerCount);
        for (unsigned int i{}; i < workerCount; ++i) {
            m_impl->m_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#else
        if (io_uring_queue_init(1024, &m_impl->m_ring, 0) < 0) {
            throw std::runtime_error("Not able to create io_uring");
        }

        if (struct io_uring_probe* probe{ io_uring_get_probe_ring(&m_impl->m_ring) }) {
            m_impl->m_kernelTimers = io_uring_opcode_supported(probe, IORING_OP_TIMEOUT)
                                  && io_uring_opcode_supported(probe, IORING_OP_ASYNC_CANCEL);
            io_uring_free_probe(probe);
        }

        m_impl->m_isRunning.store(true, std::memory_order_release);

        m_impl->m_workers.reserve(workerCount);

        m_impl->m_workers.emplace_back([this, workerCount] {
            while (m_impl->m_isRunning.load(std::memory_order_relaxed)) {
                struct io_uring_cqe* cqe;
                const int ret{ io_uring_wait_cqe(&m_impl->m_ring, &cqe) };
                if (ret < 0) continue;
                if (!cqe) continue;

                auto* status      { static_cast<TransferOperStatus*>(io_uring_cqe_get_data(cqe)) };
                const size_t bytes{ cqe->res >= 0 ? static_cast<size_t>(cqe->res) : 0 };
                const bool success{ cqe->res >= 0 };
                io_uring_cqe_seen(&m_impl->m_ring, cqe);

                if (!status) continue;

                if (workerCount > 1) {
                    {
                        std::lock_guard lock(m_impl->m_workMutex);
                        m_impl->m_workQueue.push({ status->context, status->callback, bytes, success });
                    }
                    m_impl->m_workCv.notify_one();
                } else {
                    if (status->callback) {
                        try {
                            status->callback(status->context, bytes, success);
                        } catch (const std::exception& e) {
                            std::println(stderr, "Exception in FastIoLoop Worker: {}", e.what());
                        } catch (...) {
                            std::println(stderr, "Unknown exception in FastIoLoop Worker");
                        }
                    }
                }
            }
        });

        for (unsigned int i{ 1 }; i < workerCount; ++i) {
            m_impl->m_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#endif
    }

    inline FastIoLoop::~FastIoLoop() {
        Stop();
    }

#ifndef _WIN32
    template <class F>
    inline bool FastIoLoop::SubmitSqe(F&& prep) const noexcept {
        std::lock_guard lock(m_impl->m_ringMutex);

        if (!m_impl->m_isRunning.load(std::memory_order_acquire))
            return false;

        if (struct io_uring_sqe* sqe{ io_uring_get_sqe(&m_impl->m_ring) }) {
            prep(sqe);
            return io_uring_submit(&m_impl->m_ring) >= 0;
        }
        return false;
    }

    inline bool FastIoLoop::SubmitIo(const IoRequest& request) const noexcept {
        return SubmitSqe([&](struct io_uring_sqe* sqe) {
            const int fd{ static_cast<int>(request.socket) };

            switch (request.operation) {
                case IoOperation::Accept:
                    io_uring_prep_accept(sqe, fd, request.acceptAddress, request.acceptAddressLength, 0);
                    break;
                case IoOperation::Connect:
                    io_uring_prep_connect(sqe, fd, request.destination, request.destinationLength);
                    break;
                case IoOperation::Send:
                    io_uring_prep_send(sqe, fd, request.buffer, request.length, MSG_NOSIGNAL);
                    break;
                case IoOperation::Recv:
                    io_uring_prep_recv(sqe, fd, request.buffer, request.length, 0);
                    break;
            }

            io_uring_sqe_set_data(sqe, request.status);
        });
    }

    [[nodiscard]] inline bool FastIoLoop::ScheduleTimedOp(TransferOperStatus* status,
                                                          const DeadlineTimepoint deadline) const noexcept {
        if (status == nullptr)
            return false;

        if (!m_impl->m_kernelTimers)
            return false;

        auto remaining{ std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - std::chrono::steady_clock::now()) };
        auto secs{ std::chrono::duration_cast<std::chrono::seconds>(remaining) };
        remaining -= secs;
        secs      = std::max(secs, std::chrono::seconds{ 0 });
        remaining = std::clamp(remaining, std::chrono::nanoseconds{ 0 }, std::chrono::nanoseconds{ 999'999'999 });

        const struct __kernel_timespec dueTime{ secs.count(), remaining.count() };

        return SubmitSqe([&](struct io_uring_sqe* sqe) {
            io_uring_prep_timeout(sqe, &dueTime, 0, 0);
            io_uring_sqe_set_data(sqe, status);
        });
    }

    [[nodiscard]] inline bool FastIoLoop::CancelTimedOp(TransferOperStatus* status) const noexcept {
        if (status == nullptr)
            return false;

        if (!m_impl->m_isRunning.load(std::memory_order_acquire))
            return false;

        static_cast<void>(SubmitSqe([&](struct io_uring_sqe* sqe) {
            io_uring_prep_cancel(sqe, status, 0);
            io_uring_sqe_set_data(sqe, nullptr);
        }));
        return true;
    }
#else
    inline bool FastIoLoop::SubmitIo(const IoRequest& request) const noexcept {
        switch (request.operation) {
            case IoOperation::Send: {
                WSABUF buffer{ static_cast<ULONG>(request.length), reinterpret_cast<char*>(request.buffer) };
                if (WSASend(request.socket, &buffer, 1, nullptr, 0, request.status, nullptr) == macroSOCKET_ERROR)
                    return WSAGetLastError() == WSA_IO_PENDING;
                return true;
            }
            case IoOperation::Recv: {
                WSABUF buffer{ static_cast<ULONG>(request.length), reinterpret_cast<char*>(request.buffer) };
                DWORD flags{};
                if (WSARecv(request.socket, &buffer, 1, nullptr, &flags, request.status, nullptr) == macroSOCKET_ERROR)
                    return WSAGetLastError() == WSA_IO_PENDING;
                return true;
            }
            case IoOperation::Connect: {
                if (::connect(request.socket, request.destination, request.destinationLength) == 0) {
                    request.status->callback(request.status->context, 0, true);
                    return true;
                }

                if (WSAGetLastError() != WSAEWOULDBLOCK)
                    return false;

                fd_set writeSet{};
                fd_set errorSet{};
                FD_ZERO(&writeSet);
                FD_ZERO(&errorSet);
                FD_SET(request.socket, &writeSet);
                FD_SET(request.socket, &errorSet);

                if (select(static_cast<int>(request.socket) + 1, nullptr, &writeSet, &errorSet, nullptr) <= 0) {
                    request.status->callback(request.status->context, 0, false);
                    return true;
                }

                int soError{};
                int length{ sizeof(soError) };
                getsockopt(request.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &length);

                request.status->callback(request.status->context, 0, soError == 0);
                return true;
            }
            case IoOperation::Accept: {
                if (!request.acceptExtensions || !request.acceptExtensions->lpfnAcceptEx)
                    return false;

                DWORD bytesReceived{};
                if (!request.acceptExtensions->lpfnAcceptEx(request.socket, request.acceptedSocket, request.buffer, 0,
                    sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16,
                    &bytesReceived, request.status)
                    && WSAGetLastError() != WSA_IO_PENDING)
                    return false;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] inline bool FastIoLoop::ScheduleTimedOp(TransferOperStatus* status,
                                                          const DeadlineTimepoint deadline) const noexcept {
        if (status == nullptr)
            return false;

        bool wake{};
        if (!m_impl->PushTimed(status, deadline, wake))
            return false;

        if (wake)
            static_cast<void>(PostQueuedCompletionStatus(m_impl->m_iocpHandle, 0, 0, nullptr));
        return true;
    }

    [[nodiscard]] inline bool FastIoLoop::CancelTimedOp(TransferOperStatus* status) const noexcept {
        if (status == nullptr)
            return false;

        if (!m_impl->m_isRunning.load(std::memory_order_acquire))
            return false;

        if (m_impl->RemoveTimed(status))
            static_cast<void>(PostQueuedCompletionStatus(m_impl->m_iocpHandle, 0, 0, status));
        return true;
    }

    inline void FastIoLoop::ProcessExpiredTimers() const noexcept {
        std::vector<TransferOperStatus*> due{};
        m_impl->PopDueTo(due);

        for (TransferOperStatus* status : due) {
            if (!status || !status->callback)
                continue;
            try {
                status->callback(status->context, 0, true);
            } catch (const std::exception& e) {
                std::println(stderr, "Exception in FastIoLoop Worker: {}", e.what());
            } catch (...) {
                std::println(stderr, "Unknown exception in FastIoLoop Worker");
            }
        }
    }
#endif

    inline bool FastIoLoop::CancelIo(const SocketHandle handle, const TransferOperStatus* status) const noexcept {
        if (status == nullptr)
            return false;

#ifdef _WIN32
        if (reinterpret_cast<SocketFd>(handle) == macroINVALID_SOCKET)
            return false;

        if (CancelIoEx(handle, const_cast<TransferOperStatus*>(status)))
            return true;

        return GetLastError() == ERROR_NOT_FOUND;
#else
        static_cast<void>(handle);

        return SubmitSqe([&](struct io_uring_sqe* sqe) {
            io_uring_prep_cancel(sqe, const_cast<TransferOperStatus*>(status), 0);
            io_uring_sqe_set_data(sqe, nullptr);
        });
#endif
    }

    inline bool FastIoLoop::RegisterHandle(const SocketHandle handle) const noexcept {
#ifdef _WIN32
        return CreateIoCompletionPort(handle, m_impl->m_iocpHandle, 0, 0) != nullptr;
#else
        (void)handle;
        return true;
#endif
    }

    inline bool FastIoLoop::PostWork(TransferOperStatus* status) const noexcept {
#ifdef _WIN32
        return PostQueuedCompletionStatus(m_impl->m_iocpHandle, 0, 0, status);
#else
        return SubmitSqe([&](struct io_uring_sqe* sqe) {
            io_uring_prep_nop(sqe);
            io_uring_sqe_set_data(sqe, status);
        });
#endif
    }

    inline void FastIoLoop::Stop() noexcept {
        if (!m_impl->m_isRunning.exchange(false, std::memory_order_acq_rel))
            return;

#ifdef _WIN32
        {
            const std::lock_guard lock{ m_impl->m_timerMutex };
            m_impl->m_timers.clear();
        }

        for (size_t i{}; i < m_impl->m_workers.size(); ++i)
            PostQueuedCompletionStatus(m_impl->m_iocpHandle, 0, 0, nullptr);

        m_impl->m_workers.clear();

        if (m_impl->m_iocpHandle) {
            CloseHandle(m_impl->m_iocpHandle);
            m_impl->m_iocpHandle = nullptr;
        }
#else
        {
            std::lock_guard lock(m_impl->m_ringMutex);
            if (struct io_uring_sqe* sqe{ io_uring_get_sqe(&m_impl->m_ring) }) {
                io_uring_prep_nop(sqe);
                io_uring_sqe_set_data(sqe, nullptr);
                io_uring_submit(&m_impl->m_ring);
            }
        }

        m_impl->m_workCv.notify_all();
        m_impl->m_workers.clear();

        {
            std::lock_guard lock(m_impl->m_ringMutex);

            // Cancel every in-flight request before tearing the ring down: io_uring_queue_exit
            // with live ops is undefined behaviour (the kernel may still touch the buffers and
            // the ring memory being released). IORING_ASYNC_CANCEL_ANY (kernel 5.19+) cancels
            // all pending requests; their -ECANCELED completions are reaped below and DROPPED,
            // never dispatched - the senders that submitted them are long gone.
            if (struct io_uring_sqe* sqe{ io_uring_get_sqe(&m_impl->m_ring) }) {
                io_uring_prep_cancel(sqe, nullptr, IORING_ASYNC_CANCEL_ANY);
                io_uring_sqe_set_data(sqe, nullptr);
                io_uring_submit(&m_impl->m_ring);
            }

            // Drain what the kernel already completed. The cancel-any marker (user_data ==
            // nullptr) bounds the wait: after it lands, all cancellations have been issued and
            // whatever follows is just cancellation debris to discard.
            struct io_uring_cqe* cqe{};
            for (int i{}; i < 4096; ++i) {
                if (io_uring_peek_cqe(&m_impl->m_ring, &cqe) != 0 || !cqe) {
                    std::this_thread::yield();
                    continue;
                }
                const bool wasCancelMarker{ io_uring_cqe_get_data(cqe) == nullptr };
                io_uring_cqe_seen(&m_impl->m_ring, cqe);
                if (wasCancelMarker) break;
            }

            // Best-effort sweep for the -ECANCELED completions that landed after the marker.
            while (io_uring_peek_cqe(&m_impl->m_ring, &cqe) == 0 && cqe)
                io_uring_cqe_seen(&m_impl->m_ring, cqe);
            io_uring_queue_exit(&m_impl->m_ring);
        }
#endif
    }

    inline void FastIoLoop::WorkerLoop() const noexcept {
        while (m_impl->m_isRunning.load(std::memory_order_relaxed)) {
#ifdef _WIN32
            DWORD bytesTransferred{};
            ULONG_PTR completionKey{};
            WSAOVERLAPPED *overlapped{};

            const bool success{
                GetQueuedCompletionStatus(m_impl->m_iocpHandle, &bytesTransferred, &completionKey, &overlapped,
                                          m_impl->TimedWaitMs()) != 0
            };

            if (!success && GetLastError() == WAIT_TIMEOUT) {
                ProcessExpiredTimers();
                continue;
            }

            if (!overlapped) continue;
            auto *status{ static_cast<TransferOperStatus *>(overlapped) };
            if (!status || !status->callback)
                continue;
            try {
                status->callback(status->context, static_cast<size_t>(bytesTransferred), success);
            } catch (const std::exception& e) {
                std::println(stderr, "Exception in FastIoLoop Worker: {}", e.what());
            } catch (...) {
                std::println(stderr, "Unknown exception in FastIoLoop Worker");
            }
#else
            Impl::WorkItem item{};
            {
                std::unique_lock lock(m_impl->m_workMutex);
                m_impl->m_workCv.wait(lock, [this] {
                    return !m_impl->m_workQueue.empty() || !m_impl->m_isRunning.load(std::memory_order_relaxed);
                });
                if (m_impl->m_workQueue.empty()) continue;
                item = m_impl->m_workQueue.front();
                m_impl->m_workQueue.pop();
            }

            if (item.callback) {
                try {
                    item.callback(item.context, item.bytesTransferred, item.success);
                } catch (const std::exception& e) {
                    std::println(stderr, "Exception in FastIoLoop Worker: {}", e.what());
                } catch (...) {
                    std::println(stderr, "Unknown exception in FastIoLoop Worker");
                }
            }
#endif
        }
    }

    [[nodiscard]] inline FastIoScheduler FastIoLoop::GetScheduler() const noexcept {
        return FastIoScheduler{ this };
    }

    [[nodiscard]] inline FastIoScheduleSender FastIoScheduler::schedule() const noexcept {
        return FastIoScheduleSender{ m_loop };
    }

    [[nodiscard]] inline FastIoScheduler::time_point FastIoScheduler::now() const noexcept {
        return std::chrono::steady_clock::now();
    }

    [[nodiscard]] inline FastIoScheduleAfterSender FastIoScheduler::schedule_after(const duration d) const noexcept {
        return FastIoScheduleAfterSender{ m_loop, d };
    }

    template <class Clock, class Duration>
    [[nodiscard]] FastIoScheduleAfterSender
    FastIoScheduler::schedule_at(const std::chrono::time_point<Clock, Duration>& when) const noexcept {
        return { m_loop, std::chrono::duration_cast<duration>(when - Clock::now()) };
    }

    [[nodiscard]] inline FastIoScheduler FastIoScheduleEnv::query(
        stdexec::get_completion_scheduler_t<stdexec::set_value_t>) const noexcept {
        return m_scheduler;
    }

    [[nodiscard]] inline FastIoScheduleEnv FastIoScheduleSender::get_env() const noexcept {
        return FastIoScheduleEnv{ FastIoScheduler{ m_loop } };
    }

    template <class Receiver>
    FastIoScheduleSender::OperationState<Receiver>::OperationState(const FastIoLoop *loop, Receiver r)
        : m_loop{ loop }, m_receiver{ std::move(r) } {
    }

    template <class Receiver>
    void FastIoScheduleSender::OperationState<Receiver>::Callback(void *context, LongIoCount /*bytesTransferred*/,
                                                                    const bool success) {
        auto *self = static_cast<OperationState *>(context);
        if (success)
            stdexec::set_value(std::move(self->m_receiver));
        else
            stdexec::set_error(std::move(self->m_receiver), ConnectionErrorEnum::NoScheduler);
    }

    template <class Receiver>
    void FastIoScheduleSender::OperationState<Receiver>::start() & noexcept {
        m_status.context  = this;
        m_status.callback = Callback;
        if (!m_loop->PostWork(&m_status))
            stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
    }

    template <class Receiver>
    auto FastIoScheduleSender::connect(Receiver r) const noexcept -> OperationState<Receiver> {
        return { m_loop, std::move(r) };
    }

    [[nodiscard]] inline FastIoScheduleEnv FastIoScheduleAfterSender::get_env() const noexcept {
        return FastIoScheduleEnv{ FastIoScheduler{ m_loop } };
    }

    template <class Receiver>
    FastIoScheduleAfterSender::OperationState<Receiver>::OperationState(
        const FastIoLoop* loop, const std::chrono::steady_clock::duration duration, Receiver r)
        : m_loop{ loop }, m_receiver{ std::move(r) }, m_deadline{ std::chrono::steady_clock::now() + duration } {
    }

    template <class Receiver>
    void FastIoScheduleAfterSender::OperationState<Receiver>::start() & noexcept {
        m_status.context  = this;
        m_status.callback = Callback;

        auto token{ stdexec::get_stop_token(stdexec::get_env(m_receiver)) };

        if (token.stop_possible() && token.stop_requested()) {
            stdexec::set_stopped(std::move(m_receiver));
            return;
        }

        if (token.stop_possible())
            m_stopCb.emplace(std::move(token), CancelHandler{ this });

        if (m_cancelRequested.load(std::memory_order_seq_cst)) {
            m_stopCb.reset();
            stdexec::set_stopped(std::move(m_receiver));
            return;
        }

        if (!m_loop->ScheduleTimedOp(&m_status, m_deadline)) {
            m_stopCb.reset();
            stdexec::set_error(std::move(m_receiver), ConnectionErrorEnum::NoScheduler);
        }
    }

    template <class Receiver>
    void FastIoScheduleAfterSender::OperationState<Receiver>::CancelPath() noexcept {
        m_cancelRequested.store(true, std::memory_order_seq_cst);
        if (!m_loop->CancelTimedOp(&m_status))
            stdexec::set_stopped(std::move(m_receiver));
    }

    template <class Receiver>
    void FastIoScheduleAfterSender::OperationState<Receiver>::Callback(void* context,
                                                                       [[maybe_unused]] LongIoCount bytesTransferred,
                                                                       [[maybe_unused]] const bool success) {
        auto* self{ static_cast<OperationState*>(context) };
        self->m_stopCb.reset();

        const bool cancelled{ self->m_cancelRequested.load(std::memory_order_seq_cst)
                           || stdexec::get_stop_token(stdexec::get_env(self->m_receiver)).stop_requested() };
        if (cancelled)
            stdexec::set_stopped(std::move(self->m_receiver));
        else
            stdexec::set_value(std::move(self->m_receiver));
    }

    template <class Receiver>
    auto FastIoScheduleAfterSender::connect(Receiver r) const noexcept -> OperationState<Receiver> {
        return { m_loop, m_duration, std::move(r) };
    }
}

#endif