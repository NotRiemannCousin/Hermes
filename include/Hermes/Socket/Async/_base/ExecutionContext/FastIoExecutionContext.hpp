#pragma once
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <thread>
#include <memory>

namespace Hermes {
    enum class ConnectionErrorEnum;
}

namespace Hermes {

#ifdef _WIN32
    struct TransferOperStatus : WSAOVERLAPPED {
        using Operation = void(void* context, size_t transferedBytes, bool success);

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

    class FastIoLoop;
    struct FastIoScheduler;
    struct FastIoScheduleSender;

    //! @brief IOCP/io_uring based loop for sockets.
    class FastIoLoop {
        struct Impl;
        std::unique_ptr<Impl> _impl;

    public:
        explicit FastIoLoop(unsigned int threadCount = std::thread::hardware_concurrency());
        ~FastIoLoop();

        FastIoLoop(const FastIoLoop&) = delete;
        FastIoLoop& operator=(const FastIoLoop&) = delete;

        bool RegisterHandle(SocketHandle handle) const noexcept;
        void Stop() noexcept;

        [[nodiscard]] FastIoScheduler GetScheduler() const noexcept;

        bool PostWork(TransferOperStatus* status) const noexcept;

#ifndef _WIN32
        static FastIoLoop* GetLoopForSocket(int fd) noexcept;
        static void RegisterSocketLoop(int fd, FastIoLoop* loop) noexcept;
        static void UnregisterSocketLoop(int fd) noexcept;

        template <typename F>
        void SubmitIo(F&& prep_fn) const noexcept;
#endif

    private:
        void WorkerLoop() const noexcept;
    };

    struct FastIoScheduler {
        const FastIoLoop* _loop;

        bool operator==(const FastIoScheduler& other) const noexcept = default;

        [[nodiscard]] FastIoScheduleSender schedule() const noexcept;
    };

    struct FastIoScheduleSender {
        using sender_concept = stdexec::sender_t;

        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_stopped_t()
        >;

        const FastIoLoop* _loop;

        template <class Receiver>
        struct OperationState {
            const FastIoLoop* _loop;
            Receiver _receiver;
            TransferOperStatus _status{};

            OperationState(const FastIoLoop* loop, Receiver r);

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept {
                self._status.context = &self;
                self._status.callback = S_Callback;
                self._loop->PostWork(&self._status);
            }
            static void S_Callback(void* context, size_t bytesTransferred, bool success);
        };

        template <class Receiver>
        friend OperationState<Receiver> tag_invoke(stdexec::connect_t, const FastIoScheduleSender& self, Receiver r) noexcept {
            return { self._loop, std::move(r) };
        }
    };

    static_assert(stdexec::scheduler<FastIoScheduler>);
    static_assert(stdexec::sender<FastIoScheduleSender>);
    static_assert(std::same_as<stdexec::schedule_result_t<FastIoScheduler>, FastIoScheduleSender>);
}

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.tpp>