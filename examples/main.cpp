#include <Hermes/Socket/ClientSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
#include <algorithm>
#include <ranges>
#include <print>

namespace rg = std::ranges;
namespace vs = std::views;
using namespace std::literals::string_view_literals;

// fake method
std::expected<std::monostate, string> HttpHeaders(auto sla) {
    std::println("headers:\n\n{}", sla | rg::to<string>());

    return {};
};

expected<std::monostate, string> MakeRequest() {
    struct {
        std::string scheme;
        std::string hostname;
        std::string path;
    } url {
        // "https",
        // "api.jikan.moe",
        // "v4/anime/57555"
        "https",
        "api.discogs.com",
        "artists/4001234",
    };

    auto endpoint{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!endpoint)
        return std::unexpected{ "Could not resolve endpoint" };

    auto socket{ Hermes::RawTlsClient::Connect(Hermes::TlsSocketData<>{ *endpoint, url.hostname }) };
    // endpoint + host = minimal socket data to
    if (!socket)
        return std::unexpected{ "Could not connect to endpoint" };

    const auto request{
        format(
            "GET /{} HTTP/1.1\r\n"
            "Accept-Encoding: identity\r\n"
            "User-Agent: Hermes/0.2\r\n"
            "Host: {}\r\n\r\n",
            url.path, url.hostname) };



    if (const auto err{ socket->Send(request) }; !err.second)
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

    auto chunkLength{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>() };

    const auto body{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>() };
    // const auto body{ socketView | rg::to<string>() };
    // The range automatically stops when the connection ends, but be careful with this.

    std::println("body:\n\n{}", body);

    if (const auto err{ socketView.Error() }; !err)
        return std::unexpected{ "Error receiving message" };

    return {};
}




int main() {
    if (const auto res{ MakeRequest() }; !res) {
        int err{ WSAGetLastError() };


        std::println("\n\n{} : {}", res.error(), err);
    }

    return 0;
}

