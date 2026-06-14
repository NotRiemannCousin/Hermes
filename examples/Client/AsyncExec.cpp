// ReSharper disable All
#include <Hermes/Socket/Async/AsyncClientSocket.hpp>

#include <stdexec/execution.hpp>
#include <exec/variant_sender.hpp>

#include <string_view>
#include <string>
#include <memory>
#include <array>
#include <charconv>

#include "_base.hpp"

namespace rg = std::ranges;
namespace vs = std::views;

namespace {
    // When using purely functional asynchronous pipelines (Senders/Receivers), we cannot rely on local
    // variables to survive across asynchronous boundaries (`let_value`, `then`).
    // Therefore, we allocate a state block on the heap (`ClientState`) using `std::shared_ptr` to
    // maintain the socket client and buffers alive throughout the entire pipeline lifetime.
    struct ClientState {
        Hermes::RawTlsAsyncClient client;
        std::string request{};
        std::string response{};
        std::array<char, 8192> chunk{};

        explicit ClientState(Hermes::RawTlsAsyncClient&& c) : client{ std::move(c) } {}
    };
}

extern ExpString MakeRequest() {
    using namespace std::literals::string_view_literals;
    using SharedState = std::shared_ptr<ClientState>;

    Hermes::FastIoLoop ioLoop{ 1 };

#pragma region Lambdas

    const auto s_resolveEndpoint{ [hostname{ url.hostname }, scheme{ url.scheme }]() {
        auto res{ Hermes::IpEndpoint::TryResolve(hostname, scheme) };
        if (!res) throw res.error();
        return *res;
    } };

    const auto s_makeSocket{ [&](const Hermes::IpEndpoint& endpoint) {
        return Hermes::RawTlsAsyncClient::Connect(Hermes::TlsSocketData<>{ endpoint, url.hostname },
            {{
                .recvBufferSize = 8192,
                .scheduler = &ioLoop
            }});
    } };

    const auto s_makeRequest{ [&](Hermes::RawTlsAsyncClient& client) {
        SharedState state{ std::make_shared<ClientState>(std::move(client)) };
        state->request = url.FormatRequest();

        const auto s_returnState{ [state](size_t) { return state; } };

        // `Send` returns a Sender. We chain it using `stdexec::then` to map the resulting
        // value (bytes sent) back into our `ClientState` pointer, moving it forward down the pipeline.
        return state->client.Send(state->request) | stdexec::then(s_returnState);
    } };

    constexpr auto s_getResponse{ [](SharedState& state) {
        const auto s_returnPair{ [state](size_t bytesReceived) {
            return std::pair{ state, bytesReceived };
        } };

        return state->client.Recv(state->chunk, Hermes::RecvModeEnum::Any) | stdexec::then(s_returnPair);
    } };

    constexpr auto s_processData{ [](const std::pair<SharedState, size_t>& data) {
        auto [state, bytesReceived]{ data };

        if (bytesReceived == 0)
            throw std::string{ "Connection Closed" };

        state->response.append(state->chunk.data(), bytesReceived);
        const std::string& socketView{ state->response };

        if (!socketView.starts_with("HTTP/1.1"sv))
            throw std::string{ "Non supported version" };

        const auto headerEnd{ socketView.find("\r\n\r\n") };
        if (headerEnd == std::string::npos)
            throw std::string{ "Error receiving complete message or incomplete headers" };

        std::string_view headers{ socketView.data(), headerEnd };
        std::string_view body{ socketView.data() + headerEnd + 4, socketView.size() - headerEnd - 4 };

        size_t bodySize{};
        const auto [ptr, err]{ std::from_chars(body.data(), body.data() + body.size(), bodySize, 16) };

        body.remove_prefix(ptr - body.data());

        if (err != std::errc{} || !body.starts_with("\r\n"))
            throw std::string{ "Invalid body size" };

        body.remove_prefix(2);

        if (headers.size() < 12 || headers.substr(9, 3) != "200") {
            const auto statusCode{ headers.size() >= 12 ? headers.substr(9, 3) : "UNK" };
            throw std::format("error code: {}", statusCode);
        }

        using RecvSender = decltype(state->client.Recv(state->response | vs::drop(0), Hermes::RecvModeEnum::All));
        using JustSender = decltype(stdexec::just(std::size_t{}));

        using Sender = exec::variant_sender<JustSender, RecvSender>;
        const auto s_retResp{ [state](std::size_t) { return state->response; } };

        state->response = body;
        state->response.resize(bodySize);

        // Not an accurate way to implement HTTP. It's assuming a chunk based response and parsing just the first
        // chunk (needs exec::repeat_effect to implement it properly), but in this example it's enough.
        if (body.size() >= bodySize)
            return Sender{ stdexec::just(std::size_t{}) } | stdexec::then(s_retResp);

        return Sender{ state->client.Recv(state->response | vs::drop(body.size()), Hermes::RecvModeEnum::All) }
                | stdexec::then(s_retResp);
    } };

    constexpr auto s_mapErrorPipeline{ []<typename T>(T&& error) {
        std::string errStr{};
        using ErrorType = std::remove_cvref_t<T>;

        if constexpr (std::same_as<ErrorType, std::exception_ptr>) {
            try { std::rethrow_exception(error); }
            catch (Hermes::ConnectionErrorEnum  e) { errStr = std::format("{}", e); }
            catch (const std::exception&        e) { errStr = e.what(); }
            catch (const std::string&           s) { errStr = s; }
            catch (const char*                  s) { errStr = s; }
            catch (...) { errStr = "unknown exception"; }
        }
        else if constexpr (std::formattable<ErrorType, char>)
            errStr = std::format("{}", error);
        else
            errStr = "unknown error type";

        return stdexec::just(ExpString{ std::unexpect, std::move(errStr) });
    } };

    constexpr auto s_returnExpected{ [](std::string val) -> ExpString {
        return val;
    } };

#pragma endregion


    // The execution pipeline definition. Notice how `stdexec::just()` kickstarts the lazy evaluation.
    // `let_value` is used to chain asynchronous operations, while `then` is used for synchronous transformations.
    auto requestSender{ stdexec::just()
            | stdexec::then(s_resolveEndpoint)
            | stdexec::let_value(s_makeSocket)
            | stdexec::let_value(s_makeRequest)
            | stdexec::let_value(s_getResponse)
            | stdexec::let_value(s_processData)
            | stdexec::then(s_returnExpected)
            | stdexec::let_error(s_mapErrorPipeline) };

    // sync_wait blocks the calling thread until the entire execution graph finishes evaluating.
    auto [result]{ stdexec::sync_wait(requestSender).value() };
    return result;
}