#include <Hermes/Socket/Sync/ListenerSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
#include <algorithm>
#include <ranges>
#include <print>
#include <format>
#include <expected>
#include <ostream>

namespace rg = std::ranges;
namespace vs = std::views;

std::expected<std::monostate, std::string> RunServer() {
    using namespace std::literals::string_view_literals;
    using Hermes::RawTcpListener;
    using Hermes::RawTcpServer;

    const Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({127, 0, 0, 1}), 8080 };

    static constexpr auto s_log = [](std::string message) {
        return [message = std::move(message)]<class T>(T&& obj) -> T {
            std::print("{}", message);
            return std::forward<T>(obj);
        };
    };
    static constexpr auto s_sendRequest = [](RawTcpServer&& socket) -> Hermes::ConnectionResultOper {
        auto socketView{ socket.RecvStream<char>() };

        const auto requestLine{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };
        std::println("Request Line:\n{}", requestLine);

        const auto headers{ socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv) | rg::to<std::string>() };
        std::println("Headers:\n{}", headers);

        constexpr auto body{ "<h1>Hello World!</h1>"sv };
        const auto response{
            std::format(
                "HTTP/1.1 200 OK\r\n"
                "Server: Hermes/0.5\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: {}\r\n"
                "Connection: close\r\n\r\n"
                "{}",
                body.size(), body) };

        return socket.Send(response).second;
    };


    return RawTcpListener::ListenOne(Hermes::DefaultSocketData<>{ endpoint })
            .transform(s_log(std::format("Listening on http://{}:{}...", endpoint.GetIp(), endpoint.GetPort())))
            .and_then(&RawTcpListener::AcceptOneConnection)
            .transform(s_log("Accepted"))
            .and_then(s_sendRequest)
            .transform_error([](auto err){ return std::format("{:v}", err); });
}

int main() {
    if (const auto res{ RunServer() }; !res) {
        int err{ WSAGetLastError() };
        
        std::println("\n\n{} : {}", res.error(), err);
    } else {
        std::println("\n\nServer finished successfully.");
    }

    return 0;
}