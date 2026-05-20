#pragma once
#include <Hermes/_base/WinApi/WinApi.hpp>
#include <thread>

namespace Hermes {
    enum class ConnectionErrorEnum;
}

namespace Hermes {

    struct TransferOperStatus : WSAOVERLAPPED {
        using Operation = void(void* context, DWORD transferedBytes, bool success);

        void* context{};
        Operation* callback{};
    };

    class FastIoLoop;
    struct FastIoScheduler;
    struct FastIoScheduleSender;

    //! @brief IOCP/io_uring based loop for sockets.
    class FastIoLoop {
        HANDLE _iocpHandle{ nullptr };
        std::vector<std::jthread> _workers;
        std::atomic<bool> _isRunning{ false };

    public:
        explicit FastIoLoop(unsigned int threadCount = std::thread::hardware_concurrency());
        ~FastIoLoop();

        FastIoLoop(const FastIoLoop&) = delete;
        FastIoLoop& operator=(const FastIoLoop&) = delete;

        void RegisterHandle(HANDLE handle) const;
        void Stop() noexcept;

        [[nodiscard]] FastIoScheduler GetScheduler() const noexcept;

        void PostWork(TransferOperStatus* status) const;

    private:
        void WorkerLoop() const;
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

            OperationState(const FastIoLoop* loop, Receiver r) noexcept;

            friend void tag_invoke(stdexec::start_t, OperationState& self) noexcept;
            static void S_Callback(void* context, DWORD bytesTransferred, bool success) noexcept;
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

