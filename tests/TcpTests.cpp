// #pragma once
// #include <Hermes/Socket/Tcp/TcpClientSocket.hpp>
// #include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
// #include <BaseTests.hpp>
//
// namespace rg = std::ranges;
// namespace vs = std::views;
// vs::reverse;
//
// void TcpTests() {
//     TestBattery("TCP Tests");
//
//     struct {
//         string scheme;
//         string hostname;
//         string path;
//     } url{
//         "http",
//         "api.discogs.com",
//         "artists/4001234",
//     };
//
//     auto endpoint{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
//     if (!endpoint)
//         return Test("Endpoint Resolved", false);
//
//     Test("Endpoint Resolved", true);
//
//     auto socket{ Hermes::TcpClientSocket::Connect(*endpoint) };
//     const auto request{
//         format(
//             "GET /{} HTTP/1.1\r\n"
//             "Host: {}\r\n\r\n",
//             url.path, url.hostname) };
//
//
//     if (!socket)
//         return Test("Message Sent", false);
//
//     Test("Message Sent", true);
//
//     const auto err{ socket->SendStr(request) };
//
//     if (!err)
//         return Test("Message Sent", false);
//
//     Test("Message Sent", true);
//
//     auto message{ socket->ReceiveStr() };
//
//     if (rg::starts_with(message, string_view{ "HTTP/1.1" }))
//         return Test("Message Received", true);
//
//     Test("Message Received", false);
// }