// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/ClientSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
#include <algorithm>
#include <ranges>
#include <print>

namespace rg = std::ranges;
namespace vs = std::views;

std::expected<std::monostate, std::string> MakeRequest() {
    using namespace std::literals::string_view_literals;

    struct { std::string scheme; std::string hostname; std::string path; } url {
            "https", "api.discogs.com", "artists/4001234", };

#pragma region Lambdas

    const auto s_makeSocket = [&](const Hermes::IpEndpoint endpoint) {
        return Hermes::RawTlsClient::Connect(Hermes::TlsSocketData<>{ endpoint, url.hostname });
    };

    const auto s_makeRequest = [&](Hermes::RawTlsClient&& client) {
        const auto request{
            format(
                "GET /{} HTTP/1.1\r\n"
                "Accept-Encoding: identity\r\n"
                "User-Agent: Hermes/0.2\r\n"
                "Host: {}\r\n\r\n",
                url.path, url.hostname) };

        return client.Send(request).second
                .transform([client = std::move(client)](const auto) mutable { return std::move(client); });
    };

    constexpr auto s_mapError = [](const Hermes::ConnectionErrorEnum error) -> std::string {
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
    };

    const auto s_getResponse = [&](Hermes::RawTlsClient&& client) -> std::expected<std::string, std::string> {
        auto socketView{ client.RecvLazyRange<char>() };
        // `RecvLazyRange` is an input_range, so it consumes the bytes when you advance the iterator. Advancing an iterator of an
        // input_range can cause invalidation of other iterators, but the current state is stored in the range so all
        // iterators are treated equally and represent the current state.
        // This code shows how it can be useful.

        // (Do I need to give a name to this type of range? sibling{_input}_range? global_{input}_range? Idk yet).

        if (!rg::starts_with(socketView, "HTTP/1.1"sv))
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
        // You can use ` | std::views::take(maxChunkStringLength)` before UntilMatch to easely limit the size.
        // The range automatically stops when the connection ends, but be careful with this.

        if (const auto err{ socketView.Error() }; !err)
            return std::unexpected{ "Error receiving message" };

        return body;
    };

    const auto s_processData = [](std::string&& body) {
        std::println("body:\n\n{}", body);

        return std::monostate{};
    };

#pragma endregion

    return Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme)
            .and_then(s_makeSocket)
            .and_then(s_makeRequest)
            .transform_error(s_mapError)
            .and_then(s_getResponse)
            .transform(s_processData);
}




int main() {
    if (const auto res{ MakeRequest() }; !res) {
        int err{ WSAGetLastError() };


        std::println("\n\n{} : {}", res.error(), err);
    }

    return 0;
}

