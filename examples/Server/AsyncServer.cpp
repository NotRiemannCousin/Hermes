// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <exec/start_detached.hpp>
#include <stdexec/execution.hpp>
#include <exec/repeat_until.hpp>

#include <string_view>
#include <string>
#include <array>
#include <print>
#include <format>
#include <iostream>
#include <memory>
#include <thread>
#include <stdexcept>

namespace rg = std::ranges;
namespace vs = std::views;

struct ClientState {
    Hermes::RawTcpAsyncServer client;
    std::array<char, 8192> buffer{};
    std::string socketView{};
};

static auto S_HandleClientAsync(std::shared_ptr<ClientState> state) {
    using namespace std::literals::string_view_literals;

    auto s_onSendComplete = [state](const auto&...) {
        state->client.Close();
        return stdexec::just();
    };

    auto s_onRecv = [state, s_onSendComplete](const size_t bytesReceived) {
        if (bytesReceived == 0) {
            // std::println("Connection gracefully closed by peer.");
            throw std::runtime_error("Closed");
        }

        state->socketView.append(state->buffer.data(), bytesReceived);

        const auto headerEnd{ state->socketView.find("\r\n\r\n") };
        if (headerEnd == std::string::npos) {
            // std::println(stderr, "Incomplete headers received.");
            throw std::runtime_error("Incomplete");
        }

        const auto requestLine{ state->socketView.substr(0, state->socketView.find("\r\n")) };
        // std::println("Request Line:\n{}", requestLine);

        static auto response{ [] {
            constexpr auto body{ "<h1>Hello Monadic Async World!</h1>"sv };

            return std::format(
                "HTTP/1.1 200 OK\r\n"
                "Server: Hermes/0.2 (Async)\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: {}\r\n"
                "Connection: close\r\n\r\n"
                "{}", body.size(), body);
            }()
        };

        return state->client.AsyncSend(response)
             | stdexec::let_value(s_onSendComplete);
    };

    return state->client.AsyncRecv(state->buffer, Hermes::RecvModeEnum::Any)
         | stdexec::let_value(s_onRecv);
}

static void S_RunServerAsync(Hermes::FastIoLoop& ioLoop) {
    const Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({127, 0, 0, 1}), 8080 };

    static constexpr auto s_makeResponse = [](auto&& clientSocket) {
        // std::println("Accepted from: {}", clientSocket.GetEndpoint());
        // std::cout.flush();

        auto state{ std::make_shared<ClientState>(ClientState{ std::move(clientSocket) }) };

        exec::start_detached(
            S_HandleClientAsync(std::move(state))
                    | stdexec::let_error([](auto&& err) {
                        return stdexec::just();
                    })
        );
        // start_detached is fire-and-forget, we didn't wait the connection end
        // because the other threads (of FastIoLoop) will deal with this for us.
        // start_detached calls terminate() on error, so I'm removing this channel.

        return stdexec::just();
    };

    auto s_acceptConn = [&ioLoop](auto& listener) {
        // std::println("listening at: {}", listener.GetEndpoint());
        // std::cout.flush();

        return listener.AsyncAcceptOne({ .scheduler = &ioLoop })
                | stdexec::let_value(s_makeResponse)
                | exec::repeat_effect();
    };

    auto serve{ Hermes::RawTcpAsyncListener::Listen(Hermes::DefaultSocketData<>{endpoint}, { .scheduler = &ioLoop })
            | stdexec::let_value(s_acceptConn)
            | stdexec::upon_error([](auto err) {
                return 3.1416;
            }) };
    // repeat_effect leaves no value channel, so I'm redirecting the error channel
    // so that sync_wait can have areturn type.


    if (!stdexec::sync_wait(std::move(serve)))
        std::println("Failed to initialize listener or handle client.");
}

int main() {
    Hermes::FastIoLoop loop{ std::thread::hardware_concurrency() };

    S_RunServerAsync(loop);

    std::println("\n\nServer has been shut down.");

    return 0;
}