@page getting-started Getting Started

# Getting Started

Hermes is a modular policy-based library that stands upon socket facades. Curently, it supports only connection-based
client-server comunication, and there are two axes of facades:

<table style="width: 100%; border-collapse: collapse; margin: var(--spacing-medium) 0; font-size: var(--page-font-size);">
  <thead>
    <tr>
      <th rowspan="2" style="background-color: var(--fragment-background); color: var(--primary-color); border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center; font-weight: 700; width: 15%;">
        Role \ Execution
      </th>
      <th colspan="2" style="background-color: var(--tablehead-background); color: var(--tablehead-foreground); border: 1px solid var(--separator-color); padding: var(--spacing-small) var(--spacing-medium); text-align: center; font-weight: 600; border-bottom: 2px solid var(--primary-color);">
        Execution Model
      </th>
      <th rowspan="2" style="background-color: var(--tablehead-background); color: var(--tablehead-foreground); border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: left; font-weight: 600; width: 40%;">
        Description
      </th>
    </tr>
    <tr>
      <th style="background-color: var(--tablehead-background); color: var(--tablehead-foreground); border: 1px solid var(--separator-color); padding: var(--spacing-small) var(--spacing-medium); text-align: center; font-weight: 600; width: 22%;">
        Sync Socket
        <div style="font-size: 11px; font-weight: normal; color: var(--page-secondary-foreground-color); margin-top: 2px;">
          Synchronous communication (normal function flow)
        </div>
      </th>
      <th style="background-color: var(--tablehead-background); color: var(--tablehead-foreground); border: 1px solid var(--separator-color); padding: var(--spacing-small) var(--spacing-medium); text-align: center; font-weight: 600; width: 23%;">
        Async Socket
        <div style="font-size: 11px; font-weight: normal; color: var(--page-secondary-foreground-color); margin-top: 2px;">
          Asynchronous communication (via <code>std::execution</code>/<code>stdexec</code>)
        </div>
      </th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th style="background-color: var(--fragment-background); color: var(--primary-color); border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center; font-weight: 600;">
        Client
      </th>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center;">
        Sync Client
      </td>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center;">
        Async Client
      </td>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); color: var(--page-foreground-color);">
        Socket used in client communication
      </td>
    </tr>
    <tr>
      <th rowspan="2" style="background-color: var(--fragment-background); color: var(--primary-color); border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center; font-weight: 600; vertical-align: middle;">
        Server
      </th>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center;">
        Sync Listener
      </td>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center;">
        Async Listener
      </td>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); color: var(--page-foreground-color);">
        Socket used to accept connections (create Server sockets)
      </td>
    </tr>
    <tr>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center;">
        Sync Server
      </td>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); text-align: center;">
        Async Server
      </td>
      <td style="border: 1px solid var(--separator-color); padding: var(--spacing-medium); color: var(--page-foreground-color);">
        Socket used in server communication
      </td>
    </tr>
  </tbody>
</table>

`ClientSocket`'s are used in the client side. Client requests are usually single pontual operations, so it has a
simplier API and is a "standalone" socket. You create and dispatch it, simple.

But on the server side, you need a `ListenerSocket` listening at a given port (a continuous operation, a loop), this
socket will start to accept new connections and then create new `ServerSocket`, and this `ServerSocket` that will talk
to the client.

The current builtin aliases are:

|                 | Sync Sockets                                                                               | Async Sockets                                                                                                  |
|-----------------|--------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|
| Client Socket   | @ref Hermes::RawTcpClient "RawTcpClient", @ref Hermes::RawTlsClient "RawTlsClient"         | @ref Hermes::RawTcpAsyncClient "RawTcpAsyncClient", @ref Hermes::RawTlsAsyncClient "RawTlsAsyncClient"         |
| Listener Socket | @ref Hermes::RawTcpListener "RawTcpListener", @ref Hermes::RawTlsListener "RawTlsListener" | @ref Hermes::RawTcpAsyncListener "RawTcpAsyncListener", @ref Hermes::RawTlsAsyncListener "RawTlsAsyncListener" |
| Server Socket   | @ref Hermes::RawTcpServer "RawTcpServer", @ref Hermes::RawTlsServer "RawTlsServer"         | @ref Hermes::RawTcpAsyncServer "RawTcpAsyncServer", @ref Hermes::RawTlsAsyncServer "RawTlsAsyncServer"         |

This page will only talk about sync sockets. Overall, their API and sequence diagram are:

| Operation  |  ClientSocket                     |  ServerSocket                     | ListenerSocket                                  | Description                                                                   |
|------------|-----------------------------------|-----------------------------------|-------------------------------------------------|-------------------------------------------------------------------------------|
| Connect    | `Connect`                         | —                                 | —                                               | Creates and connects to a remote endpoint.                                    |
| Listen     | —                                 | —                                 | `Listen`, `ListenOne`                           | Binds and puts the socket into a passive listening state.                     |
| Accept     | —                                 | —                                 | `AcceptOne`, `AcceptOneConnection`, `AcceptAll` | Accepts incoming connections to produce `ServerSocket` instances.             |
| Factory    | —                                 | `FromAccepted`                    | —                                               | Factory method wrapping an already-accepted connection into a `ServerSocket`. |
| Send       | `Send`                            | `Send`                            | —                                               | Transmits a contiguous byte range over the active connection.                 |
| Receive    | `Recv`, `RecvStream`, `RecvRange` | `Recv`, `RecvStream`, `RecvRange` | —                                               | Receives data via eager buffers, lazy stream markers, or ranges.              |
| Close      | `Close`                           | `Close`                           | `Close`                                         | Performs protocol-level graceful shutdown and closes the descriptor.          |
| Abort      | `Abort`                           | `Abort`                           | `Abort`                                         | Immediately tears down and closes the socket without a graceful handshake.    |

<div class="interactive_msc">
\msc
hscale="2.4";
background.color=none;

client   [label="ClientSocket"  , textbgcolor="transparent", textcolor="#2f4153", linecolor="#1779c4"],
server   [label="ServerSocket"  , textbgcolor="transparent", textcolor="#2f4153", linecolor="#1779c4"],
listener [label="ListenerSocket", textbgcolor="transparent", textcolor="#2f4153", linecolor="#1779c4"];

vspace 30;

listener => listener [label="Listen() / ListenOne()"       , linecolor="#1779c4", textcolor="#1779c4"];
vspace 50;
client   => listener [label="Connect() [Handshake]"        , linecolor="#1779c4", textcolor="#1779c4"];
listener => server   [label="AcceptOne() -> FromAccepted()", linecolor="#2f4153", textcolor="#2f4153"];

vspace 50;

client <=> server [label="Send() -> Recv()/RecvStream()", linecolor="#2f4153", textcolor="#2f4153"];
client <=> server [label="Close() / Abort()", linecolor="#6f7e8e", textcolor="#6f7e8e"];

vspace 30;

listener => listener [label="Close() / Abort()", linecolor="#6f7e8e", textcolor="#6f7e8e"];
\endmsc
</div>

The next section explains how to use each socket, and the second one explains what the aliases hide and how to work
without them.

## Using sockets

One additional thing that you need to know is @ref Hermes::SocketDataConcept "SocketData". SocketData are classes that
store the context information about a socket like hostname, callbacks, endpoint IP/port, security info, and so on (this
is how we share info between policies, every socket needs one).

### Client

First you need to construct the SocketData with the necessary info to identify the other endpoint and then pass it to
the @ref Hermes::ClientSocket::Connect(SocketData &&) "ClientSocket::Connect" factory. This function will try to connect to the remote
endpoint and return a `ClientSocket` if successful. After it, you can call `Send` and `Recv` to send and receive data.

For a TLS request, to construct @ref Hermes::TlsSocketData "TlsSocketData" you need to resolve and endpoint using the
URL and pass the URL to the SocketData before connecting to a @ref Hermes::RawTlsClient "RawTlsClient" (TLS needs to
keep the hostname for it entire life). The result must be checked before the socket is used, but since this library was
designed with `std::expected` in mind you can specify it in a monadic way.

```cpp
#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <string_view>

std::expected<std::string, Hermes::ConnectionErrorEnum> Fetch(std::string hostname) {
    static constexpr std::unexpected unknownError{ Hermes::ConnectionErrorEnum::Unknown };
    static constexpr std::string_view requestData{
        "GET / HTTP/1.1\r\n"
        "Host: example.org\r\n"
        "Connection: close\r\n\r\n"
    };
    
    const auto createSocket{ [&](const Hermes::IpEndpoint endpoint) {
        return Hermes::RawTlsClient::Connect({ endpoint, hostname });
    } };

    const auto makeRequest{ [&](Hermes::RawTlsClient&& client) {
        return std::move(client).SendAndLift(requestData);
    } };

    const auto getResponse{ [&](Hermes::RawTlsClient&& client) -> std::expected<std::string, Hermes::ConnectionErrorEnum> {
        auto socketView{ client.RecvStream<char>() }; // more info in the Range.cpp example

        if (!rg::starts_with(socketView, "HTTP/1.1 200 OK"sv))
            return unknownError;
        const auto headers { socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv) | rg::to<std::string>() };
        const auto chunkLen{ socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };
        const auto body    { socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };

        static constexpr auto ConnClose{ Hermes::ConnectionErrorEnum::ConnectionClosed };
        if (socketView.Error().error_or(ConnClose) != ConnClose)
            return unknownError;

        return body;
    } };
    
    Hermes::IpEndpoint::TryResolve(hostname, "https")
            .and_then(createSocket)
            .and_then(makeRequest)
            .and_then(getResponse);
}
```

> `RawTlsClient` is a convenience alias, not a special transport implementation. Despites having different
> `SocketData`'s, this same pattern can be used in every sync client socket.

> Despites the use of `RecvStream in this example`, you can use `client.Recv(buffer)` too.
> Sometimes you can need to replace `RecvRange` with `RecvStream`. The difference is not cosmetic: `RecvRange` may read
> ahead while advancing, whereas `RecvStream` reads when its current value is requested. See
> `@ref recv-stream-behavior` to learn more about the difference.

You can also pass some options to a connect/send/receive step, like deadline/timeout, TLS settings, SO settings
(`setsockopt`), etc.

```cpp
const auto deadline{ std::chrono::steady_clock::now() + 5s };
const auto [sent, err]{ client->Send( payload, { .deadline = deadline }) };
```


### Server


A server follows the opposite direction of the client. Instead of creating a connected socket directly, you first create
a `ListenerSocket`, bind it to a local endpoint and then accept a connection from it. Each successful acceptance
produces a `ServerSocket` that owns the connection with that client. It accepts one connection, reads one request, talks
with a client and then finishes.

```cpp
#include <Hermes/Socket/Sync/ListenerSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>
#include <format>
#include <print>
#include <ranges>
#include <string>
#include <string_view>

namespace rg = std::ranges;

// RunServer
//     └── Listen
//           └── ProcessConnections
//                 └── AcceptAll
//                       └── ProcessRequest

Hermes::ConnectionResultOper ProcessRequest(Hermes::RawTcpServer&& server) {
    using namespace std::literals::string_view_literals;

    auto request{ server.RecvStream<char>() };
    const auto requestLine{ request
            | Hermes::Utils::UntilMatch("\r\n"sv)
            | rg::to<std::string>() };

    std::println("Request line:\n{}", requestLine);

    const auto headers{ request
            | Hermes::Utils::UntilMatch("\r\n\r\n"sv)
            | rg::to<std::string>() };

    std::println("Headers:\n{}", headers);

    constexpr std::string_view body{ "<h1>Hello World!</h1>" };
    const auto response{
        std::format(
            "HTTP/1.1 200 OK\r\n"
            "Server: Hermes/0.5\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n\r\n"
            "{}",
            body.size(), body) };

    return server.Send(response).second;
}

Hermes::ConnectionResultOper ProcessConnections(Hermes::RawTcpListener&& listener) {
    for (auto&& result : listener.AcceptAll()) {
        if (!result)
            return std::unexpected{ result.error() };

        if (const auto processed{ ProcessRequest(std::move(*result)) }; !processed)
            std::println("An error occurred while processing a client");
    }

    return std::monostate{};
}

std::expected<std::monostate, Hermes::ConnectionErrorEnum> RunServer() {
    using namespace std::literals::string_view_literals;

    static constexpr Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({ 127, 0, 0, 1 }), 8080 };
    static constexpr auto logListening{ [&](auto listener) {
        std::println(
            "Listening on http://{}:{}...",
            endpoint.GetIp( ), endpoint.GetPort());
        return std::move(listener);
    } };

    return Hermes::RawTcpListener::Listen(Hermes::DefaultSocketData<>{ endpoint })
            .transform(logListening)
            .and_then(ProcessConnections);
}

```

Both examples were very optimistic. Error handling is kept visible, but these examples does not yet implement a complete
HTTP parser.

## What the alias is made of

### Client

The client concept of a client socket is a composition of three independent responsibilities:

| Policy                                                   | Role                                                                                    |
|----------------------------------------------------------|-----------------------------------------------------------------------------------------|
| @ref Hermes::SocketDataConcept       "Socket data"       | Owns the socket context, like the native handle, endpoint and transport-specific state. |
| @ref Hermes::ConnectionPolicyConcept "Connection policy" | Knows how to connects and performs transport setup.                                     |
| @ref Hermes::TransferPolicyConcept   "Transfer policy"   | Knows how to send and receive data.                                                     |

These responsabilities are implemented as classes, and @ref Hermes::ClientSocket "ClientSocket" is just a template class
that acts as a facade to these underlying classes.

The client-side policy composition can be represented conceptually as:

```cpp
using Client = Hermes::ClientSocket<MySocketData, MyConnectionPolicy, MyTransferPolicy>;
```

### Server

The server side uses the same policy-based composition, but instead of the ConnectionPolicy it has the @ref
Hermes::AcceptPolicyConcept "AcceptPolicy". This policy is responsible for listening, accepting incoming connections and
producing a new `ServerSocket`.

`ListenerSocket` owns the listening `SocketData` and the `AcceptPolicy` instance. When `Listen` is called, the facade
delegates socket creation, option application, binding and listening to the policy. The policy receives the local
endpoint through `SocketData` and uses `ListenOptions` to configure the listening socket.

After the listener has been created, `AcceptOne` delegates the incoming-connection operation to the same policy. The
policy receives the listener's `SocketData` and a child `SocketData` created from the listener's data. It fills that
child with the accepted native handle and the remote peer endpoint. The facade then wraps the accepted child with
`ServerSocket::FromAccepted` and returns the resulting `ServerSocket`.

`AcceptAll` is built on top of this operation. It repeatedly accepts connections and yields either an accepted
`ServerSocket` or the corresponding connection error. The options passed to `AcceptAll` belong to `AcceptPolicy`, they
are not transfer options for the sockets that the generator produces.

The distinction between the listener and the accepted server socket is therefore:

| Socket           |  Owns                                                       |
|------------------|-------------------------------------------------------------|
| `ListenerSocket` | The listening `SocketData` and `AcceptPolicy`               |
| `ServerSocket`   | The child `SocketData`, `AcceptPolicy` and `TransferPolicy` |

A `ServerSocket` never establishes a connection through `Connect`. Its connection already exists when `FromAccepted`
wraps the child `SocketData`. The accepted socket keeps the `AcceptPolicy` because accept-side protocol state can
still be needed when the connection is closed.

This is especially important for TLS. `TlsAcceptPolicy` first performs the ordinary accepted-socket setup and then runs
the server-side TLS handshake. It also retains the TLS accept state required for protocol-level teardown. In this case
accepting a connection means more than getting a native TCP handle, it also includes establishing the server-side TLS
session.

Although `ListenerSocket` does not use `TransferPolicy` directly while listening or accepting, it still carries the
policy as a template parameter. The policy is part of the `ServerSocketType` that the listener creates for every
accepted connection.

The server-side policy composition can be represented conceptually as:

```cpp
using Listener = Hermes::ListenerSocket<MySocketData, MyAcceptPolicy, MyTransferPolicy>;

using Server = Hermes::ServerSocket<MySocketData, MyAcceptPolicy, MyTransferPolicy>;
```
### Implementing policies

Use the aliases while the default policy contracts match the application. Move to an explicit composition when one of
the following is true:

- the native handle or per-connection state is not represented by the default `SocketData`;
- connection setup requires a protocol step not provided by the default connection policy;
- accepting a connection needs application-specific filtering or setup;
- transfer semantics need a different framing, buffering or encryption boundary;
- a custom policy must expose an operation-specific option type.

If you encount some of these cases, see the @ref policy-architecture section for more information.

## A useful order for learning the library

Read @ref policy-architecture to understand the composition model. Then read @ref policy-options for operation-wide
options and @ref recv-stream-behavior with @ref send-lift-pattern for the central receiver design. Finally, use
@ref example-tls-client to connect the concepts to complete flows.

To know more about async sockets, see @ref async-sockets (not implemented yet, await maybe for v0.6.7).
