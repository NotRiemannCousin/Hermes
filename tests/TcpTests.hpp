#pragma once
#include <Hermes/Socket/Tcp/TcpClientSocket.hpp>
#include "_base.hpp"

inline void TcpTests() {
    TestBattery("TCP Tests");

    struct {
        string scheme;
        string hostname;
        string path;
    } url{
        "http",
        "api.discogs.com",
        "artists/4001234",
    };

    auto endpoint{ IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!endpoint)
        return Test("Endpoint Resolved", false);

    Test("Endpoint Resolved", true);

    auto socket{ Hermes::TcpClientSocket::Connect(*endpoint) };
    const auto request{
        format(
            "GET /{} HTTP/1.1\r\n"
            "Host: {}\r\n\r\n",
            url.path, url.hostname) };


    if (!socket)
        return Test("Message Sent", false);

    Test("Message Sent", true);

    const auto err{ socket->SendStr(request) };

    if (!err)
        return Test("Message Sent", false);

    Test("Message Sent", true);

    for (auto message : socket->ReceiveStr()) {
        if (!message || !message->starts_with("HTTP/1.1"))
            break;
        return Test("Message Received", true);
    }

    Test("Message Received", false);
}