#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/Socket/ClientSocket.hpp>

#include <gtest/gtest.h>

//  These tests require network access. They are disabled by default so the suite
//  passes in offline/CI environments. Run with --gtest_also_run_disabled_tests
//  to include them.

#pragma region disabled

TEST(DISABLED_TcpClientTest, Connect_SendReceive_Http) {
    const auto endpoint{ Hermes::IpEndpoint::TryResolve("example.com", "http") };
    ASSERT_TRUE(endpoint.has_value()) << "Could not resolve example.com:80";

    auto socket{ Hermes::RawTcpClient::Connect(Hermes::DefaultSocketData( *endpoint )) };
    ASSERT_TRUE(socket.has_value()) << "Could not connect";

    const std::string request{
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n\r\n"
    };

    const auto [sent, sendErr]{ socket->Send(request) };
    ASSERT_TRUE(sendErr.has_value()) << "Send failed";
    EXPECT_EQ(sent, request.size());

    auto recvRange{ socket->RecvRange<char>() };
    const std::string response{ recvRange | std::ranges::to<std::string>() };

    EXPECT_TRUE(response.starts_with("HTTP/1.1"))
        << "Unexpected response: " << response.substr(0, 64);
}

TEST(DISABLED_TlsClientTest, Connect_SendReceive_Https) {
    const auto endpoint{ Hermes::IpEndpoint::TryResolve("example.com", "https") };
    ASSERT_TRUE(endpoint.has_value()) << "Could not resolve example.com:443";

    auto socket{ Hermes::RawTlsClient::Connect(
        Hermes::TlsSocketData<>{ *endpoint, "example.com" }) };
    ASSERT_TRUE(socket.has_value()) << "TLS connect/handshake failed";

    const std::string request{
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n\r\n"
    };

    const auto [sent, sendErr]{ socket->Send(request) };
    ASSERT_TRUE(sendErr.has_value()) << "Send failed";

    auto recvRange{ socket->RecvRange<char>() };
    const std::string response{ recvRange | std::ranges::to<std::string>() };

    EXPECT_TRUE(response.starts_with("HTTP/1.1"))
        << "Unexpected response: " << response.substr(0, 64);
}

#pragma endregion