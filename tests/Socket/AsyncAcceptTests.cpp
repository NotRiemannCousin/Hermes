#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>

#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_NATIVE_SCHEDULER

using Hermes::AddressFamilyEnum;
using Hermes::DefaultSocketData;
using Hermes::FastIoLoop;
using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::RawTcpAsyncListener;
using Hermes::RawTcpClient;
using Hermes::SocketTypeEnum;

namespace {
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

    std::uint16_t GetNextPort() {
        static std::atomic<std::uint16_t> port{ 21000 };
        return port++;
    }

    IpEndpoint MakeLoopbackEndpoint(const std::uint16_t port) {
        const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
        return IpEndpoint{ loopback, port };
    }

    using AsyncSocketData = DefaultSocketData<FastIoLoop, IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>;
    using AsyncListenOptions = Hermes::DefaultAcceptPolicy<IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>::ListenOptions;

    constexpr int kListenBacklog{ 8 };
}

struct AsyncListenerTest : testing::Test {
    // The loop is created in SetUp() AFTER the backend probe: FastIoLoop's constructor
    // throws when io_uring is unavailable (Linux CI runners), and a throwing member would
    // fail every test before the body's GTEST_SKIP() could ever run.
    std::optional<FastIoLoop> loop;

    void SetUp() override {
        if (!AsyncBackendAvailable())
            GTEST_SKIP() << "native async backend (IOCP/io_uring) is unavailable in this environment";
        loop.emplace(2);
    }

    static void ConnectClientInBackground(const IpEndpoint& endpoint) {
        std::jthread clientThread{ [&] {
            auto result{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
            EXPECT_TRUE(result.has_value());
        } };
        clientThread.detach();
    }
};

TEST_F(AsyncListenerTest, Listen_CompletesWithUsableListener) {
    AsyncSocketData listenData{ MakeLoopbackEndpoint(GetNextPort()) };
    listenData.executionContext = &*loop;

    auto completed{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());

    auto listener{ std::move(std::get<0>(*completed)) };
    listener.Close();
}

TEST_F(AsyncListenerTest, Listen_SecondListenerOnSamePort_WithoutReuse_Fails) {
    const std::uint16_t port{ GetNextPort() };
    AsyncSocketData firstData{ MakeLoopbackEndpoint(port) };
    firstData.executionContext = &*loop;

    auto first{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(firstData), kListenBacklog)) };
    ASSERT_TRUE(first.has_value());
    auto listener{ std::move(std::get<0>(*first)) };

    EXPECT_ANY_THROW({
        AsyncSocketData secondData{ MakeLoopbackEndpoint(port) };
        secondData.executionContext = &*loop;

        stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(secondData), AsyncListenOptions{ .reuseAddress = false }, kListenBacklog));
    });

    listener.Close();
}

TEST_F(AsyncListenerTest, Listen_NoExecutionContext_Fails) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextPort()) };

    EXPECT_ANY_THROW({
        stdexec::sync_wait(RawTcpAsyncListener::Listen(AsyncSocketData{ endpoint }, kListenBacklog));
    });
}

TEST_F(AsyncListenerTest, AcceptOne_ConnectedClient_CompletesAndPropagatesContext) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextPort()) };
    AsyncSocketData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto completed{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    ConnectClientInBackground(endpoint);

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    EXPECT_EQ(server.GetSocketData().executionContext, &*loop);

    server.Close();
    listener.Close();
}

TEST_F(AsyncListenerTest, AcceptOne_AbortWhilePending_FailsTheAccept) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextPort()) };
    AsyncSocketData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto completed{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    listener.Abort();

    EXPECT_ANY_THROW({
        stdexec::sync_wait(listener.AsyncAcceptOne());
    });

    listener.Close();
}

TEST_F(AsyncListenerTest, AcceptOne_SequentialClients_AllAccepted) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextPort()) };
    AsyncSocketData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto completed{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(completed.has_value());
    auto listener{ std::move(std::get<0>(*completed)) };

    for (int i{}; i < 3; ++i) {
        ConnectClientInBackground(endpoint);
        auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
        ASSERT_TRUE(accepted.has_value());
        auto server{ std::move(std::get<0>(*accepted)) };
        EXPECT_EQ(server.GetSocketData().executionContext, &*loop);
        server.Close();
    }

    listener.Close();
}

#endif
