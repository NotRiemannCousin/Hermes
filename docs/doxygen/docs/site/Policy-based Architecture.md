@page policy-architecture Policy-based Architecture

# Policy-based architecture

Hermes is modular because a socket operation is different thing as the state that stores a socket. The library keeps
those concerns separate so that a transport can replace one part of the composition without forcing a new public
programming model.

The easiest way to read the design is to treat each type as a lego. `SocketData` stores state. A connection policy
creates an outgoing connection. An accept policy creates incoming connections. A transfer policy moves application data.
The synchronous or asynchronous facade assembles those pieces and forwards the operations exposed by the selected
policies.

## The four responsibilities

### Socket data: state and ownership

A socket data type owns the native handle and the state required by its transport. `DefaultSocketData` represents an
ordinary TCP case, but `TlsSocketData` also stores information about the encryption process, like session state and
decrypt buffers. Every socket needs to specify at least:
- a @ref Hermes::AddressFamilyEnum "AddressFamilyEnum" representing the family type (`SocketData::Family` constexpr variable);
- a @ref Hermes::SocketTypeEnum "SocketTypeEnum" representing the socket type (`SocketData::Type` constexpr variable);
- a @ref Hermes::AddressFamilyEnum "endpoint" representing the endpoint type (`data.endpoint` variable, its type needs to be `SocketData::EndpointType`);
- a `SOCKET` file descriptor (`data.socket` variable, named as `SocketFd`);

`Family` and `Type` are read in a `constexpr` context by `SocketDataConcept`, so they have to be known before any policy
runs. A `SocketData` can't pick its address family or socket type from a runtime value, that decision belongs to the
template arguments.

`SocketData` is move-only: the concept requires `std::movable` and forbids `std::copyable`. Copying a struct that
carries a raw OS handle would produce two owners for the same descriptor, and whichever one gets destroyed first closes
the socket out from under the other. A custom `SocketData` has to carry that guarantee through its own move constructor
and move-assignment operator, leaving the moved-from socket in a invalid state that a later destructor can recognize and
skip.

Socket data should answer questions about state and ownership, not questions about how a connection is established or
how bytes are framed. A custom `SocketData` type is appropriate when the transport needs additional state, complex
endpoint info or a different lifetime boundary.

A few practices are worth carrying over to a custom `SocketData`.

**Keep large or variable-size buffers behind a pointer, not inlined as a member.** `TlsSocketData` needs an 18 KB buffer
for encrypted bytes and a 64 KB buffer for decrypted bytes, but neither sits directly on the struct. Both live inside a
nested `State`, and `SocketData` only holds a `std::unique_ptr<State>`. The reason is move cost, a `ClientSocket` moves
its `SocketData` every time ownership changes hands. For example when `Connect()` returns the finished socket or when a
server hands an accepted connection to its caller. If the buffers sat inline, every one of those moves would touch tens
of Kb of memory instead of a single pointer.

**Say who owns what.** `TlsSocketData::credentials` is a raw, non-owning pointer to a `Credentials` object the
caller supplies. `SocketData` doesn't manage its lifetime, and the type system doesn't enforce that contract either.
A raw pointer can't tell a reader whether it owns the object it points to. Document the expectation directly,
credentials must outlive the connection, and the caller is responsible for that, not `SocketData`. Reach for an owning
type only when the socket data genuinely needs to control the lifetime, otherwise a documented borrow keeps the struct
cheap to move.

**Keep heavy third-party types out of the header.** The TLS handshake and transfer logic depend on OpenSSL types, but
`TlsSocketData.hpp` never includes an OpenSSL header directly. It stores `std::unique_ptr<ITlsConnectStateMachine<...>>`,
`std::unique_ptr<ITlsTransferStateMachine<...>>` and `std::unique_ptr<ITlsAcceptStateMachine<...>>`, interfaces, not
concrete implementations. Anyone who includes `TlsSocketData.hpp` doesn't need to see OpenSSL at all.

**Decide deliberately what `MakeChild()` carries over.** An accept policy builds each accepted connection by calling
`MakeChild()` on the listening socket's data, then fills in the new socket handle and endpoint. `MakeChild()` should
copy the configuration shared across every connection from this listener and leave anything connection-specific (the
socket handle, buffers, session state) at its default. A `MakeChild()` that deep-copies runtime state defeats the reason
accepted connections get built through it in the first place.

### Connection policy: creating an outgoing connection

A connection policy is the client side of establishing a socket. It exposes three operations. `Connect(data, opt)`
attempts to reach a remote endpoint and reports a `ConnectionResultOper`. `Close(data)` tears the connection down the
way its protocol expects. `Abort(data)` terminates it without that courtesy.

`ConnectionPolicyConcept` ties a policy to one specific combination of address family, socket type and endpoint type,
`Policy::Family`, `Policy::Type` and `Policy::EndpointType` all have to match the `SocketData` it's used with. A
connection policy isn't meant to be generic across every possible transport, it's written for the one combination its
template arguments describe.

`Options` is a nested type on the policy, not a free-standing struct, because different policies need different
configuration to establish a connection. `DefaultConnectPolicy::Options` builds itself out of small pieces that only
exist for the combination they apply to, `onlyIpv6` is only present when `Family` is `Inet6`, and `tcpNoDelay` only
exists when `Type` is `Stream`. A custom policy should follow the same rule and avoid exposing a knob that has no
meaning for the family or socket type it's instantiated with. `Options` also decides how convenient the call site looks,
the facade only offers the short `Connect(data)` overload when `Options` is `std::default_initializable`. A policy that
needs meaningful configuration, a credentials bundle, a required scheduler, should leave `Options` without a sensible
default rather than fabricate one just to keep the short overload available.

`Close` and `Abort` exist as two separate functions because they mean two different things, and a custom policy should
keep that difference real rather than routing both through the same teardown. `Close` performs whatever graceful
shutdown the transport calls for, a TLS policy sends a close-notify alert before it releases the socket, a plain TCP
policy still allows a clean FIN. `Abort` skips all of that and drops the connection immediately. Collapsing them into
one implementation removes the caller's ability to choose between a clean shutdown and an immediate one.

A connection policy also owns cleanup on its own failure path. If `Connect` fails partway through, whatever OS resources
it already created are its responsibility to release. The `SocketData` handed back to the caller should never carry a
half-valid socket handle.

### Accept policy: creating an incoming connection

An accept policy is the server side of the same problem, split across two operations that run at different points in a
server's life. `Listen(data, backlog, opt)` runs once, it binds an address and starts listening, and the`SocketData` it
operates on becomes the long-lived listening socket. `Accept(data, acceptedData, opt)` runs repeatedly after that, once
per incoming connection, and each call fills in one `acceptedData` built from the listening socket through
`MakeChild()`.

That split explains why `Listen` and `Accept` take separate option types, `ListenOptions` and `AcceptOptions`, instead
of sharing one. They configure different sockets at different moments. `ListenOptions` covers things that apply once to
the listening socket itself, address reuse, its receive and send buffer sizes. `AcceptOptions` covers what should apply
to every connection accepted through it, `TCP_NODELAY`, keep-alive, and for a TLS accept policy, a handshake timeout.
Folding both into one type would force a caller configuring the listener to also think about per-connection settings
that don't apply yet, and vice versa.

`Close` and `Abort` carry the same split they have on the connection policy side, applied to the server. `Close`
stops listening and shuts the server socket down gracefully, while `Abort` terminates one already-accepted connection
abruptly. A server genuinely needs both, closing the listener is not the same operation as dropping one misbehaving
peer, so a custom accept policy shouldn't merge them.

### Transfer policy: moving application data

A transfer policy moves bytes over an already-established socket, and it exposes that in two shapes.
`Recv(data, bufferRecv, recvMode)` and `Send(data, bufferSend)` work directly against a caller-supplied buffer and
return a @ref Hermes::StreamByteOper "StreamByteOper". Alongside that, a lazy `RecvStream` satisfies
`std::ranges::input_range`, yields a `std::byte` one at a time, and exposes an `Error()` method for whatever went wrong
during iteration. Both access patterns have to exist, callers pick whichever shape fits their loop.

@ref Hermes::RecvModeEnum "RecvModeEnum" decides when `Recv` stops. `All` keeps reading until the whole span is filled,
`Any` returns as soon as one block of data arrives. That choice is part of what `Recv` means, not an optional extra, so
every transfer policy has to honor it the same way. It's a required parameter for that reason, unlike settings a policy
is free to ignore.

What a transfer policy should leave alone is any notion of application protocol. Reading length-prefixed messages,
searching for a delimiter, these questions belong in the application or in a separate range adaptor layered on top,
not inside the policy just because the policy already has access to the bytes. See @ref policy-options for how that
deadline is threaded through `Send`/`Recv` options in practice.

## How the facade composes the lego

The exact template spelling depends on the facade, but the conceptual shape is stable:

```cpp
using Client = Hermes::ClientSocket<MySocketData, MyConnectionPolicy, MyTransferPolicy>;
```

The facade should not reimplement the policy logic. Its role is to provide the public socket vocabulary, select
overloads when an options type is default-initializable, and forward explicit options when a policy requires
construction.

This separation also explains why the convenience aliases are useful. `RawTlsClient` is a preassembled client for the
ordinary TLS case. It removes template noise without removing the underlying extension point.

## Designing a custom policy

A custom policy should begin from a contract, not from a copy of an existing implementation. Define the state the
operation needs, the result it returns, the options it accepts and the errors it can report. Then implement the smallest
policy that satisfies the corresponding facade concept.
Do not put application protocol parsing into a transfer policy merely because the policy has access to bytes. Transfer
policies should expose transport behavior, parsing belongs in the application or in a separate range adaptor such as
`UntilMatch`.

## Policy composition and options

The facade can expose a short overload only when the policy's options type is `std::default_initializable`. Otherwise,
callers must provide the options object explicitly. This distinction is intentional, it lets a policy require meaningful
construction while preserving concise calls for ordinary policies.

For example, a transfer policy can require a configured buffer or a scheduler in its options type. The facade can still
offer both forms without assuming that `{}` is a valid value for every policy:

```cpp
const Hermes::RawTcpClient::SendOptions options{ .deadline = std::chrono::steady_clock::now() + 500ms };

const auto [sent, error] = client.Send(payload, options);
```

The same rule applies to `RecvOptions`, including options passed when creating `RecvStream` or `RecvRange`. See
@ref policy-options for the operation-wide semantics.
