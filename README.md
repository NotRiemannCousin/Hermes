# Hermes

## About

Hermes is a C++ Socket Wrapper library. It provides a simple, easy-to-use, and secure interface for
creating and using sockets. The goal of this lib is to use modern C++ features such as `std::expected`,
`std::ranges`, `std::generator`, and modules. Given that, the target version is C++26.

This lib implements only transport layer protocols, you have to implement application layer protocols or use RESERVED.

## Features

Implemented protocols:

- TCP
- UDP (not implemented yet)



```cpp
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
if(!endpoint)
    return endpoint.error();

auto socket{ TcpClientSocket::Connect(*endpoint) };

auto request{
    format(
        "GET /{} HTTP/1.1\r\n"
        "Host: {}\r\n\r\n",
        url.path, url.hostname) };
           
auto _{ socket->SendStr(request) };
   

auto socketView{ socket->ReceiveStr() };

if (!rg::starts_with(socketView, "HTTP/1.1 "sv))
    return std::unexpected{ "Non supported version" };


const auto statusCode{ Hermes::Utils::CopyTo<std::array<char, 3>>(socketView) };
const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>() };

const auto headers{ HttpHeaders(socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv)) };

if (!headers.has_value())
    return std::unexpected{ headers.error() };

if (const auto err{ socketView.OptError() }; !err)
    return std::unexpected{ "Error receiving message" };
    
// ...
```