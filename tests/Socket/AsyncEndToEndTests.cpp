#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include <Hermes/Socket/Async/AsyncClientSocket.hpp>
#include <Hermes/Socket/Async/AsyncServerSocket.hpp>
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Socket/Async/_base/ExecutionContext/FastIoExecutionContext.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if HERMES_ENABLE_ASYNC && HERMES_ENABLE_NATIVE_SCHEDULER

using Hermes::AddressFamilyEnum;
using Hermes::ConnectionErrorEnum;
using Hermes::DefaultSocketData;
using Hermes::FastIoLoop;
using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::RawTcpAsyncClient;
using Hermes::RawTcpAsyncListener;
using Hermes::RawTcpAsyncServer;
using Hermes::RecvModeEnum;
using Hermes::SocketTypeEnum;
using Hermes::TransferError;

namespace rg = std::ranges;

namespace {
    std::uint16_t GetNextTcpPort() {
        static std::atomic<std::uint16_t> port{ 30000 };
        return port++;
    }

    std::uint16_t GetNextTlsPort() {
        static std::atomic<std::uint16_t> port{ 31000 };
        return port++;
    }

    IpEndpoint MakeLoopbackEndpoint(const std::uint16_t port) {
        const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
        return IpEndpoint{ loopback, port };
    }

    constexpr int kListenBacklog{ 8 };
    constexpr std::string_view kTlsHost{ "localhost" };

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

    using AsyncData = DefaultSocketData<FastIoLoop, IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>;
}

#pragma region Plain TCP End-to-End

struct AsyncEndToEndTest : testing::Test {
    // The loop is created in SetUp() AFTER the backend probe: FastIoLoop's constructor
    // throws when io_uring is unavailable (Linux CI runners), and a throwing member would
    // fail every test before the body's GTEST_SKIP() could ever run.
    std::optional<FastIoLoop> loop;

    void SetUp() override {
        if (!AsyncBackendAvailable())
            GTEST_SKIP() << "native async backend (IOCP/io_uring) is unavailable in this environment";
        loop.emplace(2);
    }
};

TEST_F(AsyncEndToEndTest, Connect_ListenerListening_ClientAndServerEstablished) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    AsyncData clientData{ endpoint };
    clientData.executionContext = &*loop;

    // The kernel completes the TCP handshake against the listen backlog even before any
    // accept() is called, so connecting first and accepting afterwards is deterministic.
    auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
    ASSERT_TRUE(connected.has_value());
    auto client{ std::move(std::get<0>(*connected)) };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    EXPECT_NE(client.GetSocketData().socket, macroINVALID_SOCKET);
    EXPECT_EQ(server.GetSocketData().executionContext, &*loop);

    server.Close();
    client.Close();
    listener.Close();
}

TEST_F(AsyncEndToEndTest, Send_Recv_RoundTrip_Bidirectional) {
    Hermes::Network::Initialize();
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    AsyncData clientData{ endpoint };
    clientData.executionContext = &*loop;

    auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
    ASSERT_TRUE(connected.has_value());
    auto client{ std::move(std::get<0>(*connected)) };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    constexpr std::string_view message{ "Hello, Hermes!" };

    auto sent{ stdexec::sync_wait(client.Send(message)) };
    ASSERT_TRUE(sent.has_value());
    EXPECT_EQ(std::get<0>(*sent), message.size());

    // RecvModeEnum::All completes only when the WHOLE buffer is filled (same contract as the
    // sync policies). A buffer larger than the incoming message would re-arm the recv and wait
    // for bytes that never arrive, hanging the test forever (regression: the 64-byte buffer
    // here deadlocked this test on Windows/IOCP).
    std::array<std::byte, message.size()> buffer{};
    auto received{ stdexec::sync_wait(server.Recv(buffer, RecvModeEnum::All)) };
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(std::get<0>(*received), message.size());
    EXPECT_TRUE(std::equal(message.begin(), message.end(), reinterpret_cast<const char*>(buffer.data())));

    auto replied{ stdexec::sync_wait(server.Send(std::span{ buffer }.first(message.size()))) };
    ASSERT_TRUE(replied.has_value());
    EXPECT_EQ(std::get<0>(*replied), message.size());

    std::array<std::byte, message.size()> reply{};
    auto back{ stdexec::sync_wait(client.Recv(reply, RecvModeEnum::All)) };
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(std::get<0>(*back), message.size());
    EXPECT_TRUE(std::equal(message.begin(), message.end(), reinterpret_cast<const char*>(reply.data())));

    server.Close();
    client.Close();
    listener.Close();
}

TEST_F(AsyncEndToEndTest, RecvModeAny_CompletesWithFirstAvailableBytes) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    AsyncData clientData{ endpoint };
    clientData.executionContext = &*loop;

    auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
    ASSERT_TRUE(connected.has_value());
    auto client{ std::move(std::get<0>(*connected)) };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    constexpr std::string_view message{ "any-mode" };
    auto sent{ stdexec::sync_wait(client.Send(message)) };
    ASSERT_TRUE(sent.has_value());

    // The pending Recv completes as soon as the first bytes of the message arrive.
    std::array<std::byte, 128> buffer{};
    auto received{ stdexec::sync_wait(server.Recv(buffer, RecvModeEnum::Any)) };
    ASSERT_TRUE(received.has_value());

    const auto receivedCount{ std::get<0>(*received) };
    EXPECT_GT(receivedCount, 0u);
    EXPECT_LE(receivedCount, buffer.size());

    server.Close();
    client.Close();
    listener.Close();
}

TEST_F(AsyncEndToEndTest, Send_LargePayload_AllBytesReceived) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    AsyncData clientData{ endpoint };
    clientData.executionContext = &*loop;

    auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
    ASSERT_TRUE(connected.has_value());
    auto client{ std::move(std::get<0>(*connected)) };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    constexpr std::size_t payloadSize{ 1024 * 1024 };
    std::vector<std::byte> payload(payloadSize);
    for (std::size_t i{}; i < payloadSize; ++i)
        payload[i] = static_cast<std::byte>(i & 0xff);

    // The sender must run on its own thread: a large send stays pending until the peer
    // drains the socket, so doing both sides sequentially on this thread would deadlock.
    std::atomic<bool> allSent{ false };
    std::jthread sender{ [&] {
        auto sent{ stdexec::sync_wait(client.Send(payload)) };
        allSent = sent.has_value() && std::get<0>(*sent) == payloadSize;
    } };

    std::vector<std::byte> received(payloadSize);
    auto recvd{ stdexec::sync_wait(server.Recv(received, RecvModeEnum::All)) };
    ASSERT_TRUE(recvd.has_value());

    sender.join();
    ASSERT_TRUE(allSent.load());
    EXPECT_EQ(std::get<0>(*recvd), payloadSize);
    EXPECT_EQ(received, payload);

    server.Close();
    client.Close();
    listener.Close();
}

TEST_F(AsyncEndToEndTest, ServerClose_ClientRecv_CompletesWithConnectionClosed) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    AsyncData clientData{ endpoint };
    clientData.executionContext = &*loop;

    auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
    ASSERT_TRUE(connected.has_value());
    auto client{ std::move(std::get<0>(*connected)) };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    server.Close();

    bool sawClosed{ false };
    try {
        std::array<std::byte, 64> buffer{};
        (void)stdexec::sync_wait(client.Recv(buffer, RecvModeEnum::Any));
        ADD_FAILURE() << "expected Recv to complete with an error after the peer closed";
    } catch (const TransferError& error) {
        sawClosed = error.error == ConnectionErrorEnum::ConnectionClosed;
    } catch (...) {
        ADD_FAILURE() << "unexpected error type propagated by the Recv sender";
    }
    EXPECT_TRUE(sawClosed);

    client.Close();
    listener.Close();
}

TEST_F(AsyncEndToEndTest, ClientShutdown_ServerRecv_SeesConnectionClosed) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    AsyncData clientData{ endpoint };
    clientData.executionContext = &*loop;

    auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
    ASSERT_TRUE(connected.has_value());
    auto client{ std::move(std::get<0>(*connected)) };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    auto server{ std::move(std::get<0>(*accepted)) };

    auto shutdown{ stdexec::sync_wait(client.Shutdown()) };
    ASSERT_TRUE(shutdown.has_value());

    bool sawClosed{ false };
    try {
        std::array<std::byte, 64> buffer{};
        (void)stdexec::sync_wait(server.Recv(buffer, RecvModeEnum::Any));
        ADD_FAILURE() << "expected Recv to complete with an error after the client shutdown";
    } catch (const TransferError& error) {
        sawClosed = error.error == ConnectionErrorEnum::ConnectionClosed;
    } catch (...) {
        ADD_FAILURE() << "unexpected error type propagated by the Recv sender";
    }
    EXPECT_TRUE(sawClosed);

    server.Close();
    client.Close();
    listener.Close();
}

TEST_F(AsyncEndToEndTest, ConcurrentClients_EchoAllComplete) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTcpPort()) };

    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    constexpr int clientCount{ 8 };
    constexpr std::string_view message{ "ping" };

    std::vector<std::jthread> echoWorkers{};
    echoWorkers.reserve(clientCount);

    std::jthread acceptThread{ [&] {
        for (int i{}; i < clientCount; ++i) {
            auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
            if (!accepted.has_value())
                return;

            echoWorkers.emplace_back([server = std::move(std::get<0>(*accepted))]() mutable {
                std::array<std::byte, 64> buffer{};
                auto received{ stdexec::sync_wait(server.Recv(buffer, RecvModeEnum::Any)) };
                if (!received.has_value())
                    return;

                const auto receivedCount{ std::get<0>(*received) };
                (void)stdexec::sync_wait(server.Send(std::span{ buffer }.first(receivedCount)));
                // `server` closes itself at scope exit.
            });
        }
    } };

    std::atomic<int> successes{ 0 };
    std::vector<std::jthread> clientThreads{};
    clientThreads.reserve(clientCount);

    for (int i{}; i < clientCount; ++i) {
        clientThreads.emplace_back([&]() mutable {
            AsyncData clientData{ endpoint };
            clientData.executionContext = &*loop;

            auto connected{ stdexec::sync_wait(RawTcpAsyncClient::Connect(std::move(clientData))) };
            if (!connected.has_value())
                return;
            auto client{ std::move(std::get<0>(*connected)) };

            auto sent{ stdexec::sync_wait(client.Send(message)) };
            if (!sent.has_value() || std::get<0>(*sent) != message.size())
                return;

            // Exact-size buffer: RecvModeEnum::All fills the whole buffer, and the echo
            // replies with exactly `message.size()` bytes - a larger buffer would hang here.
            std::array<std::byte, message.size()> buffer{};
            auto received{ stdexec::sync_wait(client.Recv(buffer, RecvModeEnum::All)) };
            if (!received.has_value() || std::get<0>(*received) != message.size())
                return;

            if (std::equal(message.begin(), message.end(), reinterpret_cast<const char*>(buffer.data())))
                successes.fetch_add(1);
        });
    }

    for (auto& thread : clientThreads)
        thread.join();
    acceptThread.join();
    for (auto& thread : echoWorkers)
        thread.join();

    EXPECT_EQ(successes.load(), clientCount);
    listener.Close();
}

#pragma endregion

#pragma region TLS End-to-End

#if HERMES_ENABLE_TLS

using Hermes::Credentials;
using Hermes::RawTlsAsyncClient;
using Hermes::RawTlsAsyncListener;
using Hermes::RawTlsAsyncServer;
using Hermes::SChannelCredFlags;
using Hermes::TlsSocketData;

namespace {
    Hermes::Credentials* GetServerAuth() {
        static auto auth{ Hermes::Credentials::Server(std::filesystem::path{ "./cert.pfx" }) };
        if (!auth.has_value()) std::abort();
        return &*auth;
    }

    Hermes::Credentials* GetClientAuth() {
        static auto auth{ Hermes::Credentials::Client(
            SChannelCredFlags::NoServernameCheck       |
            SChannelCredFlags::IgnoreNoRevocationCheck | SChannelCredFlags::IgnoreRevocationOffline |
            SChannelCredFlags::NoDefaultCreds          | SChannelCredFlags::ManualCredValidation    ) };
        if (!auth.has_value()) std::abort();
        return &*auth;
    }

    using TlsAsyncData = TlsSocketData<FastIoLoop, IpEndpoint, SocketTypeEnum::Stream, AddressFamilyEnum::Inet6>;
}

struct TlsAsyncEndToEndTest : testing::Test {
    std::optional<FastIoLoop> loop;
    std::optional<RawTlsAsyncClient> client;
    std::optional<RawTlsAsyncServer> server;

    void SetUp() override {
        if (!AsyncBackendAvailable())
            GTEST_SKIP() << "native async backend (IOCP/io_uring) is unavailable in this environment";
        loop.emplace(2);
    }

    //! Listens, connects a client on a background thread and accepts it (handshakes on both
    //! sides). On success, `client` and `server` are ready to transfer data.
    void EstablishBridge() {
        const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTlsPort()) };

        TlsAsyncData listenData{ endpoint, std::string{ kTlsHost }, GetServerAuth(), &*loop };
        auto listened{ stdexec::sync_wait(RawTlsAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
        ASSERT_TRUE(listened.has_value());
        auto listener{ std::move(std::get<0>(*listened)) };

        TlsAsyncData clientData{ endpoint, std::string{ kTlsHost }, GetClientAuth(), &*loop };
        std::atomic<bool> clientEstablished{ false };
        std::jthread clientThread{ [&] {
            auto connected{ stdexec::sync_wait(RawTlsAsyncClient::Connect(std::move(clientData))) };
            if (connected.has_value()) {
                client.emplace(std::move(std::get<0>(*connected)));
                clientEstablished = true;
            }
        } };

        // The TLS handshake needs both ends concurrently: the client drives the handshake
        // from its thread while this thread accepts (running the server-side handshake).
        auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
        ASSERT_TRUE(accepted.has_value());
        server.emplace(std::move(std::get<0>(*accepted)));

        clientThread.join();
        ASSERT_TRUE(clientEstablished.load());
        listener.Close();
    }
};

TEST_F(TlsAsyncEndToEndTest, Connect_Accept_HandshakeSucceeds) {
    ASSERT_NO_FATAL_FAILURE(EstablishBridge());

    EXPECT_NE(client->GetSocketData().socket, macroINVALID_SOCKET);
    EXPECT_EQ(server->GetSocketData().executionContext, &*loop);

    client->Close();
    server->Close();
}

TEST_F(TlsAsyncEndToEndTest, Send_Recv_RoundTrip_Bidirectional) {
    ASSERT_NO_FATAL_FAILURE(EstablishBridge());

    constexpr std::string_view message{ "secret message" };

    auto sent{ stdexec::sync_wait(client->Send(message)) };
    ASSERT_TRUE(sent.has_value());
    EXPECT_EQ(std::get<0>(*sent), message.size());

    // Exact-size buffer: RecvModeEnum::All completes only when the whole buffer is filled,
    // and the peer sends exactly one message (regression: a 64-byte buffer hung this test).
    std::array<std::byte, message.size()> buffer{};
    auto received{ stdexec::sync_wait(server->Recv(buffer, RecvModeEnum::All)) };
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(std::get<0>(*received), message.size());
    EXPECT_TRUE(std::equal(message.begin(), message.end(), reinterpret_cast<const char*>(buffer.data())));

    auto replied{ stdexec::sync_wait(server->Send(std::span{ buffer }.first(message.size()))) };
    ASSERT_TRUE(replied.has_value());

    std::array<std::byte, message.size()> reply{};
    auto back{ stdexec::sync_wait(client->Recv(reply, RecvModeEnum::All)) };
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(std::get<0>(*back), message.size());
    EXPECT_TRUE(std::equal(message.begin(), message.end(), reinterpret_cast<const char*>(reply.data())));

    client->Close();
    server->Close();
}

TEST_F(TlsAsyncEndToEndTest, Send_LargePayload_AllBytesReceived) {
    ASSERT_NO_FATAL_FAILURE(EstablishBridge());

    constexpr std::size_t payloadSize{ 256 * 1024 };
    std::vector<std::byte> payload(payloadSize);
    for (std::size_t i{}; i < payloadSize; ++i)
        payload[i] = static_cast<std::byte>(i & 0xff);

    std::atomic<bool> allSent{ false };
    std::jthread sender{ [&] {
        auto sent{ stdexec::sync_wait(client->Send(payload)) };
        allSent = sent.has_value() && std::get<0>(*sent) == payloadSize;
    } };

    std::vector<std::byte> received(payloadSize);
    auto recvd{ stdexec::sync_wait(server->Recv(received, RecvModeEnum::All)) };
    ASSERT_TRUE(recvd.has_value());

    sender.join();
    ASSERT_TRUE(allSent.load());
    EXPECT_EQ(std::get<0>(*recvd), payloadSize);
    EXPECT_EQ(received, payload);

    client->Close();
    server->Close();
}

TEST_F(TlsAsyncEndToEndTest, ServerClose_ClientRecv_CompletesWithConnectionClosed) {
    ASSERT_NO_FATAL_FAILURE(EstablishBridge());

    server->Close();

    bool sawClosed{ false };
    try {
        std::array<std::byte, 64> buffer{};
        (void)stdexec::sync_wait(client->Recv(buffer, RecvModeEnum::Any));
        ADD_FAILURE() << "expected Recv to complete with an error after the peer closed";
    } catch (const TransferError& error) {
        sawClosed = error.error == ConnectionErrorEnum::ConnectionClosed;
    } catch (...) {
        ADD_FAILURE() << "unexpected error type propagated by the Recv sender";
    }
    EXPECT_TRUE(sawClosed);

    client->Close();
}

TEST_F(TlsAsyncEndToEndTest, Connect_NonTlsListener_HandshakeFails) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextTlsPort()) };

    // Plain TCP listener: the TLS client completes the TCP connect but the handshake cannot
    // succeed. There is no handshake timeout on async policies yet, so this thread relies on
    // the peer closing to release the pending handshake receive.
    AsyncData listenData{ endpoint };
    listenData.executionContext = &*loop;

    auto listened{ stdexec::sync_wait(RawTcpAsyncListener::Listen(std::move(listenData), kListenBacklog)) };
    ASSERT_TRUE(listened.has_value());
    auto listener{ std::move(std::get<0>(*listened)) };

    TlsAsyncData clientData{ endpoint, std::string{ kTlsHost }, GetClientAuth(), &*loop };
    std::atomic<bool> connectFailed{ false };
    std::jthread clientThread{ [&] {
        try {
            (void)stdexec::sync_wait(RawTlsAsyncClient::Connect(std::move(clientData)));
        } catch (const ConnectionErrorEnum&) {
            connectFailed = true;
        } catch (...) {
        }
    } };

    auto accepted{ stdexec::sync_wait(listener.AsyncAcceptOne()) };
    ASSERT_TRUE(accepted.has_value());
    {
        // Drop the accepted plain TCP socket so the client's pending handshake receive hits
        // EOF (or RST) and fails instead of waiting forever.
        auto plain{ std::move(std::get<0>(*accepted)) };
        plain.Close();
    }

    clientThread.join();
    EXPECT_TRUE(connectFailed.load());
    listener.Close();
}

#endif // HERMES_ENABLE_TLS

#pragma endregion

#endif
