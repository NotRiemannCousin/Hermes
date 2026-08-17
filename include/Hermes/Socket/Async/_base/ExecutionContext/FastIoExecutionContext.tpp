#pragma once
#include <stdexcept>
#include <format>
#include <print>
#include <vector>
#include <atomic>

#ifndef _WIN32
#include <liburing.h>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include <queue>
#include <condition_variable>

namespace Hermes::details_ {
    inline std::unordered_map<int, FastIoLoop *> g_loopMap;
    inline std::shared_mutex g_loopMapMutex;
}
#endif

namespace Hermes {
#ifdef _WIN32
    struct FastIoLoop::Impl {
        SocketHandle m_iocpHandle{nullptr};
        std::vector<std::jthread> m_workers;
        std::atomic<bool> m_isRunning{false};
    };
#else
    struct FastIoLoop::Impl {
        io_uring m_ring{};
        std::mutex m_ringMutex{};
        std::vector<std::jthread> m_workers;
        std::atomic<bool> m_isRunning{false};

        struct WorkItem {
            void* context;
            TransferOperStatus::Operation* callback;
            size_t bytesTransferred;
            bool success;
        };
        std::queue<WorkItem> m_workQueue;
        std::mutex m_workMutex;
        std::condition_variable m_workCv;
    };
#endif

    inline FastIoLoop::FastIoLoop(const unsigned int threadCount) : m_impl(std::make_unique<Impl>()) {
#ifdef _WIN32
        m_impl->m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, threadCount);
        if (!m_impl->m_iocpHandle) {
            throw std::runtime_error(std::format("Not able to create IOCP. GetLastError: {}", GetLastError()));
        }

        m_impl->m_isRunning.store(true, std::memory_order_release);

        m_impl->m_workers.reserve(threadCount);
        for (unsigned int i{}; i < threadCount; ++i) {
            m_impl->m_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#else
        if (io_uring_queue_init(1024, &m_impl->m_ring, 0) < 0) {
            throw std::runtime_error("Not able to create io_uring");
        }

        m_impl->m_isRunning.store(true, std::memory_order_release);

        m_impl->m_workers.reserve(threadCount);

        m_impl->m_workers.emplace_back([this, threadCount] {
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

                if (threadCount > 1) {
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

        for (unsigned int i{ 1 }; i < threadCount; ++i) {
            m_impl->m_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#endif
    }

    inline FastIoLoop::~FastIoLoop() {
        Stop();
    }

#ifndef _WIN32
    template<typename F>
    inline void FastIoLoop::SubmitIo(F&& prep_fn, const bool bypassRunningCheck) const noexcept {
        std::lock_guard lock(const_cast<std::mutex&>(m_impl->m_ringMutex));

        // Bail out if the loop has already been (or is being) torn down. Without this check,
        // a SubmitIo() call racing with Stop() on another thread could read/write the io_uring
        // instance concurrently with -- or strictly after -- io_uring_queue_exit() unmapping
        // its internal buffers. Confirmed via AddressSanitizer as a SEGV (read of unmapped
        // memory) inside liburing's io_uring_get_sqe() when this happened.
        // bypassRunningCheck is set only by Stop()'s own internal wake-up NOP, which is
        // intentionally submitted right after m_isRunning is cleared (that IS the shutdown
        // signal to the completion-thread) -- still safe, because it and this check both run
        // under m_ringMutex, so it can't race with the queue_exit() call further down in Stop().
        if (!bypassRunningCheck && !m_impl->m_isRunning.load(std::memory_order_acquire))
            return;

        struct io_uring_sqe *sqe = io_uring_get_sqe(const_cast<io_uring *>(&m_impl->m_ring));
        if (sqe) {
            prep_fn(sqe);
            io_uring_submit(const_cast<io_uring *>(&m_impl->m_ring));
        }
    }
#endif

    inline bool FastIoLoop::RegisterHandle(const SocketHandle handle) const noexcept {
#ifdef _WIN32
        return CreateIoCompletionPort(handle, m_impl->m_iocpHandle, 0, 0) != nullptr;
#else
        int fd{ static_cast<int>(reinterpret_cast<intptr_t>(handle)) };
        RegisterSocketLoop(fd, const_cast<FastIoLoop *>(this));
        return true;
#endif
    }

    inline bool FastIoLoop::PostWork(TransferOperStatus *status) const noexcept {
#ifdef _WIN32
        return PostQueuedCompletionStatus(m_impl->m_iocpHandle, 0, 0, status);
#else
        SubmitIo([&](struct io_uring_sqe *sqe) {
            io_uring_prep_nop(sqe);
            io_uring_sqe_set_data(sqe, status);
        });
        return true;
#endif
    }

    inline void FastIoLoop::Stop() noexcept {
        // Guard the ENTIRE teardown (not just the wake-up signals) behind the exchange, so
        // Stop() is idempotent. Previously m_workers.clear() and io_uring_queue_exit()/
        // CloseHandle() ran unconditionally on every call: since the destructor always calls
        // Stop(), any code that also called loop.Stop() explicitly (a normal graceful-shutdown
        // pattern) hit a second io_uring_queue_exit() on an already-torn-down ring -- confirmed
        // via gdb as a SIGSEGV inside liburing's io_uring_queue_exit().
        if (m_impl->m_isRunning.exchange(false, std::memory_order_acq_rel)) {
#ifdef _WIN32
            for (size_t i{}; i < m_impl->m_workers.size(); ++i) {
                PostQueuedCompletionStatus(m_impl->m_iocpHandle, 0, 0, nullptr);
            }
#else
            SubmitIo([](struct io_uring_sqe *sqe) {
                io_uring_prep_nop(sqe);
                io_uring_sqe_set_data(sqe, nullptr);
            }, true);
            m_impl->m_workCv.notify_all();
#endif

            m_impl->m_workers.clear();

#ifdef _WIN32
            if (m_impl->m_iocpHandle) {
                CloseHandle(m_impl->m_iocpHandle);
                m_impl->m_iocpHandle = nullptr;
            }
#else
            // Also taken under m_ringMutex -- the same lock SubmitIo() holds while touching
            // the ring -- so a submit that started just before m_isRunning flipped to false
            // finishes (or safely no-ops) before the ring is torn down, instead of racing
            // with it.
            {
                std::lock_guard lock(m_impl->m_ringMutex);
                io_uring_queue_exit(&m_impl->m_ring);
            }
#endif
        }
    }

#ifndef _WIN32
    inline FastIoLoop *FastIoLoop::GetLoopForSocket(int fd) noexcept {
        std::shared_lock lock(details_::g_loopMapMutex);
        auto it{ details_::g_loopMap.find(fd) };
        return it != details_::g_loopMap.end() ? it->second : nullptr;
    }

    inline void FastIoLoop::RegisterSocketLoop(int fd, FastIoLoop *loop) noexcept {
        std::unique_lock lock(details_::g_loopMapMutex);
        details_::g_loopMap[fd] = loop;
    }

    inline void FastIoLoop::UnregisterSocketLoop(int fd) noexcept {
        std::unique_lock lock(details_::g_loopMapMutex);
        details_::g_loopMap.erase(fd);
    }
#endif

    inline void FastIoLoop::WorkerLoop() const noexcept {
        while (m_impl->m_isRunning.load(std::memory_order_relaxed)) {
#ifdef _WIN32
            DWORD bytesTransferred{};
            ULONG_PTR completionKey{};
            WSAOVERLAPPED *overlapped{};

            const bool success{
                GetQueuedCompletionStatus(m_impl->m_iocpHandle, &bytesTransferred, &completionKey, &overlapped,
                                          INFINITE) != 0
            };

            if (!overlapped) continue;
            auto *status{static_cast<TransferOperStatus *>(overlapped)};
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
            stdexec::set_stopped(std::move(self->m_receiver));
    }

    template <class Receiver>
    void FastIoScheduleSender::OperationState<Receiver>::start() & noexcept {
        m_status.context = this;
        m_status.callback = Callback;
        m_loop->PostWork(&m_status);
    }

    template <class Receiver>
    auto FastIoScheduleSender::connect(Receiver r) const noexcept -> OperationState<Receiver> {
        return { m_loop, std::move(r) };
    }
}