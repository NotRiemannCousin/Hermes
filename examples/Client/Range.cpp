// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
#include <algorithm>
#include <ranges>

#include "_base.hpp"

namespace rg = std::ranges;
namespace vs = std::views;

// This paradigm fully utilizes C++23's monadic operations for `std::expected`
// (`and_then`, `transform`, `transform_error`) combined with lazy evaluated Ranges.
// It creates a declarative, zero-overhead execution flow where the "happy path"
// runs sequentially, and any error elegantly short-circuits to the end.

extern ExpString MakeRequest() {
    using namespace std::literals::string_view_literals;

#pragma region Lambdas

    const auto s_makeSocket{ [&](const Hermes::IpEndpoint endpoint) {
        return Hermes::RawTlsClient::Connect(Hermes::TlsSocketData<>{ endpoint, url.hostname });
    } };

    const auto s_makeRequest{ [&](Hermes::RawTlsClient&& client) {
        // `client.Send` returns a pair: { bytes_sent, expected_error }.
        // We evaluate the expected result, and if successful, we return the client back
        // down the pipeline using `transform`.
        auto val{ client.Send(url.FormatRequest()) };

        auto s_returnClient{ [client{ std::move(client) }](const auto) mutable {
            return std::move(client);
        } };
        return val.second.transform(s_returnClient);
    } };

    const auto s_getResponse{ [&](Hermes::RawTlsClient&& client) -> ExpString {
        auto socketView{ client.RecvStream<char>() };
        // `RecvStream` is an input_range, so it consumes the bytes when you advance the iterator.
        // Advancing an iterator of an input_range can cause invalidation of other iterators, but
        // the current state is stored in the range so all iterators are treated equally and
        // represent the current state.
        // This code shows how it can be useful.

        // (It's not a normal input_range, do I need to give a name to this type of range?
        // sibling{_input}_range? global_{input}_range? Idk).

        if (!rg::starts_with(socketView, "HTTP/1.1"sv))
            return std::unexpected{ "Non supported version" };

        const auto statusCode{ Hermes::Utils::ExtractTo<std::array<char, 5>>(socketView) };

        if (!rg::equal(statusCode, " 200 "sv))
            return std::unexpected{ std::format("error code: {:s}", statusCode) };

        const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };
        // This range must receive more bytes just when reading with the * operator. receiving when
        // advancing isn't that good because if your protocol uses a terminated sequence you will
        // need more work to stop at the last byte (think about this like vec.end() being outside
        // of the boundaries of the vector itself).

        // e.g.: `UntilMatch("\r\n"sv)` goes until the first occurrence of "\r\n", discard the pattern
        // (exclusive match, `UntilMatch<true>` is inclusive) and advance the state again to stop at
        // the next byte of EOS.

        const auto headers { socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv) | rg::to<std::string>() };
        const auto chunkLen{ socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };
        const auto body    { socketView | Hermes::Utils::UntilMatch("\r\n"sv)     | rg::to<std::string>() };

        // Ok, this is unsafe because I'm being lazy here, but you can process and check this data properly.
        // You can use ` | std::views::take(maxChunkStringLength)` before UntilMatch to easily limit the size.
        // The range automatically stops when the connection ends, so you don't need to worry.

        static constexpr auto ConnClose{ Hermes::ConnectionErrorEnum::ConnectionClosed };
        if (socketView.Error().error_or(ConnClose) != ConnClose)
            return std::unexpected{ "Error receiving message" };

        return body;
    } };

#pragma endregion

    return Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme)
            .and_then(s_makeSocket)
            .and_then(s_makeRequest)
            .transform_error([](const auto e){ return std::format("{}", e); })
            .and_then(s_getResponse);
}