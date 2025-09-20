#include <Hermes/Socket/Tcp/TcpClientSocket.hpp>

#include <string_view>
#include <print>

int main() {
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
        return 1;

    auto socket{ Hermes::TcpClientSocket::Connect(*endpoint) };
    const auto request{
        format(
            "GET /{} HTTP/1.1\r\n"
            "Host: {}\r\n\r\n",
            url.path, url.hostname) };


    if (!socket)
        return 1;

    if (const auto err{ socket->SendStr(request) }; !err)
        return 1;


    for (const auto message : socket->ReceiveStr()) {
        if (!message || !message->starts_with("HTTP/1.1"))
            return 1;

        std::print("{}", *message);

        auto sla = message->substr(61000);

        break; // Don't do this, check manually for Content-Length/"\r\n\r\n" or use Thoth
    }

    return 0;
}