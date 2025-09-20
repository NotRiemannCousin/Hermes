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
- QUIC (not implemented yet)



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
   
for (auto block : socket->ReceiveStr()) {
    // blocks are `generator<expected<string, ConnectionErrorEnum>>`, process it as
    // you want. You can use `socket->Receive()` too, to deal with '/0'. It's type is
    // `generator<expected<vector<byte>, ConnectionErrorEnum>>`
    
    // Sockets don't know when all the data has been sent, this is a role of the application layer.
    // If you are planning to use it for HTTP check manually for Content-Length/"\r\n\r\n" or use Thoth.
}
```