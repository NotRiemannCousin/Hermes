@page recv-stream-behavior RecvStream vs RecvRange Behavior

# RecvStream vs RecvRange Behavior

Hermes provides two distinct range-like iterators for reading from sockets: `RecvStream` and `RecvRange`. Their fetching mechanisms and termination conditions differ significantly. Using the wrong one can lead to indefinitely hanging threads.

## Core Mechanics: `operator*` vs `operator++`

The primary difference lies in exactly *when* the network read operation occurs:

| **Action**         | **RecvStream**                  | **RecvRange**                             | 
|--------------------|---------------------------------|-------------------------------------------|
| **`operator*()`**  | **Blocks and receives data.**   | Returns previously cached data.           | 
| **`operator++()`** | No-op (simply returns `*this`). | **Blocks, receives data, and caches it.** | 
| **Read-ahead**     | Zero read-ahead.                | 1-byte read-ahead.                        | 

## The Keep-Alive Trap (Hanging Threads)

Why does `RecvStream` exist if `RecvRange` complies with standard C++ iterators? The answer is **persistent connections**, such as HTTP Keep-Alive.

Because `RecvRange` performs a read-ahead on `operator++` to correctly evaluate `it != end()`, it aggressively asks the socket for the next byte. If the server finishes sending a complete payload but keeps the underlying connection open waiting for your next request, `operator++` will **block indefinitely**. Your thread hangs forever because the loop tries to read a byte that doesn't exist yet.

`RecvStream` solves this. By pushing the read operation to `operator*()`, you can parse the payload, logically detect the end of the message (e.g., matching a Content-Length), and safely `break` the loop without triggering an unwanted read-ahead.

## Termination and DropLastView

To comply with standard C++ iterators and terminate the loop gracefully on closed sockets, `RecvRange` wraps the stream in a `DropLastView`.

It does not scan for specific terminator bytes. It simply evaluates to `true` when the underlying socket reaches the end of the stream (`== end()`), unconditionally discarding the last element yielded (which acts as the internal EOF marker).

## Stateful Iteration (No Iterator Invalidation)

Unlike standard `input_range` iterators where advancing one copy might invalidate others, `RecvStream` maintains the socket's read state centrally within the view object itself. Advancing any iterator safely advances the global state of the stream.

**Why is this useful?** It allows you to sequentially chain multiple range-based algorithms on the exact same view to parse a protocol step-by-step. Reading a chunk of bytes leaves the view perfectly positioned for the next algorithm, avoiding iterator invalidation bugs entirely.

```cpp
auto socketView = client.RecvStream<char>();

// 1. Extract exactly 5 bytes. The stream state advances safely.
const auto statusCode = Hermes::Utils::ExtractTo<std::array<char, 5>>(socketView);

// 2. Read until the next CRLF. Picks up exactly where step 1 left off.
const auto statusMessage = socketView 
                         | Hermes::Utils::UntilMatch("\r\n"sv) 
                         | std::ranges::to<std::string>();

// 3. Read headers until double CRLF. Picks up exactly where step 2 left off.
const auto headers = socketView 
                   | Hermes::Utils::UntilMatch("\r\n\r\n"sv) 
                   | std::ranges::to<std::string>();
```

This pattern turns complex, manual buffer management into clean, declarative algorithm chaining.

## Limiting Reads for Security (`std::views::take`)

Because `RecvStream` integrates seamlessly with `<ranges>`, you can combine it with `std::views::take` to enforce strict upper bounds on read operations. This is critical for security, preventing malicious actors from sending infinite streams of data (spam) to exhaust your memory when parsing dynamically sized fields.

```cpp
auto socketView = client.RecvStream<char>();

// Protection: Limit reading to a maximum of 1024 bytes.
// If the "\r\n\r\n" pattern is not found within this limit, 
// the view will be interrupted and the read will terminate safely.
auto safeHeadersView = socketView | std::views::take(1024);

const auto headers = safeHeadersView 
                   | Hermes::Utils::UntilMatch("\r\n\r\n"sv) 
                   | std::ranges::to<std::string>();
```

## Comparative Example

### Using `RecvStream` (Direct, No Read-Ahead)

Best for persistent connections and manual parsing.

```cpp
auto stream = client.RecvStream();
auto it = stream.begin();

while (it != stream.end()) {
    char c = *it; // Network read happens exactly HERE.
    
    if (c == '\n') break; // Loop exits safely, no read-ahead is triggered.
    
    buffer.push_back(c);
    ++it; // No-op.
}
```

### Using `RecvRange` (C++ Ranges Compatible)

Best for closed-stream scenarios and STL algorithms.

```cpp
auto range = client.RecvRange();

// '++' inside the loop mechanics performs the network read.
// '*' simply returns the cached byte.
for (char c : range) { 
    buffer.push_back(c);
    // Loop ends automatically when the connection closes.
    // DANGER: Will hang here if the connection is kept alive.
}
```

## Decision Table

| **Scenario**                         | **Recommendation**        | **Reason**                                                      | 
|--------------------------------------|---------------------------|-----------------------------------------------------------------|
| Persistent/Keep-Alive connections    | **`RecvStream`**          | Avoids blocking the thread on the next `operator++` read-ahead. | 
| Custom delimiter parsing             | **`RecvStream`**          | Gives exact control over network reads.                         | 
| Zero-copy / strict fetching          | **`RecvStream`**          | Fetches strictly when requested by `operator*`.                 | 
| Protocol chunking/algorithm chaining | **`RecvStream`**          | Preserves state perfectly across sequential algorithm calls.    | 
| Security bounds / Spam prevention    | **`RecvStream` + `take`** | Safely limits unbounded protocol fields dynamically.            | 
| Standard `for` loops                 | **`RecvRange`**           | Safely increments and evaluates loop conditions.                | 
| STL `<ranges>` algorithms            | **`RecvRange`**           | Behaves like a standard input iterator.                         |