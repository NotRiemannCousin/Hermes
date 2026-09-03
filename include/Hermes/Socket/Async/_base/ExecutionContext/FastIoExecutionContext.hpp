#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_NATIVE_SCHEDULER

#include <Hermes/_base/OsApi/OsApi.hpp>
#include <thread>
#include <memory>
#include <stdexec/execution.hpp>

namespace Hermes {
    enum class ConnectionErrorEnum;
}

namespace Hermes {

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

        bool RegisterHandle(SocketHandle handle) const noexcept;
        void Stop() noexcept;

        [[nodiscard]] FastIoScheduler GetScheduler() const noexcept;

        bool PostWork(TransferOperStatus* status) const noexcept;

#ifndef _WIN32
        static FastIoLoop* GetLoopForSocket(int fd) noexcept;
        static void RegisterSocketLoop(int fd, FastIoLoop* loop) noexcept;
        static void UnregisterSocketLoop(int fd) noexcept;

        template <typename F>
        void SubmitIo(F&& prep_fn, bool bypassRunningCheck = false) const noexcept;
#endif

    private:
        void WorkerLoop() const noexcept;
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

    static_assert(stdexec::scheduler<FastIoScheduler>);
    static_assert(stdexec::sender<FastIoScheduleSender>);
    static_assert(std::same_as<stdexec::schedule_result_t<FastIoScheduler>, FastIoScheduleSender>);
}

#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.tpp>

#endif