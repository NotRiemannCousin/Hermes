#pragma once
#include <stdexcept>
#include <format>
#include <print>

namespace Hermes {

    inline FastIoLoop::FastIoLoop(const unsigned int threadCount) {
        _iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, threadCount);
        if (!_iocpHandle) {
            throw std::runtime_error(std::format("Not able to create IOCP. GetLastError: {}", GetLastError()));
        }

        _isRunning.store(true, std::memory_order_release);

        _workers.reserve(threadCount);
        for (unsigned int i{}; i < threadCount; ++i) {
            _workers.emplace_back([this] { this->WorkerLoop(); });
        }
    }

    inline FastIoLoop::~FastIoLoop() {
        Stop();
    }

    inline bool FastIoLoop::RegisterHandle(const HANDLE handle) const noexcept {
        return CreateIoCompletionPort(handle, _iocpHandle, 0, 0) != nullptr;
    }

    inline bool FastIoLoop::PostWork(TransferOperStatus* status) const noexcept {
        return PostQueuedCompletionStatus(_iocpHandle, 0, 0, status);
    }

    inline void FastIoLoop::Stop() noexcept {
        if (_isRunning.exchange(false, std::memory_order_acq_rel)) {
            for (size_t i{}; i < _workers.size(); ++i)
                PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
        }

        _workers.clear();

        if (_iocpHandle) {
            CloseHandle(_iocpHandle);
            _iocpHandle = nullptr;
        }
    }

    [[nodiscard]] inline FastIoScheduler FastIoLoop::GetScheduler() const noexcept {
        return FastIoScheduler{ this };
    }

    inline void FastIoLoop::WorkerLoop() const noexcept {
        while (_isRunning.load(std::memory_order_relaxed)) {
            DWORD bytesTransferred{};
            ULONG_PTR completionKey{};
            WSAOVERLAPPED* overlapped{};

            const bool success{ GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &completionKey, &overlapped, INFINITE) != 0 };

            if (!overlapped) continue;

            auto* status{ static_cast<TransferOperStatus*>(overlapped) };
            if (!status || !status->callback)
                continue;

            try {
                status->callback(status->context, bytesTransferred, success);
            } catch (const std::exception& e) {
                std::println(stderr, "Exception in FastIoLoop Worker: {}", e.what());
            } catch (...) {
                std::println(stderr, "Unknown exception in FastIoLoop Worker");
            }
        }
    }

    [[nodiscard]] inline FastIoScheduleSender FastIoScheduler::schedule() const noexcept {
        return FastIoScheduleSender{ _loop };
    }

    template <class Receiver>
    FastIoScheduleSender::OperationState<Receiver>::OperationState(const FastIoLoop* loop, Receiver r)
        : _loop{ loop }, _receiver{ std::move(r) } {}

    template <class Receiver>
    void FastIoScheduleSender::OperationState<Receiver>::S_Callback(void* context, DWORD /*bytesTransferred*/, bool success) {
        auto* self = static_cast<OperationState*>(context);

        if (success)
            stdexec::set_value(std::move(self->_receiver));
        else
            stdexec::set_stopped(std::move(self->_receiver));
    }

    template <class Receiver>
    void tag_invoke(stdexec::start_t, FastIoScheduleSender::OperationState<Receiver>& self) noexcept {
        self._status.context = &self;
        self._status.callback = FastIoScheduleSender::OperationState<Receiver>::S_Callback;

        self._loop->PostWork(&self._status);
    }
}
