#include <gtest/gtest.h>
#include <Hermes/Socket/Async/AsyncClientSocket.hpp>
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Socket/Async/AsyncServerSocket.hpp>
#include <stdexec/execution.hpp>

#include <vector>
#include <string_view>
#include <thread>
#include <optional>
#include <atomic>

using Hermes::RawTcpAsyncClient;
using Hermes::RawTcpAsyncListener;
using Hermes::RawTcpAsyncServer;
using Hermes::DefaultSocketData;
using Hermes::IpAddress;
using Hermes::IpEndpoint;

#pragma region Utils

static std::uint16_t I_GetNextAsyncPort() {
    static std::atomic<std::uint16_t> s_port{ 25000 };
    return s_port++;
}

static IpEndpoint I_MakeLoopbackEndpoint(const std::uint16_t port) {
    const IpAddress loopback{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    return IpEndpoint{ loopback, port };
}

#pragma endregion

#pragma region Fixture

struct AsyncSocketBridgeFixture : testing::Test {
    std::optional<RawTcpAsyncListener> listener{};
    std::optional<RawTcpAsyncClient> client{};
    std::optional<RawTcpAsyncServer> server{};

    void SetUp() override {
        const IpEndpoint endpoint{ I_MakeLoopbackEndpoint(I_GetNextAsyncPort()) };
        
        // Sincroniza a criação do Listener que é assíncrona na nova arquitetura
        auto listenResult{ stdexec::sync_wait(RawTcpAsyncListener::Listen(DefaultSocketData<>{ endpoint })) };
        ASSERT_TRUE(listenResult.has_value());
        
        auto [boundListener] = listenResult.value();
        listener.emplace(std::move(boundListener));

        auto s_acceptLambda{ [&]() {
            auto acceptRes{ stdexec::sync_wait(listener->AsyncAcceptOne()) };
            if (acceptRes.has_value()) {
                auto [acceptedServer] = acceptRes.value();
                server.emplace(std::move(acceptedServer));
            }
        }};
        std::jthread acceptThread{ s_acceptLambda };

        auto connectRes{ stdexec::sync_wait(RawTcpAsyncClient::Connect(DefaultSocketData<>{ endpoint })) };
        ASSERT_TRUE(connectRes.has_value());
        
        auto [connectedClient] = connectRes.value();
        client.emplace(std::move(connectedClient));

        acceptThread.join();
        ASSERT_TRUE(server.has_value());
    }

    void TearDown() override {
        if (client) client->Close();
        if (server) server->Close();
        if (listener) listener->Close();
    }
};

#pragma endregion

#pragma region Tests

TEST_F(AsyncSocketBridgeFixture, PingPong_SendAndRecvThroughSenders) {
    const std::vector<std::byte> pingPayload{ std::byte{'p'}, std::byte{'i'}, std::byte{'n'}, std::byte{'g'} };
    
    auto sendResult{ stdexec::sync_wait(client->Send(pingPayload)) };
    ASSERT_TRUE(sendResult.has_value());

    auto [sent] = sendResult.value();
    EXPECT_EQ(sent, pingPayload.size());

    std::vector<std::byte> recvBuffer{};
    recvBuffer.resize(4);

    auto recvResult{ stdexec::sync_wait(server->Recv(recvBuffer)) };
    ASSERT_TRUE(recvResult.has_value());

    auto [recvd] = recvResult.value();
    EXPECT_EQ(recvBuffer, pingPayload);
}

TEST_F(AsyncSocketBridgeFixture, Shutdown_GracefullyEndsConnection) {
    auto shutdownResult{ stdexec::sync_wait(client->Shutdown()) };
    EXPECT_TRUE(shutdownResult.has_value());

    std::vector<std::byte> recvBuffer{};
    recvBuffer.resize(10);

    EXPECT_ANY_THROW({
        auto _ { stdexec::sync_wait(server->Recv(recvBuffer)) };
    });
}