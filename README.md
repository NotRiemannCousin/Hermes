# Hermes

A C++ socket wrapper library providing a simple, type-safe, and secure interface for transport-layer networking. Hermes leverages modern C++ features — `std::expected`, `std::ranges`, `std::execution` (planned), and more — targeting **Windows 10 or newer** with **C++26**.

> **Status:** v0.2 — active development. Async sockets (`std::execution`) are planned once compiler support matures.

---

## Features

- **TCP** and **TCP over TLS** (via SChannel — no third-party crypto dependency)
- **Policy-based design** — connection, transfer, and accept behaviors are independently composable via concepts
- **Lazy receive ranges** — `RecvRange` is an [`input_range`](https://en.cppreference.com/w/cpp/ranges/input_range.html) that fetches bytes on demand, eliminating manual chunk management
- **`std::expected` throughout** — no exceptions on the hot path
- Extensible: implement `SocketDataConcept`, `ConnectionPolicyConcept` / `AcceptPolicyConcept`, and `TransferPolicyConcept` to add your own transport layer protocols

Hermes operates at the transport layer only. Application-layer protocols (HTTP, WebSocket, etc.) are left to the user.

---

## Installation

### pixi

> _Coming soon._

### vcpkg

> _Coming soon._

### Manual (CMake)

```cmake
include(FetchContent)

FetchContent_Declare(
    Hermes
    GIT_REPOSITORY https://github.com/your-org/Hermes.git
    GIT_TAG        v0.2.03
)

FetchContent_MakeAvailable(Hermes)

target_link_libraries(your_target PRIVATE Hermes)
```

---

## Example

The following example performs an HTTPS GET request, parsing the response incrementally using `RecvRange`. Notice how `UntilMatch` and range composition keep the parsing logic concise — no manual buffering.

```cpp
#include <Hermes/Socket/ClientSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

namespace rg = std::ranges;
using namespace std::literals;

std::expected<std::monostate, std::string> MakeRequest() {
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

    auto socket{ Hermes::RawTlsClient::Connect(Hermes::TlsSocketData<>{ *endpoint, url.hostname }) };
    if (!socket)
        return std::unexpected{ "Could not connect to endpoint" };

    const auto request{
        std::format(
            "GET /{} HTTP/1.1\r\n"
            "Accept-Encoding: identity\r\n"
            "User-Agent: Hermes/0.2\r\n"
            "Host: {}\r\n\r\n",
            url.path, url.hostname)
    };

    if (const auto err{ socket->Send(request) }; !err.second)
        return std::unexpected{ "Could not send request" };

    // RecvRange is an input_range: bytes are fetched on demand when the iterator
    // is dereferenced. Advancing past a byte discards it — keep this in mind when
    // doing partial reads. All iterators share the same underlying state
    // (think: a "global input range"), so advancing one advances all.
    auto socketView{ socket->RecvRange<char>() };

    if (!rg::starts_with(socketView, "HTTP/1.1"sv))
        return std::unexpected{ "Unsupported HTTP version" };

    const auto statusCode{ Hermes::Utils::CopyTo<std::array<char, 5>>(socketView) };

    if (!rg::equal(statusCode, " 200 "sv))
        return std::unexpected{
            std::format("Unexpected status code: {}{}{}", statusCode[1], statusCode[2], statusCode[3])
        };

    // UntilMatch reads until the first occurrence of the pattern and discards it (exclusive).
    // The range is then positioned at the next byte after the pattern.
    // Use UntilMatch<true> for inclusive matching.
    const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };

    const auto headers{ HttpHeaders(socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv)) };
    if (!headers.has_value())
        return std::unexpected{ headers.error() };

    auto chunkLength{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };
    const auto body   { socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };

    std::println("body:\n\n{}", body);

    if (const auto err{ socketView.Error() }; !err)
        return std::unexpected{ "Error while receiving message" };

    return {};
}
```

---

## Roadmap

- Async sockets via `std::execution` (pending compiler support)
- UDP sockets
- pixi and vcpkg packages

---

## Requirements

- Windows 10 or newer
- MSVC with C++26 support
- CMake 3.29.1 or newer