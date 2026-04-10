#include <gtest/gtest.h>
#include <Hermes/Socket/ClientSocket.hpp>
#include <Hermes/Socket/ListenerSocket.hpp>
#include <Hermes/Socket/ServerSocket.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using Hermes::ConnectionErrorEnum;
using Hermes::DefaultSocketData;
using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::RawTcpClient;
using Hermes::RawTcpListener;
using Hermes::RawTcpServer;

namespace rg = std::ranges;

static IpEndpoint MakeLoopbackEndpoint(const std::uint16_t port) {
    const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    return IpEndpoint{ loopback, port };
}

static std::vector<std::byte> ToBytes(const std::string_view sv) {
    std::vector<std::byte> buf(sv.size());
    rg::transform(sv, buf.begin(), [](char c) { return static_cast<std::byte>(c); });
    return buf;
}

static std::string ToString(const std::vector<std::byte>& buf) {
    std::string s(buf.size(), '\0');
    rg::transform(buf, s.begin(), [](std::byte b) { return static_cast<char>(b); });
    return s;
}

struct AcceptedPair {
    RawTcpClient client;
    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };

    [[nodiscard]] static AcceptedPair Make(std::uint16_t port) {
        const IpEndpoint endpoint{ MakeLoopbackEndpoint(port) };

        auto listenerResult{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
        EXPECT_TRUE(listenerResult.has_value())
            << "Listen failed: " << std::format("{}", listenerResult.error());

        AcceptedPair pair{ .client{ MakeUninitialized() } };

        std::jthread acceptThread{ [&]() {
            pair.serverResult = listenerResult->AcceptOne();
        }};

        auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
        EXPECT_TRUE(clientResult.has_value())
            << "Connect failed: " << std::format("{}", clientResult.error());

        acceptThread.join();

        pair.client = std::move(*clientResult);
        return pair;
    }

private:
    static RawTcpClient MakeUninitialized() {
        auto r{ RawTcpClient::Connect(DefaultSocketData<>{ MakeLoopbackEndpoint(0) }) };
        return std::move(*r);
    }
};

struct ListenerSocketTest : testing::Test {};

TEST_F(ListenerSocketTest, Listen_ValidLoopbackEndpoint_Succeeds) {
    const auto result{ RawTcpListener::Listen(DefaultSocketData<>{ MakeLoopbackEndpoint(19100) }) };
    EXPECT_TRUE(result.has_value())
        << std::format("{}", result.error());
}

TEST_F(ListenerSocketTest, Listen_PortAlreadyBound_ReturnsAddressInUse) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(19101) };
    auto first{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(first.has_value());

    const auto second{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), ConnectionErrorEnum::AddressInUse);
}

TEST_F(ListenerSocketTest, ListenOne_ValidEndpoint_Succeeds) {
    const auto result{ RawTcpListener::ListenOne(DefaultSocketData<>{ MakeLoopbackEndpoint(19102) }) };
    EXPECT_TRUE(result.has_value())
        << std::format("{}", result.error());
}

TEST_F(ListenerSocketTest, Close_DoesNotCrash) {
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ MakeLoopbackEndpoint(19103) }) };
    ASSERT_TRUE(listener.has_value());
    EXPECT_NO_FATAL_FAILURE(listener->Close());
}

TEST_F(ListenerSocketTest, AcceptOne_ConnectedClient_ReturnsServerSocket) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(19104) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() {
        serverResult = listener->AcceptOne();
    }};

    const auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    EXPECT_TRUE(serverResult.has_value());
}

TEST_F(ListenerSocketTest, AcceptAll_YieldsServerSocketPerClient) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(19105) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    constexpr int clientCount{ 3 };
    int accepted{};

    std::jthread acceptThread{ [&]() {
        for (auto&& result : listener->AcceptAll()) {
            if (!result) break;
            if (++accepted >= clientCount)
                listener->Close();
        }
    }};

    for (int i{}; i < clientCount; ++i)
        auto _ { RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };

    acceptThread.join();
    EXPECT_EQ(accepted, clientCount);
}

struct ClientSocketTest : testing::Test {};

TEST_F(ClientSocketTest, Connect_ValidLoopbackListener_Succeeds) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(19200) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    std::jthread{ [&]() { auto _ { listener->AcceptOne() }; } }.detach();

    const auto result{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    EXPECT_TRUE(result.has_value())
        << std::format("{}", result.error());
}

TEST_F(ClientSocketTest, Connect_NoListener_ReturnsConnectionFailed) {
    const auto result{ RawTcpClient::Connect(DefaultSocketData<>{ MakeLoopbackEndpoint(19201) }) };
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ConnectionErrorEnum::ConnectionFailed);
}

TEST_F(ClientSocketTest, Close_DoesNotCrash) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(19202) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());
    std::jthread{ [&]() { auto _ { listener->AcceptOne() }; } }.detach();

    auto client{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(client.has_value());
    EXPECT_NO_FATAL_FAILURE(client->Close());
}

TEST_F(ClientSocketTest, MoveConstruct_PreservesUsability) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(19203) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto original{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(original.has_value());

    RawTcpClient moved{ std::move(*original) };
    acceptThread.join();
    ASSERT_TRUE(serverResult.has_value());

    std::vector<std::byte> payload{ ToBytes("ping") };
    const auto [sent, sendErr]{ moved.Send(payload) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, payload.size());
}

struct SocketBridgeTest : testing::Test {
    static constexpr std::uint16_t kBasePort{ 19300 };
};

TEST_F(SocketBridgeTest, ClientSend_ServerRecv_SmallMessage) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 0) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    auto& client{ *clientResult };
    auto& server{ *serverResult };

    auto outBuf{ ToBytes("hello") };
    const auto [sent, sendErr]{ client.Send(outBuf) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, outBuf.size());

    std::vector<std::byte> inBuf(outBuf.size());
    const auto [recvd, recvErr]{ server.Recv(inBuf) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(recvd, inBuf.size());
    EXPECT_EQ(ToString(inBuf), "hello");
}

TEST_F(SocketBridgeTest, ServerSend_ClientRecv_SmallMessage) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 1) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    auto outBuf{ ToBytes("world") };
    const auto [sent, sendErr]{ serverResult->Send(outBuf) };
    EXPECT_TRUE(sendErr.has_value());
    EXPECT_EQ(sent, outBuf.size());

    std::vector<std::byte> inBuf(outBuf.size());
    const auto [recvd, recvErr]{ clientResult->Recv(inBuf) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(recvd, inBuf.size());
    EXPECT_EQ(ToString(inBuf), "world");
}

TEST_F(SocketBridgeTest, Bidirectional_PingPong) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 2) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    auto& client{ *clientResult };
    auto& server{ *serverResult };

    auto ping{ ToBytes("ping") };
    EXPECT_TRUE(client.Send(ping).second.has_value());

    std::vector<std::byte> recvPing(ping.size());
    EXPECT_TRUE(server.Recv(recvPing).second.has_value());
    EXPECT_EQ(ToString(recvPing), "ping");

    auto pong{ ToBytes("pong") };
    EXPECT_TRUE(server.Send(pong).second.has_value());

    std::vector<std::byte> recvPong(pong.size());
    EXPECT_TRUE(client.Recv(recvPong).second.has_value());
    EXPECT_EQ(ToString(recvPong), "pong");
}

TEST_F(SocketBridgeTest, Send_LargePayload_AllBytesReceived) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 3) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    constexpr std::size_t payloadSize{ 64 * 1024 };
    std::vector<std::byte> outBuf(payloadSize);
    for (std::size_t i{}; i < payloadSize; ++i)
        outBuf[i] = static_cast<std::byte>(i & 0xff);

    EXPECT_TRUE(clientResult->Send(outBuf).second.has_value());

    std::vector<std::byte> inBuf(payloadSize);
    std::size_t totalRecvd{};
    while (totalRecvd < payloadSize) {
        std::span slice{ std::span{ inBuf }.subspan(totalRecvd) };
        const auto [n, err]{ serverResult->Recv(slice) };
        ASSERT_TRUE(err.has_value());
        ASSERT_GT(n, 0u);
        totalRecvd += n;
    }

    EXPECT_EQ(inBuf, outBuf);
}

TEST_F(SocketBridgeTest, RecvLazyRange_ClientSend_ServerIterates) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 4) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    std::string_view message{ "stream test" };
    auto outBuf{ ToBytes(message) };
    EXPECT_TRUE(clientResult->Send(outBuf).second.has_value());
    clientResult->Close();

    std::string received;
    received.append_range(serverResult->RecvRange<char>());

    EXPECT_EQ(received, message);
}

TEST_F(SocketBridgeTest, MultipleMessages_InSequence_AllReceived) {
    using std::literals::operator""sv;

    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 5) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    const std::array messages{ "first"sv, "second"sv, "third"sv };
    for (auto msg : messages) {
        auto buf{ ToBytes(msg) };
        EXPECT_TRUE(clientResult->Send(buf).second.has_value());

        std::vector<std::byte> inBuf(msg.size());
        EXPECT_TRUE(serverResult->Recv(inBuf).second.has_value());
        EXPECT_EQ(ToString(inBuf), msg);
    }
}

TEST_F(SocketBridgeTest, ServerClose_ClientRecv_ReturnsConnectionClosed) {
    using std::string_view_literals::operator""sv;

    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 6) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    Hermes::ConnectionResult<RawTcpServer> serverResult{ std::unexpected{ ConnectionErrorEnum::Unknown } };
    std::jthread acceptThread{ [&]() { serverResult = listener->AcceptOne(); }};

    auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
    acceptThread.join();

    ASSERT_TRUE(clientResult.has_value());
    ASSERT_TRUE(serverResult.has_value());

    serverResult->Close();

    std::vector<std::byte> inBuf(16);
    const auto [recvd, recvErr]{ clientResult->Recv(inBuf) };
    ASSERT_FALSE(recvErr.has_value());
    EXPECT_EQ(recvErr.error(), ConnectionErrorEnum::ConnectionClosed);
}

TEST_F(SocketBridgeTest, MultipleClients_EachReceivesOwnData) {
    const IpEndpoint endpoint{ MakeLoopbackEndpoint(kBasePort + 7) };
    auto listener{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
    ASSERT_TRUE(listener.has_value());

    constexpr int clientCount{ 3 };
    std::vector<Hermes::ConnectionResult<RawTcpServer>> servers;
    servers.reserve(clientCount);

    std::jthread acceptThread{ [&]() {
        for (int i{}; i < clientCount; ++i)
            servers.push_back(listener->AcceptOne());
    }};

    std::vector<RawTcpClient> clients;
    clients.reserve(clientCount);
    for (int i{}; i < clientCount; ++i) {
        auto r{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
        ASSERT_TRUE(r.has_value());
        clients.push_back(std::move(*r));
    }

    acceptThread.join();
    ASSERT_EQ(static_cast<int>(servers.size()), clientCount);

    for (int i{}; i < clientCount; ++i) {
        ASSERT_TRUE(servers[i].has_value());
        const std::string tag{ std::format("client{}", i) };

        auto out{ ToBytes(tag) };
        EXPECT_TRUE(clients[i].Send(out).second.has_value());

        std::vector<std::byte> in(tag.size());
        EXPECT_TRUE(servers[i]->Recv(in).second.has_value());
        EXPECT_EQ(ToString(in), tag);
    }
}