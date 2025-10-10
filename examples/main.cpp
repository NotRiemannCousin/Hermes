#include <functional>
#include <Hermes/Socket/Tcp/TcpClientSocket.hpp>

#include <string_view>
#include <print>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
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
    } url{
        "http",
        "api.discogs.com",
        "artists/4001234",
    };

    auto endpoint{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!endpoint)
        return std::unexpected{ "Could not resolve endpoint" };

    // auto socket{ Hermes::TcpClientSocket::Connect(*endpoint) };
    // const auto request{
    //     format(
    //         "GET /{} HTTP/1.1\r\n"
    //         "Host: {}\r\n\r\n",
    //         url.path, url.hostname) };
    //
    //
    // if (!socket)
    //     return std::unexpected{ "Could not connect to endpoint" };
    //
    // if (const auto err{ socket->SendStr(request) }; !err)
    //     return std::unexpected{ "Could not send to endpoint" };
    //
    // auto socketView{ socket->ReceiveStr() };
    //
    //
    // if (!rg::starts_with(socketView, "HTTP/1.1 "sv))
    //     return std::unexpected{ "Non supported version" };
    //
    //
    // const auto statusCode{ Hermes::Utils::CopyTo<std::array<char, 3>>(socketView) };
    // const auto statusMessage{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<string>() };
    //
    // const auto headers{ HttpHeaders(socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv)) };
    //
    // if (!headers.has_value())
    //     return std::unexpected{ headers.error() };
    //
    // // if (const auto err{ socketaView.OptError() }; !err)
    //     return std::unexpected{ "Error receiving message" };
    //
    //
    // return {};
}




int main() {
    if (const auto res{ MakeRequest() }; !res)
        std::println("\n\n{}", res.error());

    Hermes::DefaultConnectPolicy<> sla;
    return 0;
}

