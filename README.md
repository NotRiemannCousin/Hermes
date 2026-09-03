# Hermes

![Hermes](Hermes-logo.webp "Hermes, the Greek god of messengers and trade")

A C++ socket wrapper library providing a simple, type-safe, and secure interface for transport-layer networking. Hermes
leverages modern C++ features — `std::expected`, `std::ranges`, `std::execution`, and more — targeting **C++26**.

> **Status:** v0.5 — active development. Async sockets via `std::execution` are available. Linux is available.

---

## Features

- **TCP** and **TCP over TLS** via SChannel on Windows and OpenSSL on Linux (with the `HERMES_ENABLE_TLS` option,
default `ON`)
- **Policy-based design** — connection, transfer, and accept behaviors are independently composable via concepts
- **Lazy receive ranges** — `RecvStream` is an [`input_range`](https://en.cppreference.com/w/cpp/ranges/input_range.html) that fetches bytes on demand, eliminating manual
chunk management
- **`std::expected` throughout** — no exceptions on the hot path
- **Async support** — async sockets via `stdexec` (senders/receivers) and C++20 coroutines (with `HERMES_ENABLE_ASYNC`
option, default `OFF`)
- Extensible: implement `SocketDataConcept`, `ConnectionPolicyConcept` / `AcceptPolicyConcept`, and
`TransferPolicyConcept` to add your own transport layer protocols

Hermes operates at the transport layer only. Application-layer protocols (HTTP, WebSocket, etc.) are left to the user.

> Note: The library relies heavily on C++26 features. The CMake configuration temporarily targets C++23 due to current
MSVC flag compatibility, but a C++26-capable compiler is strictly required.

---

## Installation

### pixi

> _Coming soon._

### vcpkg

> _Maybe, someday, who knows._

### Manual (CMake)

```cmake
include(CPM.cmake)

CPMAddPackage(
        NAME Hermes
        GITHUB_REPOSITORY NotRiemannCousin/Hermes
        GIT_TAG v0.6.0
)

target_link_libraries(your_target PRIVATE Hermes)
```

---

## Build Options

There are 3 library options:

| Option                           | Default | Effect                                                                                                                                                         |
|----------------------------------|---------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `HERMES_ENABLE_TLS`              | `ON`    | Builds TCP over TLS (OpenSSL on Linux, SChannel on Windows) with its own aliases and policies.                                                                 |
| `HERMES_ENABLE_ASYNC`            | `OFF`   | Builds the family of async sockets (needs `stdexec`).                                                                                                          |
| `HERMES_ENABLE_NATIVE_SCHEDULER` | `OFF`   | Builds `FastIoLoop`'s native backend (used to schedule jobs on async sockets, needs io_uring on Linux and IOCP on Windows). Requires `HERMES_ENABLE_ASYNC=ON`. |

If you only need synchronous sockets, `-DHERMES_ENABLE_TLS=ON -DHERMES_ENABLE_ASYNC=OFF` (the default) avoids pulling
in `stdexec` entirely. For the full feature set (async + native scheduler + TLS):

```
-DHERMES_ENABLE_ASYNC=ON -DHERMES_ENABLE_NATIVE_SCHEDULER=ON -DHERMES_ENABLE_TLS=ON
```

---

## Example

The following example performs an HTTPS GET request, parsing the response incrementally using `RecvStream`. Notice how `UntilMatch` and range composition keep the parsing logic concise — no manual buffering.

```cpp
#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

namespace rg = std::ranges;
namespace vs = std::views;

std::expected<std::monostate, std::string> MakeRequest() {
    using namespace std::literals::string_view_literals;

    struct { std::string scheme; std::string hostname; std::string path; } url {
            "https", "api.discogs.com", "artists/4001234", };

#pragma region Lambdas

    const auto makeSocket{ [&](const Hermes::IpEndpoint endpoint) {
        return Hermes::RawTlsClient::Connect(Hermes::TlsSocketData<>{ endpoint, url.hostname });
    } };

    const auto makeRequest{ [&](Hermes::RawTlsClient&& client) {
        const auto request{
            format(
                "GET /{} HTTP/1.1\r\n"
                "Accept-Encoding: identity\r\n"
                "User-Agent: Hermes/0.5\r\n"
                "Host: {}\r\n\r\n",
                url.path, url.hostname) };

        return client.Send(request).second
                .transform([client = std::move(client)](const auto) mutable { return std::move(client); });
    } };

    constexpr auto mapError{ [](const Hermes::ConnectionErrorEnum error) -> std::string {
        using Error = Hermes::ConnectionErrorEnum;
        switch (error) {
            case Error::ResolveHostNotFound:
            case Error::ResolveServiceNotFound:
            case Error::ResolveTemporaryFailure:
            case Error::ResolveFailed:
            case Error::ResolveNoAddressFound:
            case Error::UnsupportedAddressFamily:
                return "Could not resolve endpoint";

            case Error::HandshakeFailed:
            case Error::ConnectionFailed:
            case Error::CertificateError:
            case Error::Unknown:
                return "Could not connect to endpoint";

            case Error::ConnectionClosed:
            case Error::ReceiveFailed:
            case Error::DecryptionFailed:
            case Error::SendFailed:
                return "Could not send to endpoint";
            default:
                return "unknown error";
        }
    } };

    const auto getResponse{ [&](Hermes::RawTlsClient&& client) -> std::expected<std::string, std::string> {
        auto socketView{ client.RecvLazyRange<char>() };
        // `RecvLazyRange` is an input_range, so it consumes the bytes when you advance the iterator. Advancing an iterator of an
        // input_range can cause invalidation of other iterators, but the current state is stored in the range so all
        // iterators are treated equally and represent the current state.
        // This code shows how it can be useful.

        // (Do I need to give a name to this type of range? sibling{_input}_range? global_{input}_range? Idk yet).

        if (!rg::startwith(socketView, "HTTP/1.1"sv))
            return std::unexpected{ "Non supported version" };


        const auto statusCode{ Hermes::Utils::CopyTo<std::array<char, 5>>(socketView) };

        if (!rg::equal(statusCode, " 200 "sv))
            return std::unexpected{ std::format("error code: {}{}{}", statusCode[1], statusCode[2], statusCode[3]) };


        const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };
        // This range must receive more bytes just when reading with the * operator. receiving when advancing isn't that
        // good because if your protocol uses a terminated sequence you will need more work to stop at the last byte
        // (think about this like vec.end() being outside of the boundaries of the vector itself).

        // e.g.: `UntilMatch("\r\n"sv)` goes until the first occurrence of "\r\n", discard the pattern (exclusive match,
        // `UntilMatch<true>` is inclusive) and advance the state again to stop at the next byte of EOS.

        const auto headers { socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv) | rg::to<std::string>() };
        const auto chunkLen{ socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };
        const auto body    { socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };

        // Ok, this is unsafe because I'm being lazy here, but you can process and check this data properly.
        // You can use ` | std::views::take(maxChunkStringLength)` before UntilMatch to easily limit the size.
        // The range automatically stops when the connection ends, but be careful with this.

        if (const auto err{ socketView.Error() }; !err)
            return std::unexpected{ "Error receiving message" };

        return body;
    } };

    const auto processData{ [](std::string&& body) {
        std::println("body:\n\n{}", body);

        return std::monostate{};
    } };

#pragma endregion

    return Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme)
            .and_then(makeSocket)
            .and_then(makeRequest)
            .transform_error(mapError)
            .and_then(getResponse)
            .transform(processData);
}
```

---

## Roadmap

- UDP sockets
- pixi and vcpkg packages

---

## Requirements

- Windows 10 or newer / Linux
- On Linux, OpenSSL for `HERMES_ENABLE_TLS` and liburing for `HERMES_ENABLE_NATIVE_SCHEDULER` (download like `sudo apt
install libssl-dev liburing-dev`)
- MSVC or GCC with C++26 support (GCC 16 on Linux)
- CMake 3.29.1 or newer
