#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_NATIVE_SCHEDULER

#include <stdexcept>
#include <format>
#include <print>
#include <vector>
#include <atomic>
#include <algorithm>

#ifndef _WIN32
#include <liburing.h>
#include <mutex>
#include <queue>
#include <condition_variable>
#endif

namespace Hermes {
#ifdef _WIN32
    struct FastIoLoop::Impl {
        SocketHandle m_iocpHandle{ nullptr };
        std::vector<std::jthread> m_workers;
        std::atomic<bool> m_isRunning{ false };
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
    };
#endif

    inline FastIoLoop::FastIoLoop(const unsigned int threadCount) : m_impl{ std::make_unique<Impl>() } {
        const unsigned int workerCount{ std::max(1u, threadCount) };

#ifdef _WIN32
        m_impl->m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, workerCount);
        if (!m_impl->m_iocpHandle) {
            throw std::runtime_error(std::format("Not able to create IOCP. GetLastError: {}", GetLastError()));
        }

        m_impl->m_isRunning.store(true, std::memory_order_release);

        m_impl->m_workers.reserve(workerCount);
        for (unsigned int i{}; i < workerCount; ++i) {
            m_impl->m_workers.emplace_back([this] { this->WorkerLoop(); });
        }
#else
        if (io_uring_queue_init(1024, &m_impl->m_ring, 0) < 0) {
            throw std::runtime_error("Not able to create io_uring");
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
    template <typename F>
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
                    io_uring_prep_send(sqe, fd, request.buffer, request.length, 0);
                    break;
                case IoOperation::Recv:
                    io_uring_prep_recv(sqe, fd, request.buffer, request.length, 0);
                    break;
            }

            io_uring_sqe_set_data(sqe, request.status);
        });
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
#endif

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
                                          INFINITE) != 0
            };

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
        m_status.context  = this;
        m_status.callback = Callback;
        if (!m_loop->PostWork(&m_status))
            stdexec::set_stopped(std::move(m_receiver));
    }

    template <class Receiver>
    auto FastIoScheduleSender::connect(Receiver r) const noexcept -> OperationState<Receiver> {
        return { m_loop, std::move(r) };
    }
}

#endif