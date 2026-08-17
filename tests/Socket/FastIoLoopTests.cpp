#include <gtest/gtest.h>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>
#include <stdexec/execution.hpp>

#include <atomic>
#include <thread>
#include <vector>
#include <barrier>
#include <chrono>

using namespace std::chrono_literals;

namespace {
    // Runs `work` on the given scheduler and blocks until it completes (via set_value)
    // or is stopped (via set_stopped). Returns true iff set_value fired.
    bool RunOnScheduler(const Hermes::FastIoScheduler scheduler, auto&& work) {
        bool completed{ false };
        auto sender{ stdexec::then(stdexec::schedule(scheduler), [&] { work(); completed = true; }) };
        stdexec::sync_wait(std::move(sender));
        return completed;
    }
}

#pragma region Baseline correctness

TEST(FastIoLoopTest, SingleScheduledOperationCompletes) {
    Hermes::FastIoLoop loop{ 2 };

    bool ran{ false };
    const bool completed{ RunOnScheduler(loop.GetScheduler(), [&] { ran = true; }) };

    EXPECT_TRUE(completed);
    EXPECT_TRUE(ran);
}

TEST(FastIoLoopTest, SingleThreadedLoop_SingleScheduledOperationCompletes) {
    // threadCount=1 takes a structurally different code path (the completion thread runs
    // the user callback inline instead of handing off through the work queue), so it needs
    // its own coverage.
    Hermes::FastIoLoop loop{ 1 };

    bool ran{ false };
    const bool completed{ RunOnScheduler(loop.GetScheduler(), [&] { ran = true; }) };

    EXPECT_TRUE(completed);
    EXPECT_TRUE(ran);
}

TEST(FastIoLoopTest, MultipleSequentialOperationsAllComplete) {
    Hermes::FastIoLoop loop{ 2 };
    const auto scheduler{ loop.GetScheduler() };

    int count{ 0 };
    for (int i{}; i < 50; ++i)
        RunOnScheduler(scheduler, [&] { ++count; });

    EXPECT_EQ(count, 50);
}

#pragma endregion


#pragma region Stop() idempotency (regression test for a real crash)

// Regression test. Previously, FastIoLoop::Stop() ran m_workers.clear() and
// io_uring_queue_exit() unconditionally on every call, instead of only on the first.
// Since ~FastIoLoop() always calls Stop(), any code that also called loop.Stop()
// explicitly (a normal graceful-shutdown pattern) triggered a second
// io_uring_queue_exit() on an already-torn-down ring. Confirmed via gdb as a SIGSEGV
// inside liburing's io_uring_queue_exit() before the fix.
TEST(FastIoLoopTest, ExplicitStopFollowedByDestructorDoesNotCrash) {
    Hermes::FastIoLoop loop{ 2 };

    bool ran{ false };
    RunOnScheduler(loop.GetScheduler(), [&] { ran = true; });
    EXPECT_TRUE(ran);

    loop.Stop();          // explicit shutdown ...
    // ... followed by the destructor calling Stop() again when `loop` goes out of scope.
    SUCCEED();
}

TEST(FastIoLoopTest, CallingStopManyTimesIsSafe) {
    Hermes::FastIoLoop loop{ 2 };

    for (int i{}; i < 10; ++i)
        loop.Stop();

    SUCCEED();
}

TEST(FastIoLoopTest, CallingStopBeforeAnyWorkIsSafe) {
    Hermes::FastIoLoop loop{ 2 };
    loop.Stop();
    SUCCEED();
}

#pragma endregion


#pragma region Concurrency: many threads scheduling concurrently

struct FastIoLoopConcurrencyTest : testing::TestWithParam<unsigned int> {};

TEST_P(FastIoLoopConcurrencyTest, ConcurrentSchedulesAllCompleteExactlyOnce) {
    constexpr int threadsCount{ 8 };
    constexpr int opsPerThread{ 200 };

    Hermes::FastIoLoop loop{ GetParam() };
    const auto scheduler{ loop.GetScheduler() };

    std::atomic<int> completedCount{ 0 };
    std::vector<std::jthread> threads;
    threads.reserve(threadsCount);

    for (int t{}; t < threadsCount; ++t) {
        threads.emplace_back([&] {
            for (int i{}; i < opsPerThread; ++i)
                RunOnScheduler(scheduler, [&] { completedCount.fetch_add(1, std::memory_order_relaxed); });
        });
    }
    threads.clear(); // joins all

    EXPECT_EQ(completedCount.load(), threadsCount * opsPerThread);
}

INSTANTIATE_TEST_SUITE_P(
    VariousLoopThreadCounts,
    FastIoLoopConcurrencyTest,
    testing::Values(1u, 2u, 4u),
    [](const testing::TestParamInfo<unsigned int>& info) { return "LoopThreads_" + std::to_string(info.param); }
);

#pragma endregion


#pragma region Concurrency: socket<->loop registration map

#ifndef _WIN32
TEST(FastIoLoopTest, ConcurrentRegisterAndUnregisterSocketLoop_NoCorruption) {
    constexpr int threadsCount{ 8 };
    constexpr int opsPerThread{ 500 };

    Hermes::FastIoLoop loopA(1);
    Hermes::FastIoLoop loopB(1);

    std::vector<std::jthread> threads;
    threads.reserve(threadsCount);

    for (int t{}; t < threadsCount; ++t) {
        threads.emplace_back([&, t] {
            for (int i{}; i < opsPerThread; ++i) {
                // Distinct fd per (thread, iteration) so entries don't stomp each other,
                // while still hammering the shared g_loopMapMutex from many threads.
                const int fd{ t * opsPerThread + i };
                Hermes::FastIoLoop::RegisterSocketLoop(fd, (i % 2 == 0) ? &loopA : &loopB);
                const auto* got{ Hermes::FastIoLoop::GetLoopForSocket(fd) };
                EXPECT_EQ(got, (i % 2 == 0) ? &loopA : &loopB);
                Hermes::FastIoLoop::UnregisterSocketLoop(fd);
                EXPECT_EQ(Hermes::FastIoLoop::GetLoopForSocket(fd), nullptr);
            }
        });
    }
    threads.clear(); // joins all

    SUCCEED();
}
#endif

#pragma endregion