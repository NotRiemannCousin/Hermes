// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>

#include <exec/start_detached.hpp>
#include <stdexec/execution.hpp>
#include <exec/repeat_until.hpp>
#include <exec/variant_sender.hpp>

#include <string_view>
#include <string>
#include <array>
#include <charconv>
#include <print>
#include <format>
#include <iostream>
#include <memory>
#include <thread>

struct ClientState {
    Hermes::RawTcpAsyncServer client;
    std::array<char, 8192> buffer{};
    std::string socketView{};
    size_t bodyIdx{};
    bool keepAlive{ true };
};

static auto HandleClientAsync(std::shared_ptr<ClientState> state) {
    using namespace std::literals::string_view_literals;

    auto onComplete{ [state](const size_t) noexcept {
        state->socketView.clear();

        return stdexec::just(!state->keepAlive);
    } };

    auto extractHeaders{ [state] noexcept {
        using VariantSender = exec::variant_sender<
            decltype(state->client.Recv(state->socketView
                    | std::views::drop(0), Hermes::RecvModeEnum::All)),
            decltype(stdexec::just())
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
    } };

    auto requestMoreIfNeeded{ [](auto...) noexcept {
        return stdexec::just();
    } };

    auto sendResponse{ [state] noexcept {

        static auto response{ [] {
            constexpr auto body{ "<h1>Hello Monadic Async World!</h1>"sv };

            return std::format(
                "HTTP/1.1 200 OK\r\n"
                "Server: Hermes/0.6 (Async)\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: {}\r\n"
                "Connection: keep-alive\r\n\r\n"
                "{}", body.size(), body);
            }()
        };

        return state->client.Send(response);
    } };

    auto appendReadedBytes{ [state](const size_t count) noexcept {
        state->socketView.append(state->buffer.data(), count);

        return stdexec::just(state->socketView.contains("\r\n\r\n"));
    } };

    auto processOneRequest{ stdexec::just()
                | stdexec::let_value([state, appendReadedBytes]() noexcept {
                      return state->client.Recv(state->buffer, Hermes::RecvModeEnum::Any)
                           | stdexec::let_value(appendReadedBytes);
                  })
                | exec::repeat_until()
                | stdexec::let_value(extractHeaders)
                | stdexec::let_value(requestMoreIfNeeded)
                | stdexec::let_value(sendResponse)
                | stdexec::let_value(onComplete) };

    return processOneRequest | exec::repeat_until();
}

static void RunServerAsync(Hermes::FastIoLoop& ioLoop) {
    const Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({ 127, 0, 0, 1 }), 8080 };

    static constexpr auto makeResponse{ [](Hermes::RawTcpAsyncServer& clientSocket) noexcept {
        std::println("Accepted from: {}", clientSocket.GetEndpoint());
        std::cout.flush();

        auto state{ std::make_shared<ClientState>(ClientState{ std::move(clientSocket) }) };

        // The error channel of the whole client handler carries a single type: TransferError from
        // Recv/Send. Every callable in the graph is noexcept, so no std::exception_ptr ever reaches it;
        // the handler just reports the concrete error (with the partial transfer info) and detaches.
        exec::start_detached(
            HandleClientAsync(state)
                    | stdexec::upon_error([](const Hermes::TransferError& err) noexcept {
                          std::println("Client connection failed: {} ({} byte(s) transferred)",
                                       err.error, err.bytesTransferred);
                      }));

        return stdexec::just();
    } };

    auto acceptConn{ [](Hermes::RawTcpAsyncListener& listener) noexcept {
        std::println("listening at: {}", listener.GetEndpoint());
        std::cout.flush();

        return stdexec::just()
                | stdexec::let_value([&listener]() noexcept {
                      return listener.AsyncAcceptOne()
                               | stdexec::let_value(makeResponse);
                })
                | exec::repeat();
    } };

    Hermes::DefaultSocketData<Hermes::FastIoLoop> listenData{ endpoint };
    listenData.executionContext = &ioLoop;


    auto serve{ Hermes::RawTcpAsyncListener::Listen(std::move(listenData))
            | stdexec::let_value(acceptConn)
            | stdexec::upon_error([](const Hermes::ConnectionErrorEnum err) noexcept {
                  std::println("Listener terminated: {}", err);
              }) };

    if (!stdexec::sync_wait(std::move(serve)))
        std::println("Listener stopped.");
}

int main() {
    Hermes::FastIoLoop loop{ std::thread::hardware_concurrency() };

    RunServerAsync(loop);

    std::println("\n\nServer has been shut down.");

    return 0;
}
