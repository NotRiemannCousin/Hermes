# Hermes

## About

Hermes is a C++ Socket Wrapper library. It provides a simple, easy-to-use, and secure interface for
creating and using sockets. The goal of this lib is to use modern C++ features such as `std::expected`,
`std::ranges`, `std::generator`, modules and templates. Given that, the target version is C++26.

This lib implements only transport layer protocols, you have to implement application layer protocols by yourself.
If can follow `SocketDataConcept`, `ConnectionPolicyConcept` and `TransferPolicyConcept` implement you own transport
layer protocols.

## Features

The main way to read bytes from a socket is by using the `RecvRange` provided by `TransferPolicyConcept`. It 
automatically receives data from the socket on demand, so you don't need to worry about managing chunks of data
(keep in mind that this is an <a href="https://en.cppreference.com/w/cpp/ranges/input_range.html">
input_range</a> so every time you advance the iterator the last byte is lost).

The example on this page shows how this behaviour makes development easier.

Async types will be developed soon.

Implemented protocols:

- TCP
- TCP with TLS
- UDP (not implemented yet)
- UDP with DTLS (not implemented yet)


Example:
```cpp
expected<std::monostate, string> MakeRequest() {
    struct {
        std::string scheme;
        std::string hostname;
        std::string path;
    } url {
        "https",
        "api.discogs.com",
        "artists/4001234",
    };

    auto endpoint{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!endpoint)
        return std::unexpected{ "Could not resolve endpoint" };

    auto socket{ Hermes::RawTlsClient::Connect(*endpoint, Hermes::TlsSocketData{ url.hostname }) };
    const auto request{
        format(
            "GET /{} HTTP/1.1\r\n"
            "Accept-Encoding: identity\r\n"
            "User-Agent: Hermes/0.1\r\n"
            "Host: {}\r\n\r\n",
            url.path, url.hostname) };


    if (!socket)
        return std::unexpected{ "Could not connect to endpoint" };

    if (const auto err{ socket->Send(request) }; !err)
        return std::unexpected{ "Could not send to endpoint" };




    auto socketView{ socket->RecvRange<char>() };
    // `RecvRange` is an input_range, so it consumes the bytes when you advance the iterator. Advancing an iterator of an
    // input_range can cause invalidation of other iterators, but the current state is stored in the range so all
    // iterators are treated equally and represent the current state (Do I need to give a name to this type of range?
    // sibling{_input}_range? global_{input}_range? Idk). This code shows how it can be useful.

    if (!rg::starts_with(socketView, "HTTP/1.1"sv))
        return std::unexpected{ "Non supported version" };


    const auto statusCode{ Hermes::Utils::CopyTo<std::array<char, 5>>(socketView) };

    if (!rg::equal(statusCode, " 200 "sv))
        return std::unexpected{ std::format("error code: {}{}{}", statusCode[1], statusCode[2], statusCode[3]) };


    const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>() };
    // This range must receive more bytes just when reading with the * operator. receiving when advancing isn't that
    // good because if your protocol uses a terminated sequence you will need more work to stop at the last byte
    // (think about this like vec.end() being outside of the boundaries of the vector itself).

    // e.g.: `UntilMatch("\r\n"sv)` goes until the first occurrence of "\r\n", discard the pattern (exclusive match,
    // `UntilMatch<true>` is inclusive) and advance the state again to stop at the next byte of EOS.


    const auto headers{ HttpHeaders(socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv)) };

    if (!headers.has_value())
        return std::unexpected{ headers.error() };

    auto sla = socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>();

    const auto body{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>() };

    std::println("body:\n\n{}", body);

    if (const auto err{ socketView.Error() }; !err)
        return std::unexpected{ "Error receiving message" };

    return {};
}

```