#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Socket/Async/_base/Connection/TlsAsyncConnectPolicy.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_NATIVE_SCHEDULER

using Hermes::AddressFamilyEnum;
using Hermes::DefaultSocketData;
using Hermes::FastIoLoop;
using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::SocketTypeEnum;

namespace {
    std::uint16_t GetNextPort() {
        static std::atomic<std::uint16_t> port{ 25000 };
        return port++;
    }

    IpEndpoint MakeLoopbackEndpoint(const std::uint16_t port) {
        const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
        return IpEndpoint{ loopback, port };
    }

    constexpr int kListenBacklog{ 8 };

#ifdef _WIN32
    bool AsyncBackendAvailable() {
        return true;
    }
#else
    bool AsyncBackendAvailable() {
        io_uring ring{};
        const int result{ io_uring_queue_init(2, &ring, 0) };
        if (result < 0)
            return false;
        io_uring_queue_exit(&ring);
        return true;
    }
#endif

    //! Scriptable stand-in for FastIoLoop implementing Hermes::ExecutionContextConcept.
    //!
    //! The concept evaluates the whole surface on `const Context&` (TransferOperStatus.hpp),
    //! so every method MUST be const-qualified; all mutable state is `mutable` (same pattern
    //! the real loops use internally). Only the execution context is mocked: sockets, policies
    //! and facades remain the real code, so failing registrations, failing submissions, failing
    //! completions and aborts during a pending operation become deterministic.
    class MockExecutionContext {
    public:
        // knobs (written by the test thread only)
        bool registerHandleResult{ true };
        int  registerHandleFailAfter{ -1 };
        bool submitIoResult{ true };
        bool completeSubmissionsWithError{ false };
        std::function<void()> onSubmit;

        // observation
        mutable std::atomic<int> registerHandleCalls{ 0 };
        mutable std::atomic<int> submitIoCalls{ 0 };
        mutable std::atomic<int> postWorkCalls{ 0 };
        mutable std::atomic<int> getSchedulerCalls{ 0 };

        bool RegisterHandle(const Hermes::SocketHandle handle) const {
            const int call{ registerHandleCalls.fetch_add(1) + 1 };
            {
                const std::lock_guard lock{ m_mutex };
                m_registered.push_back(handle);
            }
            if (registerHandleFailAfter >= 0 && call > registerHandleFailAfter)
                return false;
            return registerHandleResult;
        }

        bool SubmitIo(const Hermes::IoRequest& request) const {
            submitIoCalls.fetch_add(1);
            Hermes::TransferOperStatus* status{ request.status };

            {
                const std::lock_guard lock{ m_mutex };
                m_submitted.push_back(request);
                if (!completeSubmissionsWithError && status)
                    m_pending.push_back(status);
            }

            if (onSubmit)
                onSubmit();

            if (!submitIoResult)
                return false;

            if (completeSubmissionsWithError && status)
                status->callback(status->context, static_cast<Hermes::LongIoCount>(0), false);

            return true;
        }

        template<class... Args>
        bool PostWork(Args&&...) const noexcept {
            postWorkCalls.fetch_add(1);
            return true;
        }

        Hermes::FastIoScheduler GetScheduler() const {
            getSchedulerCalls.fetch_add(1);
            const std::lock_guard lock{ m_mutex };
            if (!m_schedulerFallback)
                m_schedulerFallback = std::make_unique<Hermes::FastIoLoop>(1);
            return m_schedulerFallback->GetScheduler();
        }

        void FailAllPendingRequests() {
            std::vector<Hermes::TransferOperStatus*> pending;
            {
                const std::lock_guard lock{ m_mutex };
                pending.swap(m_pending);
            }
            for (auto* status : pending)
                status->callback(status->context, static_cast<Hermes::LongIoCount>(0), false);
        }

        std::vector<Hermes::SocketHandle> RegisteredHandles() const {
            const std::lock_guard lock{ m_mutex };
            return m_registered;
        }

        std::vector<Hermes::IoRequest> SubmittedRequests() const {
            const std::lock_guard lock{ m_mutex };
            return m_submitted;
        }

    private:
        mutable std::mutex m_mutex;
        mutable std::unique_ptr<Hermes::FastIoLoop> m_schedulerFallback{};
        mutable std::vector<Hermes::SocketHandle> m_registered;
        mutable std::vector<Hermes::IoRequest> m_submitted;
        mutable std::vector<Hermes::TransferOperStatus*> m_pending;
    };

    static_assert(Hermes::ExecutionContextConcept<MockExecutionContext>,
        "MockExecutionContext must satisfy the same concept the real loops do.");

    using MockData = DefaultSocketData<MockExecutionContext, IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>;
    using MockListener = Hermes::AsyncListenerSocket<
        MockData,
        Hermes::DefaultAsyncAcceptPolicy<MockExecutionContext, MockData>,
        Hermes::DefaultAsyncTransferPolicy<MockExecutionContext, MockData>>;

    using RealSocketData = DefaultSocketData<FastIoLoop, IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>;
    using RealListenOptions = Hermes::DefaultAcceptPolicy<IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>::ListenOptions;

#if HERMES_ENABLE_TLS
    // TLS paths that fail before any real I/O happens still need client credentials to build
    // the connect state machine (no certificate file required).
    Hermes::Credentials* GetMockClientAuth() {
        static auto auth{ Hermes::Credentials::Client(
            Hermes::SChannelCredFlags::NoServernameCheck       |
            Hermes::SChannelCredFlags::IgnoreNoRevocationCheck | Hermes::SChannelCredFlags::IgnoreRevocationOffline |
            Hermes::SChannelCredFlags::NoDefaultCreds          | Hermes::SChannelCredFlags::ManualCredValidation    ) };
        if (!auth.has_value()) std::abort();
        return &*auth;
    }
#endif
}

struct MockListenerTest : testing::Test {
    MockExecutionContext mock;
};

TEST_F(MockListenerTest, Listen_NullContext_FailsWithoutTouchingContext) {
    EXPECT_ANY_THROW({
        stdexec::sync_wait(MockListener::Listen(MockData{ MakeLoopbackEndpoint(GetNextPort()) }, kListenBacklog));
    });

    EXPECT_EQ(mock.registerHandleCalls.load(), 0);
    EXPECT_EQ(mock.submitIoCalls.load(), 0);
    EXPECT_EQ(mock.postWorkCalls.load(), 0);
    EXPECT_EQ(mock.getSchedulerCalls.load(), 0);
}

TEST_F(MockListenerTest, Listen_RegisterHandleFails_FailsFast) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;
    mock.registerHandleResult = false;

    EXPECT_ANY_THROW({ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)); });

    EXPECT_EQ(mock.registerHandleCalls.load(), 1);
    EXPECT_EQ(mock.submitIoCalls.load(), 0);
    EXPECT_EQ(mock.postWorkCalls.load(), 0);
    EXPECT_EQ(mock.getSchedulerCalls.load(), 0);
}

TEST_F(MockListenerTest, Listen_Success_RegistersListenerSocketExactlyOnce) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    EXPECT_EQ(mock.registerHandleCalls.load(), 1);
    ASSERT_EQ(mock.RegisteredHandles().size(), 1u);
    EXPECT_EQ(mock.RegisteredHandles()[0], reinterpret_cast<Hermes::SocketHandle>(listener.GetSocketData().socket));

    EXPECT_EQ(mock.submitIoCalls.load(), 0);
    EXPECT_EQ(mock.postWorkCalls.load(), 0);
    EXPECT_EQ(mock.getSchedulerCalls.load(), 0);

    listener.Close();
}

TEST_F(MockListenerTest, Listen_FlakyContext_SecondListenerFailsOnRegisterHandle) {
    mock.registerHandleFailAfter = 1;

    MockData firstData{ MakeLoopbackEndpoint(GetNextPort()) };
    firstData.executionContext = &mock;
    auto first{ stdexec::sync_wait(MockListener::Listen(std::move(firstData), kListenBacklog)) };
    ASSERT_TRUE(first.has_value());
    auto listener{ std::move(std::get<0>(*first)) };

    MockData secondData{ MakeLoopbackEndpoint(GetNextPort()) };
    secondData.executionContext = &mock;
    EXPECT_ANY_THROW({ stdexec::sync_wait(MockListener::Listen(std::move(secondData), kListenBacklog)); });

    EXPECT_EQ(mock.registerHandleCalls.load(), 2);
    EXPECT_EQ(mock.submitIoCalls.load(), 0);

    listener.Close();
}

TEST_F(MockListenerTest, Listen_RegisterHandleFails_ReleasesTheBoundPort) {
    // Needs the REAL backend: the regression check re-binds the port through a real loop.
    if (!AsyncBackendAvailable())
        GTEST_SKIP() << "real async backend (IOCP/io_uring) is unavailable in this environment";

    const std::uint16_t port{ GetNextPort() };

    {
        MockData listenData{ MakeLoopbackEndpoint(port) };
        listenData.executionContext = &mock;
        mock.registerHandleResult = false;

        EXPECT_ANY_THROW({ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)); });
    }

    // Regression test for the async Listen factory: when RegisterHandle (or
    // LoadAcceptExtensions) fails AFTER the sync bind/listen succeeded, the factory
    // must Close the socket it created. If it leaks the bound socket, this real
    // Listen on the same port (reuseAddress = false) fails with address-in-use.
    FastIoLoop loop{ 1 };
    RealSocketData realData{ MakeLoopbackEndpoint(port) };
    realData.executionContext = &loop;

    auto completed{ stdexec::sync_wait(Hermes::RawTcpAsyncListener::Listen(
        std::move(realData), RealListenOptions{ .reuseAddress = false }, kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };
    listener.Close();
}

TEST_F(MockListenerTest, AcceptOne_SubmitIoFails_FailsWithConnectionFailed) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    mock.submitIoResult = false;

    bool errorCaptured{ false };
    try {
        stdexec::sync_wait(listener.AsyncAcceptOne());
        ADD_FAILURE() << "AsyncAcceptOne completed despite SubmitIo returning false";
    } catch (const Hermes::ConnectionErrorEnum& error) {
        errorCaptured = true;
        EXPECT_EQ(error, Hermes::ConnectionErrorEnum::ConnectionFailed);
    } catch (...) {
        ADD_FAILURE() << "AsyncAcceptOne threw an unexpected error type (expected ConnectionErrorEnum)";
    }
    EXPECT_TRUE(errorCaptured);

    EXPECT_EQ(mock.submitIoCalls.load(), 1);
    EXPECT_EQ(mock.registerHandleCalls.load(), 1);
    EXPECT_EQ(mock.postWorkCalls.load(), 0);

    listener.Close();
}

TEST_F(MockListenerTest, AcceptOne_SequentialFailingSubmissions_EachAttemptsOneSubmit) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    mock.submitIoResult = false;

    for (int i{}; i < 3; ++i)
        EXPECT_ANY_THROW({ stdexec::sync_wait(listener.AsyncAcceptOne()); });

    EXPECT_EQ(mock.submitIoCalls.load(), 3);
    EXPECT_EQ(mock.registerHandleCalls.load(), 1);

    listener.Close();
}

TEST_F(MockListenerTest, AcceptOne_SubmitIoReceivesListenerSocketAndAcceptOperation) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    mock.submitIoResult = false;
    EXPECT_ANY_THROW({ stdexec::sync_wait(listener.AsyncAcceptOne()); });

    const auto submitted{ mock.SubmittedRequests() };
    ASSERT_EQ(submitted.size(), 1u);
    EXPECT_EQ(submitted[0].operation, Hermes::IoOperation::Accept);
    EXPECT_EQ(submitted[0].socket, listener.GetSocketData().socket);

    listener.Close();
}

TEST_F(MockListenerTest, AcceptOne_CompletionReportsFailure_FailsWithConnectionFailed) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    mock.completeSubmissionsWithError = true;

    bool errorCaptured{ false };
    try {
        stdexec::sync_wait(listener.AsyncAcceptOne());
        ADD_FAILURE() << "AsyncAcceptOne completed despite a failed completion";
    } catch (const Hermes::ConnectionErrorEnum& error) {
        errorCaptured = true;
        EXPECT_EQ(error, Hermes::ConnectionErrorEnum::ConnectionFailed);
    } catch (...) {
        ADD_FAILURE() << "AsyncAcceptOne threw an unexpected error type (expected ConnectionErrorEnum)";
    }
    EXPECT_TRUE(errorCaptured);

    EXPECT_EQ(mock.submitIoCalls.load(), 1);
    EXPECT_EQ(mock.postWorkCalls.load(), 0);
    EXPECT_EQ(mock.getSchedulerCalls.load(), 0);

    listener.Close();
}

TEST_F(MockListenerTest, AcceptOne_AbortDuringPendingAccept_FailsDeterministically) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    mock.onSubmit = [&] {
        std::jthread teardown{ [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
            listener.Abort();
            mock.FailAllPendingRequests();
        } };
        teardown.detach();
    };

    EXPECT_ANY_THROW({ stdexec::sync_wait(listener.AsyncAcceptOne()); });

    EXPECT_EQ(mock.submitIoCalls.load(), 1);

    listener.Close();
}

TEST_F(MockListenerTest, MockSurface_PostWorkAndGetScheduler_CountCalls) {
    // GetScheduler() lazily builds a real FastIoLoop to hand out a genuine scheduler.
    if (!AsyncBackendAvailable())
        GTEST_SKIP() << "real async backend (IOCP/io_uring) is unavailable in this environment";

    EXPECT_TRUE(mock.PostWork());
    EXPECT_TRUE(mock.PostWork(1, "x"));
    EXPECT_EQ(mock.postWorkCalls.load(), 2);

    auto scheduler{ mock.GetScheduler() };
    (void)scheduler;
    EXPECT_EQ(mock.getSchedulerCalls.load(), 1);

    EXPECT_EQ(mock.registerHandleCalls.load(), 0);
    EXPECT_EQ(mock.submitIoCalls.load(), 0);
}

#if HERMES_ENABLE_TLS
TEST_F(MockListenerTest, TlsConnect_TcpSubmissionFails_ReleasesTheSocketHandle) {
    // Exercises TlsAsyncConnectPolicy::Connect DIRECTLY (no facade): when the connect chain
    // fails, the policy's let_error must release the socket handle, because no facade exists
    // to close it in its destructor. Regression for the connected-but-unnegotiated socket leak.
    Hermes::Network::Initialize();
    using TlsData          = Hermes::TlsSocketData<MockExecutionContext>;
    using TlsConnectPolicy = Hermes::TlsAsyncConnectPolicy<MockExecutionContext, TlsData>;

    MockExecutionContext connectMock;
    connectMock.completeSubmissionsWithError = true; // every SubmitIo completes with failure

    TlsData connectData{ MakeLoopbackEndpoint(GetNextPort()), std::string{ "localhost" }, GetMockClientAuth(), &connectMock };

    TlsConnectPolicy policy{};
    // sync_wait requires every error type to be convertible to std::exception_ptr, but the connect
    // chain carries both ConnectionErrorEnum and the std::exception_ptr that stdexec::let_value adds
    // for potentially-throwing functions. Wrap both into a real exception object so the catch blocks
    // below can observe them (same mapping approach as the examples' mapErrorPipeline).
    auto connectSender{ policy.Connect(connectData, TlsConnectPolicy::Options{}) |
        stdexec::let_error([]<typename T>(T&& error) {
            using ErrorType = std::remove_cvref_t<T>;
            std::exception_ptr wrapped;
            if constexpr (std::same_as<ErrorType, std::exception_ptr>)
                wrapped = std::forward<T>(error);
            else
                wrapped = std::make_exception_ptr(std::forward<T>(error));
            return stdexec::just_error(std::move(wrapped));
        }) };
    try {
        (void)stdexec::sync_wait(connectSender);
        ADD_FAILURE() << "expected the TLS connect sender to complete with an error";
    } catch (const Hermes::ConnectionErrorEnum& error) {
        EXPECT_EQ(error, Hermes::ConnectionErrorEnum::ConnectionFailed);
    } catch (...) {
        ADD_FAILURE() << "unexpected error type propagated by the TLS connect sender";
    }

    EXPECT_EQ(connectData.socket, macroINVALID_SOCKET);
    EXPECT_EQ(connectMock.registerHandleCalls.load(), 1);
}
#endif

TEST_F(MockListenerTest, Teardown_CloseAndAbort_DoNotTouchTheContext) {
    MockData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &mock;

    auto completed{ stdexec::sync_wait(MockListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    const auto callsBefore{ mock.registerHandleCalls.load() + mock.submitIoCalls.load() };

    listener.Close();
    EXPECT_EQ(mock.registerHandleCalls.load() + mock.submitIoCalls.load(), callsBefore);

    listener.Abort();
    EXPECT_EQ(mock.registerHandleCalls.load() + mock.submitIoCalls.load(), callsBefore);
    EXPECT_EQ(mock.postWorkCalls.load(), 0);
    EXPECT_EQ(mock.getSchedulerCalls.load(), 0);
}

#endif
