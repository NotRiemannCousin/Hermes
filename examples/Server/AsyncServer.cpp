// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <stdexec/execution.hpp>
#include <exec/task.hpp>

#include <string_view>
#include <string>
#include <array>
#include <print>
#include <format>
#include <expected>
#include <memory>
#include <thread>

namespace rg = std::ranges;
namespace vs = std::views;

// Handle a single client connection natively within its own execution frame
static exec::task<void> S_HandleClientAsync(std::shared_ptr<Hermes::RawTcpAsyncServer> client_ptr) {
    using namespace std::literals::string_view_literals;
    auto& client = *client_ptr;

    try {
        std::array<char, 8192> buffer{};
        std::string socketView{};

        const size_t bytesReceived{ co_await client.AsyncRecv(buffer, Hermes::RecvModeEnum::Any) };

        if (bytesReceived == 0) {
            std::println("Connection gracefully closed by peer.");
            co_return;
        }

        socketView.append(buffer.data(), bytesReceived);

        const auto headerEnd{ socketView.find("\r\n\r\n") };
        if (headerEnd == std::string::npos) {
            std::println(stderr, "Incomplete headers received.");
            co_return;
        }

        const auto requestLine{ socketView.substr(0, socketView.find("\r\n")) };
        std::println("Request Line:\n{}", requestLine);

        constexpr auto body{ "<h1>Hello Async World!</h1>"sv };
        const auto response{
            std::format(
                "HTTP/1.1 200 OK\r\n"
                "Server: Hermes/0.2 (Async)\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: {}\r\n"
                "Connection: close\r\n\r\n"
                "{}",
                body.size(), body) };

        co_await client.AsyncSend(response);
        co_await client.AsyncShutdown();
    }
    catch (const std::exception& e) {
        std::println(stderr, "Exception in client handler: {}", e.what());
    }
    catch (...) {
        std::println(stderr, "Unknown exception in client handler.");
    }
}

static exec::task<void> S_RunServerAsync(Hermes::FastIoLoop& ioLoop) {
    const Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({127, 0, 0, 1}), 8080 };
    auto listenerRes{ Hermes::RawTcpAsyncListener::ListenOne(Hermes::DefaultSocketData<>{endpoint}) };
    if (!listenerRes) {
        std::println(stderr, "Failed to bind/listen.");
        co_return;
    }

    auto listener{ std::move(*listenerRes) };
    std::println("Listening asynchronously on http://{}:{}...", listener.GetEndpoint().GetIp(), listener.GetEndpoint().GetPort());

    while (true) {
        try {
            // co_await on a sender unpacks the value or throws on error.
            // The result is the client socket object itself.
            auto client{ co_await listener.AsyncAcceptOne({ .scheduler = &ioLoop }) };
            std::println("Accepted connection from {}:{}.", client.GetEndpoint().GetIp(), client.GetEndpoint().GetPort());

            // Spawns and detaches the handler. Runs in parallel without suspending the accept loop.
            // Using a detached thread as a diagnostic step.
            std::jthread([](Hermes::RawTcpAsyncServer client_socket) {
                stdexec::sync_wait(S_HandleClientAsync(std::make_shared<Hermes::RawTcpAsyncServer>(std::move(client_socket))));
            }, std::move(client)).detach();
        }
        catch (const std::exception& e) {
            // Errors sent via set_error by co_await become throws and are caught here.
            std::println(stderr, "Exception while accepting client: {}", e.what());
        }
        catch (...) {
            std::println(stderr, "Unknown exception thrown during accept.");
        }
    }
}

int main() {
    Hermes::FastIoLoop loop{ std::thread::hardware_concurrency() };

    // This will run the server loop until an unrecoverable error occurs or the program is stopped.
    stdexec::sync_wait(S_RunServerAsync(loop));

    std::println("\n\nServer has been shut down.");

    return 0;
}