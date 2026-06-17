#include <gtest/gtest.h>
#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Socket/Sync/ListenerSocket.hpp>
#include <Hermes/Socket/Sync/ServerSocket.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <atomic>
#include <optional>

using Hermes::ConnectionErrorEnum;
using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::RawTlsClient;
using Hermes::RawTlsListener;
using Hermes::RawTlsServer;
using Hermes::TlsSocketData;
using Hermes::SChannelCredFlags;

namespace rg = std::ranges;
using namespace std::chrono;
using namespace std::chrono_literals;

#pragma region Utils & Factories

static std::uint16_t I_GetNextPort() {
    static std::atomic<std::uint16_t> s_port{ 20000 };
    return s_port++;
}

static IpEndpoint I_MakeEndpoint(const std::uint16_t port) {
    const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    return IpEndpoint{ loopback, port };
}

static Hermes::Credentials* I_GetServerAuth() {
    static auto auth{ Hermes::Credentials::Server(std::filesystem::path{ "./cert.pfx" }) };
    if (!auth.has_value()) std::abort();
    return &*auth;
}

static Hermes::Credentials* I_GetClientAuth() {
    static auto auth{ Hermes::Credentials::Client(
        SChannelCredFlags::NoServernameCheck |
        SChannelCredFlags::IgnoreNoRevocationCheck | SChannelCredFlags::IgnoreRevocationOffline |
        SChannelCredFlags::NoDefaultCreds | SChannelCredFlags::ManualCredValidation) };
    if (!auth.has_value()) std::abort();
    return &*auth;
}

static constexpr std::string_view kTlsHost{ "localhost" };

static auto I_MakeListenSocket(std::uint16_t port, Hermes::TlsAcceptPolicy<>::ListenOptions options = {}) {
    return RawTlsListener::Listen({ I_MakeEndpoint(port), std::string{ kTlsHost }, I_GetServerAuth() }, options);
}

static auto I_MakeClientSocket(std::uint16_t port, Hermes::TlsConnectPolicy<>::Options options = {}) {
    return RawTlsClient::Connect({ I_MakeEndpoint(port), std::string{ kTlsHost }, I_GetClientAuth() }, options);
}

#pragma endregion

#pragma region Server Lifecycle & Timeouts

struct TlsListenerSocketTest : testing::Test {};

TEST_F(TlsListenerSocketTest, Listen_ValidLoopbackEndpoint_Succeeds) {
    EXPECT_TRUE(I_MakeListenSocket(I_GetNextPort()).has_value());
}

TEST_F(TlsListenerSocketTest, Listen_PortAlreadyBound_ReturnsAddressInUse) {
    const std::uint16_t port{ I_GetNextPort() };
    auto first{ I_MakeListenSocket(port, {{ .reuseAddress = false }}) };
    ASSERT_TRUE(first.has_value());

    const auto second{ I_MakeListenSocket(port) };
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), ConnectionErrorEnum::AddressInUse);
}

TEST_F(TlsListenerSocketTest, Listen_InvalidAddress_ReturnsError) {
    const IpAddress foreignIp{ IpAddress::FromIpv4({8, 8, 8, 8}) };
    const auto result{ RawTlsListener::Listen({ IpEndpoint{ foreignIp, I_GetNextPort() }, std::string{ kTlsHost }, I_GetServerAuth() }) };
    ASSERT_FALSE(result.has_value());
}

TEST_F(TlsListenerSocketTest, ListenOne_ValidEndpoint_Succeeds) {
    auto result{ RawTlsListener::ListenOne({ I_MakeEndpoint(I_GetNextPort()), std::string{ kTlsHost }, I_GetServerAuth() }) };
    EXPECT_TRUE(result.has_value());
}

TEST_F(TlsListenerSocketTest, AcceptAll_YieldsServerSocketPerClient) {
    const std::uint16_t port{ I_GetNextPort() };
    auto listener{ I_MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    constexpr int clientCount{ 3 };
    std::atomic<int> accepted{ 0 };

    std::jthread acceptThread{ [&]() {
        for (auto&& result : listener->AcceptAll()) {
            if (!result) break;
            if (++accepted >= clientCount)
                listener->Close();
        }
    }};

    for (int i{}; i < clientCount; ++i) {
        auto _{ I_MakeClientSocket(port) };
    }

    acceptThread.join();
    EXPECT_EQ(accepted.load(), clientCount);
}

TEST_F(TlsListenerSocketTest, Concurrent_Clients_StressTest) {
    const std::uint16_t port{ I_GetNextPort() };
    auto listener{ I_MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    constexpr int numClients{ 15 };
    std::atomic<int> successfulHandshakes{ 0 };

    std::jthread acceptThread{ [&]() {
        for (int i{}; i < numClients; ++i) {
            if (listener->AcceptOne().has_value()) ++successfulHandshakes;
        }
    }};

    std::vector<std::jthread> clientThreads{};
    for (int i{}; i < numClients; ++i) {
        clientThreads.emplace_back([&]() { EXPECT_TRUE(I_MakeClientSocket(port).has_value()); });
    }

    for (auto& t : clientThreads) t.join();
    acceptThread.join();

    EXPECT_EQ(successfulHandshakes.load(), numClients);
}

TEST_F(TlsListenerSocketTest, AcceptOne_HandshakeTimeout_AbortsTlsEarly) {
    const std::uint16_t port{ I_GetNextPort() };
    auto listener{ I_MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    std::jthread tarpit{ [&]() {
        auto tcpClient{ Hermes::RawTcpClient::Connect(Hermes::DefaultSocketData<>{ I_MakeEndpoint(port) }) };
        std::this_thread::sleep_for(500ms);
    }};

    auto start{ steady_clock::now() };
    const auto result{ listener->AcceptOne({ .handshakeTimeout = 50ms }) };
    auto elapsed{ duration_cast<milliseconds>(steady_clock::now() - start) };

    EXPECT_FALSE(result.has_value());
    EXPECT_LT(elapsed, 400ms);
}

#pragma endregion

#pragma region Client Lifecycle & Timeouts

struct TlsClientSocketTest : testing::Test {};

TEST_F(TlsClientSocketTest, Connect_NoListener_ReturnsConnectionFailed) {
    const auto result{ I_MakeClientSocket(I_GetNextPort()) };
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::ConnectionFailed);
}

TEST_F(TlsClientSocketTest, Connect_NonTlsListener_ReturnsHandshakeFailed) {
    const std::uint16_t port{ I_GetNextPort() };
    auto listener{ Hermes::RawTcpListener::Listen(Hermes::DefaultSocketData<>{ I_MakeEndpoint(port) }) };
    ASSERT_TRUE(listener.has_value());

    std::jthread{ [&]() { auto _{ listener->AcceptOne() }; } }.detach();

    const auto result{ I_MakeClientSocket(port) };
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::HandshakeFailed);
}

TEST_F(TlsClientSocketTest, MoveConstruct_PreservesUsability) {
    const std::uint16_t port{ I_GetNextPort() };
    auto listener{ I_MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTlsServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto original{ I_MakeClientSocket(port) };
    ASSERT_TRUE(original.has_value());

    RawTlsClient moved{ std::move(*original) };
    acceptThread.join();
    ASSERT_TRUE(serverResult.has_value());

    constexpr std::string_view payload{ "ping" };
    const auto [sent, sendErr]{ moved.Send(payload) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, payload.size());
}

TEST_F(TlsClientSocketTest, Connect_ConnectionTimeout_AbortsTcpEarly) {
    const IpEndpoint blackhole{ IpAddress::FromIpv4({ 192, 0, 2, 1 }), 443 };

    const auto start{ steady_clock::now() };
    const auto result{ RawTlsClient::Connect({ blackhole, std::string{ kTlsHost }, I_GetClientAuth() }, {{
            .connectionTimeout = 100ms
        }}) };
    const auto elapsed{ duration_cast<milliseconds>(steady_clock::now() - start) };

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::ConnectionFailed);
    EXPECT_LT(elapsed, 1s);
}

TEST_F(TlsClientSocketTest, Connect_HandshakeTimeout_AbortsTlsEarly) {
    const std::uint16_t port{ I_GetNextPort() };

    auto tcpListener{ Hermes::RawTcpListener::Listen(Hermes::DefaultSocketData<>{ I_MakeEndpoint(port) }) };
    ASSERT_TRUE(tcpListener.has_value());

    std::jthread tarpit{ [&]() {
        if (auto tcpCli{ tcpListener->AcceptOne() }) {
            std::this_thread::sleep_for(milliseconds{ 500 });
        }
    }};

    auto start{ steady_clock::now() };
    const auto result{ I_MakeClientSocket(port, { .handshakeTimeout = 50ms }) };
    auto elapsed{ duration_cast<milliseconds>(steady_clock::now() - start) };

    EXPECT_FALSE(result.has_value());
    EXPECT_LT(elapsed, 400ms);
}

#pragma endregion

#pragma region Transfer Bridge (Fixtured)

struct TlsSocketBridgeFixture : testing::Test {
    std::optional<RawTlsClient> client{};
    std::optional<RawTlsServer> server{};

    void SetUp() override {
        const std::uint16_t port{ I_GetNextPort() };
        auto listener{ I_MakeListenSocket(port) };
        ASSERT_TRUE(listener.has_value());

        std::jthread acceptThread{[&]() {
            if (auto srv{ listener->AcceptOne() }) server.emplace(std::move(*srv));
        }};

        auto cli{ I_MakeClientSocket(port) };
        ASSERT_TRUE(cli.has_value());

        acceptThread.join();
        ASSERT_TRUE(server.has_value());
        client.emplace(std::move(*cli));
    }

    void TearDown() override {
        if (client) client->Close();
        if (server) server->Close();
    }
};

TEST_F(TlsSocketBridgeFixture, ClientSend_ServerRecv_SmallMessage) {
    constexpr std::string_view outBuf{ "hello" };
    const auto [sent, sendErr]{ client->Send(outBuf) };
    EXPECT_TRUE(sendErr.has_value());

    std::string inBuf(outBuf.size(), '\0');
    const auto [recvd, recvErr]{ server->Recv(inBuf) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(inBuf, "hello");
}

TEST_F(TlsSocketBridgeFixture, ServerSend_ClientRecv_SmallMessage) {
    constexpr std::string_view outBuf{ "world" };
    const auto [sent, sendErr]{ server->Send(outBuf) };
    EXPECT_TRUE(sendErr.has_value());

    std::string inBuf(outBuf.size(), '\0');
    const auto [recvd, recvErr]{ client->Recv(inBuf) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(inBuf, "world");
}

TEST_F(TlsSocketBridgeFixture, Send_LargePayload_AllBytesReceived) {
    constexpr std::size_t payloadSize{ 64 * 1024 };
    std::vector<std::byte> outBuf{};
    outBuf.resize(payloadSize);

    for (std::size_t i{}; i < payloadSize; ++i) {
        outBuf[i] = static_cast<std::byte>(i & 0xff);
    }

    EXPECT_TRUE(client->Send(outBuf).second.has_value());

    std::vector<std::byte> inBuf{};
    inBuf.resize(payloadSize);

    std::size_t totalRecvd{};
    while (totalRecvd < payloadSize) {
        std::span slice{ std::span{ inBuf }.subspan(totalRecvd) };
        const auto [n, err]{ server->Recv(slice) };
        ASSERT_TRUE(err.has_value());
        totalRecvd += n;
    }

    EXPECT_EQ(inBuf, outBuf);
}

TEST_F(TlsSocketBridgeFixture, RecvRange_ClientSend_ServerIterates) {
    constexpr std::string_view message{ "stream test" };
    EXPECT_TRUE(client->Send(message).second.has_value());
    client->Close();

    std::string received{};
    received.append_range(server->RecvRange<char>());
    EXPECT_EQ(received, message);
}

TEST_F(TlsSocketBridgeFixture, MultipleMessages_InSequence_AllReceived) {
    using std::literals::operator""sv;
    constexpr std::array messages{ "first"sv, "second"sv, "third"sv };

    for (auto msg : messages) {
        EXPECT_TRUE(client->Send(msg).second.has_value());
        std::string inBuf(msg.size(), '\0');
        EXPECT_TRUE(server->Recv(inBuf).second.has_value());
        EXPECT_EQ(inBuf, msg);
    }
}

TEST_F(TlsSocketBridgeFixture, ServerClose_ClientSend_ReturnsError) {
    server->Abort();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

    constexpr std::string_view payload{ "after close" };
    const auto [sent, sendErr]{ client->Send(payload) };
    EXPECT_FALSE(sendErr.has_value());
}

TEST_F(TlsSocketBridgeFixture, ClientClose_ServerRecv_HandlesGracefully) {
    client->Close();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

    std::vector<std::byte> inBuf{};
    inBuf.resize(10);
    const auto [recvd, recvErr]{ server->Recv(inBuf) };

    if (recvErr.has_value()) {
        EXPECT_EQ(recvd, 0);
    } else {
        EXPECT_EQ(recvErr.error(), ConnectionErrorEnum::ConnectionClosed);
    }
}

#pragma endregion