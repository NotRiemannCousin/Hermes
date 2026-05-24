// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <exec/start_detached.hpp>
#include <stdexec/execution.hpp>
#include <exec/repeat_until.hpp>
#include <exec/variant_sender.hpp>

#include <string_view>
#include <string>
#include <array>
#include <print>
#include <format>
#include <iostream>
#include <memory>
#include <thread>

namespace rg = std::ranges;
namespace vs = std::views;

struct ClientState {
    Hermes::RawTcpAsyncServer client;
    std::array<char, 8192> buffer{};
    std::string socketView{};
    size_t bodyIdx{};
    bool keepAlive{ true };
};

static auto S_HandleClientAsync(std::shared_ptr<ClientState> state) {
    using namespace std::literals::string_view_literals;

    auto s_onComplete = [state](const auto&...) {
        state->socketView.clear();

        return stdexec::just(!state->keepAlive);
    };

    auto s_extractHeaders = [state] {
        using VariantSender = exec::variant_sender<
            decltype(state->client.Recv(state->socketView
                    | std::views::drop(0), Hermes::RecvModeEnum::All)),
            decltype(stdexec::just()),
            decltype(stdexec::just_error(Hermes::ConnectionErrorEnum{}))
        >;

        constexpr std::string_view clKey{ "content-length: " };
        constexpr std::string_view closeKey{ "connection: close" };
        constexpr std::string_view endKey{ "\r\n\r\n" };

        auto& socketView{ state->socketView };

        const auto headerLimitIdx{ socketView.find(endKey) };
        std::string_view headersStr{ socketView.data(), headerLimitIdx };

        const auto contentSizeIdx{ headersStr.find(clKey) };

        state->keepAlive = !socketView.contains(closeKey);
        state->bodyIdx   = headerLimitIdx + endKey.size();

        if (contentSizeIdx == std::string::npos)
            return VariantSender{ stdexec::just() };

        headersStr.remove_prefix(contentSizeIdx + clKey.size());
        size_t contentLength{ 67 };
        std::from_chars(headersStr.data(), headersStr.data() + headersStr.size(), contentLength);

        auto lastSize{ socketView.size() };
        socketView.resize(contentLength);

        assert(lastSize - state->bodyIdx <= contentLength);

        auto requestMore{ state->client.Recv(state->socketView
                | std::views::drop(lastSize), Hermes::RecvModeEnum::All) };


        if (lastSize - state->bodyIdx == contentLength)
            return VariantSender{ stdexec::just() };
        return VariantSender{ requestMore };
    };

    auto s_requestMoreIfNeeded = [](auto...){
        return stdexec::just();
    };

    auto s_sendResponse = [state]{

        static auto response{ [] {
            constexpr auto body{ "<h1>Hello Monadic Async World!</h1>"sv };

            return std::format(
                "HTTP/1.1 200 OK\r\n"
                "Server: Hermes/0.2 (Async)\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: {}\r\n"
                "Connection: keep-alive\r\n\r\n"
                "{}", body.size(), body);
            }()
        };

        return state->client.Send(response);
    };

    auto s_appendReadedBytes = [state](const size_t count) {
        state->socketView.append_range(state->buffer | std::views::take(count));

        return stdexec::just(state->socketView.contains("\r\n\r\n"));
    };

    return state->client.Recv(state->buffer, Hermes::RecvModeEnum::Any)
            | stdexec::let_value(s_appendReadedBytes)
            | exec::repeat_until()
            | stdexec::let_value(s_extractHeaders)
            | stdexec::let_value(s_requestMoreIfNeeded)
            | stdexec::let_value(s_sendResponse)
            | stdexec::let_value(s_onComplete)
            | exec::repeat_until();
}

static void S_RunServerAsync(Hermes::FastIoLoop& ioLoop) {
    const Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({127, 0, 0, 1}), 8080 };

    static constexpr auto s_makeResponse = [](auto&& clientSocket) {
        std::println("Accepted from: {}", clientSocket.GetEndpoint());
        std::cout.flush();

        auto state{ std::make_shared<ClientState>(ClientState{ std::move(clientSocket) }) };

        exec::start_detached(
            S_HandleClientAsync(state)
                    | stdexec::let_error([](auto&& err) { return stdexec::just(); })
        );
        // start_detached is fire-and-forget, we didn't wait the connection end
        // because the other threads (of FastIoLoop) will deal with this for us.
        // start_detached calls terminate() on error, so I'm removing this channel.

        return stdexec::just();
    };

    auto s_acceptConn = [&ioLoop](auto& listener) {
        std::println("listening at: {}", listener.GetEndpoint());
        std::cout.flush();

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