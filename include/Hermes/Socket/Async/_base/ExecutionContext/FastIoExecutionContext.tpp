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

namespace Hermes::_details {
    inline std::unordered_map<int, FastIoLoop *> g_loopMap;
    inline std::shared_mutex g_loopMapMutex;
}
#endif

namespace Hermes {
#ifdef _WIN32
    struct FastIoLoop::Impl {
        SocketHandle _iocpHandle{nullptr};
        std::vector<std::jthread> _workers;
        std::atomic<bool> _isRunning{false};
    };
#else
    struct FastIoLoop::Impl {
        io_uring _ring{};
        std::mutex _ringMutex{};
        std::vector<std::jthread> _workers;
        std::atomic<bool> _isRunning{false};

        struct WorkItem {
            void* context;
            TransferOperStatus::Operation* callback;
            size_t bytesTransferred;
            bool success;
        };
        std::queue<WorkItem> _workQueue;
        std::mutex _workMutex;
        std::condition_variable _workCv;
    };
#endif

    inline FastIoLoop::FastIoLoop(const unsigned int threadCount) : _impl(std::make_unique<Impl>()) {
#ifdef _WIN32
        _impl->_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, threadCount);
        if (!_impl->_iocpHandle) {
            throw std::runtime_error(std::format("Not able to create IOCP. GetLastError: {}", GetLastError()));
        }

        _impl->_isRunning.store(true, std::memory_order_release);

        _impl->_workers.reserve(threadCount);
        for (unsigned int i{}; i < threadCount; ++i) {
            _impl->_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#else
        if (io_uring_queue_init(1024, &_impl->_ring, 0) < 0) {
            throw std::runtime_error("Not able to create io_uring");
        }

        _impl->_isRunning.store(true, std::memory_order_release);

        _impl->_workers.reserve(threadCount);

        _impl->_workers.emplace_back([this, threadCount] {
            while (_impl->_isRunning.load(std::memory_order_relaxed)) {
                struct io_uring_cqe* cqe;
                const int ret = io_uring_wait_cqe(&_impl->_ring, &cqe);
                if (ret < 0) continue;
                if (!cqe) continue;

                auto* status       = static_cast<TransferOperStatus*>(io_uring_cqe_get_data(cqe));
                const size_t bytes = cqe->res >= 0 ? static_cast<size_t>(cqe->res) : 0;
                const bool success = cqe->res >= 0;
                io_uring_cqe_seen(&_impl->_ring, cqe);

                if (!status) continue;

                if (threadCount > 1) {
                    {
                        std::lock_guard lock(_impl->_workMutex);
                        _impl->_workQueue.push({ status->context, status->callback, bytes, success });
                    }
                    _impl->_workCv.notify_one();
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
            _impl->_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#endif
    }

    inline FastIoLoop::~FastIoLoop() {
        Stop();
    }

#ifndef _WIN32
    template<typename F>
    inline void FastIoLoop::SubmitIo(F&& prep_fn) const noexcept {
        std::lock_guard lock(const_cast<std::mutex&>(_impl->_ringMutex));
        struct io_uring_sqe *sqe = io_uring_get_sqe(const_cast<io_uring *>(&_impl->_ring));
        if (sqe) {
            prep_fn(sqe);
            io_uring_submit(const_cast<io_uring *>(&_impl->_ring));
        }
    }
#endif

    inline bool FastIoLoop::RegisterHandle(const SocketHandle handle) const noexcept {
#ifdef _WIN32
        return CreateIoCompletionPort(handle, _impl->_iocpHandle, 0, 0) != nullptr;
#else
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
        RegisterSocketLoop(fd, const_cast<FastIoLoop *>(this));
        return true;
#endif
    }

    inline bool FastIoLoop::PostWork(TransferOperStatus *status) const noexcept {
#ifdef _WIN32
        return PostQueuedCompletionStatus(_impl->_iocpHandle, 0, 0, status);
#else
        SubmitIo([&](struct io_uring_sqe *sqe) {
            io_uring_prep_nop(sqe);
            io_uring_sqe_set_data(sqe, status);
        });
        return true;
#endif
    }

    inline void FastIoLoop::Stop() noexcept {
        if (_impl->_isRunning.exchange(false, std::memory_order_acq_rel)) {
#ifdef _WIN32
            for (size_t i{}; i < _impl->_workers.size(); ++i) {
                PostQueuedCompletionStatus(_impl->_iocpHandle, 0, 0, nullptr);
            }
#else
            SubmitIo([](struct io_uring_sqe *sqe) {
                io_uring_prep_nop(sqe);
                io_uring_sqe_set_data(sqe, nullptr);
            });
            _impl->_workCv.notify_all();
#endif
        }

        _impl->_workers.clear();

#ifdef _WIN32
        if (_impl->_iocpHandle) {
            CloseHandle(_impl->_iocpHandle);
            _impl->_iocpHandle = nullptr;
        }
#else
        io_uring_queue_exit(&_impl->_ring);
#endif
    }

#ifndef _WIN32
    inline FastIoLoop *FastIoLoop::GetLoopForSocket(int fd) noexcept {
        std::shared_lock lock(_details::g_loopMapMutex);
        auto it = _details::g_loopMap.find(fd);
        return it != _details::g_loopMap.end() ? it->second : nullptr;
    }

    inline void FastIoLoop::RegisterSocketLoop(int fd, FastIoLoop *loop) noexcept {
        std::unique_lock lock(_details::g_loopMapMutex);
        _details::g_loopMap[fd] = loop;
    }

    inline void FastIoLoop::UnregisterSocketLoop(int fd) noexcept {
        std::unique_lock lock(_details::g_loopMapMutex);
        _details::g_loopMap.erase(fd);
    }
#endif

    inline void FastIoLoop::WorkerLoop() const noexcept {
        while (_impl->_isRunning.load(std::memory_order_relaxed)) {
#ifdef _WIN32
            DWORD bytesTransferred{};
            ULONG_PTR completionKey{};
            WSAOVERLAPPED *overlapped{};

            const bool success{
                GetQueuedCompletionStatus(_impl->_iocpHandle, &bytesTransferred, &completionKey, &overlapped,
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
                std::unique_lock lock(_impl->_workMutex);
                _impl->_workCv.wait(lock, [this] {
                    return !_impl->_workQueue.empty() || !_impl->_isRunning.load(std::memory_order_relaxed);
                });
                if (_impl->_workQueue.empty()) continue;
                item = _impl->_workQueue.front();
                _impl->_workQueue.pop();
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
        return FastIoScheduleSender{ _loop };
    }

    template <class Receiver>
    FastIoScheduleSender::OperationState<Receiver>::OperationState(const FastIoLoop *loop, Receiver r)
        : _loop{ loop }, _receiver{ std::move(r) } {
    }

    template <class Receiver>
    void FastIoScheduleSender::OperationState<Receiver>::S_Callback(void *context, size_t /*bytesTransferred*/,
                                                                    bool success) {
        auto *self = static_cast<OperationState *>(context);
        if (success)
            stdexec::set_value(std::move(self->_receiver));
        else
            stdexec::set_stopped(std::move(self->_receiver));
    }
}
