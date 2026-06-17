#include <gtest/gtest.h>
#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Socket/Sync/ListenerSocket.hpp>
#include <Hermes/Socket/Sync/ServerSocket.hpp>
#include <thread>
#include <vector>

using Hermes::RawTcpClient;
using Hermes::RawTcpListener;
using Hermes::RawTcpServer;
using Hermes::DefaultSocketData;
using Hermes::IpEndpoint;
using Hermes::IpAddress;

static IpEndpoint I_MakeLoopbackEndpoint(const std::uint16_t port) {
    const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    return IpEndpoint{ loopback, port };
}

static std::uint16_t I_GetNextPort() {
    static std::uint16_t port{ 20000 };
    return port++;
}

struct SocketBridgeFixture : testing::Test {
    std::optional<RawTcpListener> listener{};
    std::optional<RawTcpClient> client{};
    std::optional<RawTcpServer> server{};

    void SetUp() override {
        const IpEndpoint endpoint{ I_MakeLoopbackEndpoint(I_GetNextPort()) };
        auto listenerResult{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
        ASSERT_TRUE(listenerResult.has_value());
        listener = std::move(*listenerResult);

        auto s_acceptLambda{ [&]() {
            auto res{ listener->AcceptOne() };
            if (res.has_value()) {
                server = std::move(*res);
            }
        }};

        std::jthread acceptThread{ s_acceptLambda };

        auto clientResult{ RawTcpClient::Connect(DefaultSocketData<>{ endpoint }) };
        ASSERT_TRUE(clientResult.has_value());
        client = std::move(*clientResult);

        acceptThread.join();
        ASSERT_TRUE(server.has_value());
    }

    void TearDown() override {
        if (client) client->Close();
        if (server) server->Close();
        if (listener) listener->Close();
    }
};

TEST_F(SocketBridgeFixture, PingPong_BidirectionalTransferSucceeds) {
    const std::vector<std::byte> pingPayload{ std::byte{'p'}, std::byte{'i'}, std::byte{'n'}, std::byte{'g'} };

    const auto [sent, sendErr]{ client->Send(pingPayload) };
    EXPECT_TRUE(sendErr.has_value());

    std::vector<std::byte> recvBuffer;
    recvBuffer.resize(4);

    const auto [recvd, recvErr]{ server->Recv(recvBuffer) };
    EXPECT_TRUE(recvErr.has_value());
    EXPECT_EQ(recvBuffer, pingPayload);
}

TEST_F(SocketBridgeFixture, ServerClose_ClientRecv_ReturnsConnectionClosed) {
    server->Close();

    std::vector<std::byte> inBuf{};
    inBuf.resize(16);

    const auto [recvd, recvErr]{ client->Recv(inBuf) };
    ASSERT_FALSE(recvErr.has_value());

    EXPECT_EQ(recvErr.error(), Hermes::ConnectionErrorEnum::ConnectionClosed);
}