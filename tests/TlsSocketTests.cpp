// ReSharper disable CppPassValueParameterByConstReference
#include <gtest/gtest.h>
#include <../include/Hermes/Socket/Sync/ClientSocket.hpp>
#include <../include/Hermes/Socket/Sync/ListenerSocket.hpp>
#include <../include/Hermes/Socket/Sync/ServerSocket.hpp>

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

namespace rg = std::ranges;

#pragma region Utils

static uint16_t GetNextPort() {
    static std::atomic<uint16_t> s_port{ 20000 };
    return s_port++;
}

static IpEndpoint MakeEndpoint(const std::uint16_t port) {
    const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    return IpEndpoint{ loopback, port };
}

static Hermes::Credentials* GetServerAuth() {
    static auto auth{ Hermes::Credentials::Server(std::filesystem::path{ "./cert.pfx" }) };
    if (!auth.has_value()) std::abort();
    return &*auth;
}

static Hermes::Credentials* GetClientAuth() {
    static auto auth{ Hermes::Credentials::Client(
        SChannelCredFlags::NoServernameCheck |
        SChannelCredFlags::IgnoreNoRevocationCheck | SChannelCredFlags::IgnoreRevocationOffline |
        SChannelCredFlags::NoDefaultCreds | SChannelCredFlags::ManualCredValidation) };
    if (!auth.has_value()) std::abort();
    return &*auth;
}

static constexpr std::string_view kTlsHost{ "localhost" };

Hermes::ConnectionResult<RawTlsListener> MakeListenSocket(uint16_t port, Hermes::TlsAcceptPolicy<>::ListenOptions options = {}) {
    return RawTlsListener::Listen({ MakeEndpoint(port), std::string{ kTlsHost }, GetServerAuth() }, options);
}

Hermes::ConnectionResult<RawTlsClient> MakeClientSocket(uint16_t port, Hermes::TlsConnectPolicy<>::Options options = {}) {
    return RawTlsClient::Connect({ MakeEndpoint(port), std::string{ kTlsHost }, GetClientAuth() }, options);
}

#pragma endregion


#pragma region Server

struct TlsListenerSocketTest : testing::Test {};

TEST_F(TlsListenerSocketTest, Listen_ValidLoopbackEndpoint_Succeeds) {
    EXPECT_TRUE(MakeListenSocket(GetNextPort()).has_value());
}

TEST_F(TlsListenerSocketTest, Listen_PortAlreadyBound_ReturnsAddressInUse) {
    const uint16_t port{ GetNextPort() };
    auto first{ MakeListenSocket(port, {{ .reuseAddress = false }}) };
    ASSERT_TRUE(first.has_value());

    const auto second{ MakeListenSocket(port) };
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), ConnectionErrorEnum::AddressInUse);
}

TEST_F(TlsListenerSocketTest, Listen_InvalidAddress_ReturnsError) {
    const IpAddress foreignIp{ IpAddress::FromIpv4({8, 8, 8, 8}) };
    const auto result{ RawTlsListener::Listen({ IpEndpoint{ foreignIp, GetNextPort() }, std::string{ kTlsHost }, GetServerAuth() }) };

    ASSERT_FALSE(result.has_value());
}

TEST_F(TlsListenerSocketTest, ListenOne_ValidEndpoint_Succeeds) {
    auto result{ RawTlsListener::ListenOne({ MakeEndpoint(GetNextPort()), std::string{ kTlsHost }, GetServerAuth() }) };
    EXPECT_TRUE(result.has_value());
}

TEST_F(TlsListenerSocketTest, Close_DoesNotCrash) {
    auto listener{ MakeListenSocket(GetNextPort()) };
    ASSERT_TRUE(listener.has_value());
    EXPECT_NO_FATAL_FAILURE(listener->Close());
}

TEST_F(TlsListenerSocketTest, AcceptOne_ConnectedClient_HandshakeCompletes) {
    const uint16_t port{ GetNextPort() };
    auto listener{ MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTlsServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    const auto clientResult{ MakeClientSocket(port) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    EXPECT_TRUE(serverResult.has_value());
}

TEST_F(TlsListenerSocketTest, AcceptAll_YieldsServerSocketPerClient) {
    const uint16_t port{ GetNextPort() };
    auto listener{ MakeListenSocket(port) };
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

    for (int i{}; i < clientCount; ++i)
        auto _{ MakeClientSocket(port) };

    acceptThread.join();
    EXPECT_EQ(accepted.load(), clientCount);
}

TEST_F(TlsListenerSocketTest, Concurrent_Clients_StressTest) {
    const uint16_t port{ GetNextPort() };
    auto listener{ MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    constexpr int numClients{ 15 };
    std::atomic<int> successfulHandshakes{ 0 };

    std::jthread acceptThread{ [&]() {
        for (int i{}; i < numClients; ++i) {
            if (listener->AcceptOne().has_value()) ++successfulHandshakes;
        }
    }};

    std::vector<std::jthread> clientThreads;
    for (int i{}; i < numClients; ++i) {
        clientThreads.emplace_back([&]() { EXPECT_TRUE(MakeClientSocket(port).has_value()); });
    }

    for (auto& t : clientThreads) t.join();
    acceptThread.join();

    EXPECT_EQ(successfulHandshakes.load(), numClients);
}

#pragma endregion


#pragma region Client

struct TlsClientSocketTest : testing::Test {};

TEST_F(TlsClientSocketTest, Connect_ValidLoopbackListener_Succeeds) {
    const uint16_t port{ GetNextPort() };
    auto listener{ MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    std::jthread{ [&]() { auto _{ listener->AcceptOne() }; } }.detach();

    EXPECT_TRUE(MakeClientSocket(port).has_value());
}

TEST_F(TlsClientSocketTest, Connect_NoListener_ReturnsConnectionFailed) {
    const auto result{ MakeClientSocket(GetNextPort()) };
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::ConnectionFailed);
}

TEST_F(TlsClientSocketTest, Connect_NonTlsListener_ReturnsHandshakeFailed) {
    const uint16_t port{ GetNextPort() };
    auto listener{ Hermes::RawTcpListener::Listen(Hermes::DefaultSocketData<>{ MakeEndpoint(port) }) };
    ASSERT_TRUE(listener.has_value());

    std::jthread{ [&]() { auto _{ listener->AcceptOne() }; } }.detach();

    const auto result{ MakeClientSocket(port) };
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::HandshakeFailed);
}

TEST_F(TlsClientSocketTest, MoveConstruct_PreservesUsability) {
    const uint16_t port{ GetNextPort() };
    auto listener{ MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTlsServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto original{ MakeClientSocket(port) };
    ASSERT_TRUE(original.has_value());

    RawTlsClient moved{ std::move(*original) };
    acceptThread.join();
    ASSERT_TRUE(serverResult.has_value());

    constexpr std::string_view payload{ "ping" };
    const auto [sent, sendErr]{ moved.Send(payload) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, payload.size());
}

#pragma endregion


#pragma region Bridge

struct TlsSocketBridgeTest : testing::Test {
    std::optional<RawTlsClient> client;
    std::optional<RawTlsServer> server;

    void SetUp() override {
        const uint16_t port{ GetNextPort() };

        auto listener{ MakeListenSocket(port) };
        ASSERT_TRUE(listener.has_value()) << "Setup Listen failed";

        std::jthread acceptThread{[&]() {
            if (auto srv{ listener->AcceptOne() }) server.emplace(std::move(*srv));
        }};

        auto cli{ MakeClientSocket(port) };
        ASSERT_TRUE(cli.has_value()) << "Setup Connect failed";

        acceptThread.join();
        ASSERT_TRUE(server.has_value()) << "Setup Accept failed";

        client.emplace(std::move(*cli));
    }
};

TEST_F(TlsSocketBridgeTest, ClientSend_ServerRecv_SmallMessage) {
    constexpr std::string_view outBuf{ "hello" };
    const auto [sent, sendErr]{ client->Send(outBuf) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, outBuf.size());

    std::string inBuf(outBuf.size(), '\0');
    const auto [recvd, recvErr]{ server->Recv(inBuf) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(recvd, inBuf.size());
    EXPECT_EQ(inBuf, "hello");
}

TEST_F(TlsSocketBridgeTest, ServerSend_ClientRecv_SmallMessage) {
    constexpr std::string_view outBuf{ "world" };
    const auto [sent, sendErr]{ server->Send(outBuf) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, outBuf.size());


    std::string inBuf(outBuf.size(), '\0');
    const auto [recvd, recvErr]{ client->Recv(inBuf) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(recvd, inBuf.size());
    EXPECT_EQ(inBuf, "world");

    server->Close();
}

TEST_F(TlsSocketBridgeTest, Bidirectional_PingPong) {
    constexpr std::string_view ping{ "ping" };
    constexpr std::string_view pong{ "pong" };

    EXPECT_TRUE(client->Send(ping).second.has_value());

    std::string recvPing(ping.size(), '\0');
    EXPECT_TRUE(server->Recv(recvPing).second.has_value());
    EXPECT_EQ(recvPing, ping);

    EXPECT_TRUE(server->Send(pong).second.has_value());

    std::string recvPong(pong.size(), '\0');
    EXPECT_TRUE(client->Recv(recvPong).second.has_value());
    EXPECT_EQ(recvPong, pong);
}

TEST_F(TlsSocketBridgeTest, Send_LargePayload_AllBytesReceived) {
    constexpr std::size_t payloadSize{ 64 * 1024 };
    std::vector<std::byte> outBuf(payloadSize);
    for (std::size_t i{}; i < payloadSize; ++i)
        outBuf[i] = static_cast<std::byte>(i & 0xff);

    EXPECT_TRUE(client->Send(outBuf).second.has_value());

    std::vector<std::byte> inBuf(payloadSize);
    std::size_t totalRecvd{};
    while (totalRecvd < payloadSize) {
        std::span slice{ std::span{ inBuf }.subspan(totalRecvd) };
        const auto [n, err]{ server->Recv(slice) };
        ASSERT_TRUE(err.has_value());
        ASSERT_GT(n, 0u);
        totalRecvd += n;
    }

    EXPECT_EQ(inBuf, outBuf);
}

TEST_F(TlsSocketBridgeTest, RecvRange_ClientSend_ServerIterates) {
    constexpr std::string_view message{ "stream test" };

    EXPECT_TRUE(client->Send(message).second.has_value());
    client->Close();

    std::string received;
    received.append_range(server->RecvRange<char>());

    EXPECT_EQ(received, message);
}

TEST_F(TlsSocketBridgeTest, MultipleMessages_InSequence_AllReceived) {
    using std::literals::operator""sv;
    constexpr std::array messages{ "first"sv, "second"sv, "third"sv };

    for (auto msg : messages) {
        EXPECT_TRUE(client->Send(msg).second.has_value());

        std::string inBuf(msg.size(), '\0');
        EXPECT_TRUE(server->Recv(inBuf).second.has_value());
        EXPECT_EQ(inBuf, msg);
    }
}

TEST_F(TlsSocketBridgeTest, ServerClose_ClientSend_ReturnsError) {
    server->Abort();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

    constexpr std::string_view payload{ "after close" };
    const auto [sent, sendErr]{ client->Send(payload) };

    EXPECT_FALSE(sendErr.has_value());
}

TEST_F(TlsSocketBridgeTest, ClientClose_ServerRecv_HandlesGracefully) {
    client->Close();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

    std::vector<std::byte> inBuf(10);
    const auto [recvd, recvErr]{ server->Recv(inBuf) };

    if (recvErr.has_value()) {
        EXPECT_EQ(recvd, 0);
    } else {
        EXPECT_EQ(recvErr.error(), ConnectionErrorEnum::ConnectionClosed);
    }
}

#pragma endregion


#pragma region Timeouts

using namespace std::chrono;
using namespace std::chrono_literals;

TEST_F(TlsClientSocketTest, Connect_ConnectionTimeout_AbortsTcpEarly) {
    // 192.0.2.1 is reserved (TEST-NET-1), it's a blackhole (causes packet drop)
    const IpEndpoint blackhole{ IpAddress::FromIpv4({ 192, 0, 2, 1 }), 443 };

    const auto start{ steady_clock::now() };
    const auto result{ RawTlsClient::Connect({ blackhole, std::string{ kTlsHost }, GetClientAuth() }, {{
            .connectionTimeout = 100ms
        }}) };
    const auto elapsed{ duration_cast<milliseconds>(steady_clock::now() - start) };

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::ConnectionFailed);

    EXPECT_LT(elapsed, 1s);
}

TEST_F(TlsClientSocketTest, Connect_HandshakeTimeout_AbortsTlsEarly) {
    const uint16_t port{ GetNextPort() };

    auto tcpListener{ Hermes::RawTcpListener::Listen(Hermes::DefaultSocketData<>{ MakeEndpoint(port) }) };
    ASSERT_TRUE(tcpListener.has_value());

    std::jthread tarpit{ [&]() {
        if (auto client{ tcpListener->AcceptOne() }) {
            std::this_thread::sleep_for(milliseconds{ 500 });
        }
    }};

    auto start{ steady_clock::now() };
    const auto result{ MakeClientSocket(port, { .handshakeTimeout = 50ms }) };
    auto elapsed{ duration_cast<milliseconds>(steady_clock::now() - start) };

    EXPECT_FALSE(result.has_value());
    EXPECT_LT(elapsed, 400ms);
}

TEST_F(TlsListenerSocketTest, AcceptOne_HandshakeTimeout_AbortsTlsEarly) {
    const uint16_t port{ GetNextPort() };
    auto listener{ MakeListenSocket(port) };
    ASSERT_TRUE(listener.has_value());

    std::jthread tarpit{ [&]() {
        auto tcpClient{ Hermes::RawTcpClient::Connect(Hermes::DefaultSocketData<>{ MakeEndpoint(port) }) };
        std::this_thread::sleep_for(500ms);
    }};

    auto start{ steady_clock::now() };
    const auto result{ listener->AcceptOne({ .handshakeTimeout = 50ms }) };
    auto elapsed{ duration_cast<milliseconds>(steady_clock::now() - start) };

    EXPECT_FALSE(result.has_value());
    EXPECT_LT(elapsed, 400ms);
}

#pragma endregion