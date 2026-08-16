@page policy-options Policy options

# Policy options

Every policy in Hermes accepts its configuration through a nested options type. @ref policy-architecture explains why
that shape exists. This page walks through what each options type actually looks like and how the facade decides which
overload a call resolves to.

## The two-overload pattern

A facade method that takes an options type comes in two overloads. One takes no options and only exists when the
policy's options type is `std::default_initializable`, it default-constructs `{}` internally and forwards to the other
overload. The other always exists and takes the options type explicitly.

```cpp
auto client{ Hermes::RawTcpClient::Connect(std::move(data)) };            // uses ConnectionPolicy::Options{}
auto client{ Hermes::RawTcpClient::Connect(std::move(data), myOptions) }; // uses an explicit ConnectionPolicy::Options
```

If a policy's option type can't default-construct, the caller only sees the second overload. For example,
@ref Hermes::TlsSocketData "TlsSocketData" already needs a hostname to connect and it can be needed again in
renegotiation, so this shows up naturally on the socket data side rather than on `Options`, but the same rule applies
wherever a policy author decides `Options{}` isn't a meaningful default. There's nothing
special about the short overload beyond that, it's `Connect(data)` calling `Connect(data, {})`.

## Connection options

`ConnectionPolicy::Options` configures a single `Connect()` call.
@ref Hermes::DefaultConnectPolicy::Options "DefaultConnectPolicy::Options" carries `connectionTimeout`, `keepAlive`,
and, only when they apply to the family or socket type in use, `onlyIpv6` and `tcpNoDelay`. `TlsConnectPolicy::Options`
builds on top of it and adds `handshakeTimeout`, `ignoreCertificateErrors` and `requestMutualAuth`.

```cpp
const Hermes::RawTlsClient::ConnOptions options{ .connectionTimeout = 3s, .handshakeTimeout  = 2s };

auto client{ Hermes::RawTlsClient::Connect({ endpoint, hostname }, options) };
```

## Listen and accept options

Accept policies split their configuration in two, because `Listen` and `Accept` configure different sockets at
different times. `ListenOptions` applies once, to the listening socket itself, `reuseAddress` and the listening
socket's buffer sizes live here. `AcceptOptions` applies to every connection the listener accepts from that point on,
`tcpNoDelay`, `keepAlive`, and for `TlsAcceptPolicy`, `handshakeTimeout` and `requestClientCertificate`.

```cpp
const Hermes::RawTlsListener::ListenOptions listenOptions{ .reuseAddress = true };
auto listener{ Hermes::RawTlsListener::Listen(std::move(data), listenOptions, 128) };

const Hermes::RawTlsListener::AcceptOptions acceptOptions{ .handshakeTimeout = 2s };
for (auto&& result : listener->AcceptAll(acceptOptions)) {
    // ...
}
```

## Send and receive options, and the deadline

Transfer operations use two separate options types, `SendOptions` and `RecvOptions`, even though today both only carry
one field, `deadline`. They're kept separate because a field meaningful only for one direction, some framing or
buffering hint that only makes sense while reading, for instance, would otherwise be silently accepted and ignored on
the other call.

`deadline` is a `std::optional<std::chrono::steady_clock::time_point>`, an absolute point in time, not a duration.
Leaving it unset preserves the policy's normal blocking behavior. Setting it bounds the whole call, not each underlying
read or write inside it.

```cpp
const auto deadline{ std::chrono::steady_clock::now() + 5s };

const auto [sent, sendErr]{ client.Send(payload, { .deadline = deadline }) };
const auto [got , recvErr]{ client.Recv(buffer, Hermes::RecvModeEnum::All, { .deadline = deadline }) };
```

If `Recv` needs several underlying reads to fill `buffer`, all of them check against the same `deadline`, none of them
restart the clock. A deadline reached while receiving reports `ConnectionErrorEnum::ReceiveTimeout`, one reached while
sending reports `SendTimeout`.

This matters most once a caller starts composing several calls into one logical operation, a request/response round trip
that connects, sends a request and reads a response across multiple `Recv` calls, for instance. The caller owns that
composition, not the policy, so it's the caller's job to compute a fresh `deadline` before each step from the
operation's overall budget, `remaining = overallDeadline - now()`, rather than pass the same fixed duration to every
call and let the total run far longer than intended.

`RecvStream` and `RecvRange` take the same `RecvOptions`, and follow the same default-initializable rule as every other
options-accepting method.

```cpp
auto stream{ client.RecvStream<char>({ .deadline = deadline }) };
```
