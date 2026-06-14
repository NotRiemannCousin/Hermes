// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
#include <string>
#include <array>
#include <algorithm>
#include <ranges>

#include "_base.hpp"

namespace rg = std::ranges;
namespace vs = std::views;

// This procedural approach demonstrates a traditional imperative flow using early returns.
// However, instead of manually buffering bytes in a `while(true)` loop, it leverages
// Hermes' modern input ranges (`RecvStream` and `UntilMatch`) to process the incoming
// TCP stream safely and sequentially.

extern ExpString MakeRequest() {
    using namespace std::literals::string_view_literals;

    // 1. Resolve the endpoint.
    auto endpointRes{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!endpointRes)
        return std::unexpected{ std::format("{}", endpointRes.error()) };

    // 2. Connect the socket.
    auto clientRes{ Hermes::RawTlsClient::Connect(Hermes::TlsSocketData<>{ *endpointRes, url.hostname }) };
    if (!clientRes)
        return std::unexpected{ std::format("{}", clientRes.error()) };

    auto client{ std::move(*clientRes) };

    // 3. Prepare and send the HTTP request.
    auto [bytesSent, sendErr]{ client.Send(url.FormatRequest()) };
    if (!sendErr)
        return std::unexpected{ std::format("{}", sendErr.error()) };

    // 4. Receive and process the response using Ranges procedurally.
    auto socketView{ client.RecvStream<char>() };

    // `RecvStream` is an input_range, so it consumes the bytes when you advance the iterator.
    // Advancing an iterator of an input_range can cause invalidation of other iterators, but the
    // current state is stored in the range itself, so all iterators represent the current stream state.

    if (!rg::starts_with(socketView, "HTTP/1.1"sv))
        return std::unexpected{ "Non supported version" };

    // Copying exact bytes bypasses the need for string manipulation on fixed-size fields.
    const auto statusCode{ Hermes::Utils::ExtractTo<std::array<char, 5>>(socketView) };

    if (!rg::equal(statusCode, " 200 "sv))
        return std::unexpected{ std::format("error code: {:s}", statusCode) };

    const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };

    // This range must receive more bytes just when reading with the * operator. Receiving when
    // advancing isn't optimal because if your protocol uses a terminated sequence, you will need
    // more work to stop at the last byte.
    // e.g.: `UntilMatch("\r\n"sv)` reads until the first occurrence of "\r\n", discards the pattern
    // (exclusive match), and advances the internal stream state to stop at the next byte of the payload.

    const auto headers { socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv) | rg::to<std::string>() };
    const auto chunkLen{ socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };
    const auto body    { socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };

    // The stream inherently handles errors internally. Checking the state post-extraction
    // guarantees that our string generation wasn't interrupted by a TCP failure.

    static constexpr auto ConnClose{ Hermes::ConnectionErrorEnum::ConnectionClosed };
    if (socketView.Error().error_or(ConnClose) != ConnClose)
        return std::unexpected{ "Error receiving message" };

    return body;
}