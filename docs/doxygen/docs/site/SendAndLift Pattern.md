@page send-lift-pattern SendAndLift and Composition with and_then

# SendAndLift and Composition with and_then

`Send` reports whether it managed to send everything, `total` on success is always equal to the buffer size, `Send`
either sends it all or returns an error. `SendAndLift` exists to carry the socket forward through a `std::expected`
pipeline once that check passes, instead of a hand-written lambda re-capturing it at every step.

## The problem it replaces

Without a lift, threading a socket through `.and_then()` after a `Send` looks like this.

```cpp
const auto s_makeRequest{ [&](Hermes::RawTlsClient&& client) {
    auto val{ client.Send(url.FormatRequest()) };

    auto s_returnClient{ [client{ std::move(client) }](const auto) mutable {
        return std::move(client);
    } };
    return val.second.transform(s_returnClient);
} };
```

The lambda inside the lambda exists for one reason, `Send` gives you back a byte count and an error, not the socket.

## SendAndLift is a factory, not the operation itself

`SendAndLift` doesn't send anything by itself. It's a `static` function that captures the data and options you give
it and returns a lambda, and that lambda is what performs the send once `.and_then()` invokes it with a socket.

```cpp
[[nodiscard]] static auto SendAndLift(R&& data) noexcept
    requires std::default_initializable<SendOptions>;

[[nodiscard]] static auto SendAndLift(R&& data, SendOptions options) noexcept;
```

```cpp
template<ContiguousByteRange R>
auto ClientSocket<SocketData, ConnectionPolicy, TransferPolicy>::SendAndLift(R&& data, SendOptions options) noexcept {
    return [options, fData{ std::forward<R>(data) }](ClientSocket& self) -> ConnectionResult<ClientSocket> {
        auto val{ self.Send(std::forward<R>(fData), options) };

        auto movFromThis{ [client{ std::move(self) }](const auto) mutable {
            return std::move(client);
        } };
        return val.second.transform(movFromThis);
    };
}
```

The earlier problem collapses into a single expression at the call site.

```cpp
const auto s_makeRequest{ ClientSocket::SendAndLift(url.FormatRequest()) };
// ...
return connectResult.and_then(s_makeRequest);
```

## Why a factory instead of calling Send and lifting directly

An earlier version of this pattern tried to make `SendAndLift` an instance method you call directly, so a step in a
chain could pass `&Socket::SendAndLift` straight into `.and_then()`. That ran into two separate problems, and the
factory shape here avoids both at once.

Taking the address of an overloaded member function is ambiguous without a full argument list to pick a candidate
from, so `SendAndLift(data)` and `SendAndLift(data, options)` couldn't share one name if either was meant to be taken
by address. As a factory, `SendAndLift(data)` is called with parentheses, like every other overload in the library,
and returns a plain lambda. Nothing ever takes its address, so the two overloads coexist the normal way.

A `&&`-qualified instance method also doesn't compose cleanly with `std::expected::and_then`, since `and_then`
forwards its contained value according to the value category of the `expected` object at that point in the chain,
and a chain doesn't keep that value category rvalue at every step, an intermediate lambda that stores the socket in
a named variable before returning it turns it into an lvalue, which a `&&`-qualified method can't bind to. The
lambda `SendAndLift` returns takes a plain `ClientSocket&` instead, which `and_then` can bind to either way, so
nothing here depends on the value category of the chain at all.

## The trade that's still there

Dropping the `&&` removes a compile-time guard, not the underlying risk. The returned lambda still moves out of
`self` to build the `ClientSocket` it hands back on success, so a variable passed through it is left moved-from
either way, there's just no `&&` on an instance method left to catch a caller reusing it by mistake. Reserve
`SendAndLift` for pipeline code built through `.and_then()`, where the socket is a value passed along the chain and
never touched again outside it, and prefer plain `Send` when the socket stays in a named variable you intend to keep
using afterward.

## Why there is no RecvAndLift

The same idea does not carry over to `Recv`. `Recv` reports `ConnectionErrorEnum::ConnectionClosed` through the
error channel whenever the peer closes the socket, but a closed connection is not necessarily a failure, it is
frequently the expected end of a response. A `RecvAndLift` would have to treat that as a hard error to keep its
lambda's return type as `expected<Socket, Error>`, silently making that call for the caller.

It also would not have anything left to hand back either way. By the time `Recv` reports `ConnectionClosed`, the
underlying socket has already been closed and its handle invalidated, there is no live socket to lift back into the
chain regardless of how the close is interpreted.

## Decision table

| Scenario                                                       | Use                      | Reason                                                                          |
|----------------------------------------------------------------|--------------------------|---------------------------------------------------------------------------------|
| Socket stays in a named variable used again later              | `Send` / `Send(opt)`     | Keeps the value in your control, no risk of an unnoticed moved-from.            |
| Building a `.and_then()` chain, default options                | `SendAndLift(data)`      | Returns a plain lambda, no member-pointer ambiguity, no ref-qualifier to fight. |
| Building a `.and_then()` chain, explicit deadline or options   | `SendAndLift(data, opt)` | Same factory, same overload set as everywhere else in the library.              |
| Reading a response where the connection closing ends it        | `Recv` / `RecvStream`    | The caller decides whether `ConnectionClosed` is expected, not the library.     |


> @ref https://github.com/NotRiemannCousin/Viracocha "Viracocha" is a coding style guide based in this monadic design.
> Planned to be enforced in Hermes soon.