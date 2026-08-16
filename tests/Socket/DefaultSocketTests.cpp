#include <gtest/gtest.h>
#include <atomic>

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

static IpEndpoint MakeLoopbackEndpoint(const std::uint16_t port) {
    const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    return IpEndpoint{ loopback, port };
}

static std::uint16_t GetNextPort() {
    static std::uint16_t port{ 20000 };
    return port++;
}

struct SocketBridgeFixture : testing::Test {
    std::optional<RawTcpListener> listener{};
    std::optional<RawTcpClient> client{};
    std::optional<RawTcpServer> server{};
protected:
    void SetUp() override {
        const IpEndpoint endpoint{ MakeLoopbackEndpoint(GetNextPort()) };
        auto listenerResult{ RawTcpListener::Listen(DefaultSocketData<>{ endpoint }) };
        ASSERT_TRUE(listenerResult.has_value());
        listener = std::move(*listenerResult);

        auto acceptLambda{ [&]() {
            auto res{ listener->AcceptOne({ .recvBufferSize = 1024 }) };
            if (res.has_value()) {
                server = std::move(*res);
            }
        }};

        std::jthread acceptThread{ acceptLambda };

        auto clientResult{ RawTcpClient::Connect( DefaultSocketData<>{ endpoint }, { .sendBufferSize = 1024 }) };
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


TEST_F(SocketBridgeFixture, Recv_DeadlineCoversMultipleBlocks) {
    using namespace std::chrono_literals;

    const std::vector<std::byte> first{ std::byte{'a'} };
    const std::vector<std::byte> second{ std::byte{'b'} };
    const auto deadline{ std::chrono::steady_clock::now() + 150ms };

    std::jthread sender{ [&] {
        ASSERT_TRUE(client->Send(first).second.has_value());
        std::this_thread::sleep_for(300ms);
        client->Send(second);
    }};

    std::array<std::byte, 2> buffer{};
    const auto [received, err]{ server->Recv(buffer, Hermes::RecvModeEnum::All, { .deadline = deadline }) };

    EXPECT_EQ(received, 1);
    ASSERT_FALSE(err.has_value());
    EXPECT_EQ(err.error(), Hermes::ConnectionErrorEnum::ReceiveTimeout);
    EXPECT_EQ(buffer[0], first[0]);
}

TEST_F(SocketBridgeFixture, RecvStream_DeadlineCoversMultipleBlocks) {
    using namespace std::chrono_literals;

    const std::vector first{ std::byte{ 'a' } };
    const std::vector second{ std::byte{ 'b' } };
    const auto deadline{ std::chrono::steady_clock::now() + 150ms };

    std::jthread sender{ [&] {
        ASSERT_TRUE(client->Send(first).second.has_value());
        std::this_thread::sleep_for(300ms);
        client->Send(second);
    }};

    auto stream{ server->RecvStream<char>({ .deadline = deadline }) };
    auto iterator{ stream.begin() };

    ASSERT_EQ(*iterator, 'a');
    ++iterator;
    static_cast<void>(*iterator);

    ASSERT_FALSE(stream.Error().has_value());
    EXPECT_EQ(stream.Error().error(), Hermes::ConnectionErrorEnum::ReceiveTimeout);
}

TEST_F(SocketBridgeFixture, Send_ExpiredDeadlineReturnsTimeout) {
    static constexpr std::array payload{ std::byte{'x'} };
    const auto deadline{ std::chrono::steady_clock::now() };

    const auto [sent, err]{ client->Send( payload, { .deadline = deadline } ) };

    EXPECT_EQ(sent, 0);
    ASSERT_FALSE(err.has_value());
    EXPECT_EQ(err.error(), Hermes::ConnectionErrorEnum::SendTimeout);
}
