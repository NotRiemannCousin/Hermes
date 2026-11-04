// ReSharper disable All
#include <Hermes/Socket/Async/AsyncClientSocket.hpp>

#include <stdexec/execution.hpp>
#include <exec/variant_sender.hpp>

#include <string_view>
#include <string>
#include <memory>
#include <array>
#include <charconv>

#include <Hermes/Utils/Overloads.hpp>

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

    // DNS resolution is a synchronous call, so it stays out of the sender graph: on failure the function
    // simply returns at its own boundary, before any pipeline is even built.
    const auto resolvedEndpoint{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!resolvedEndpoint) return std::unexpected{ std::format("{}", resolvedEndpoint.error()) };

#pragma region Lambdas

    // One single error type flows through the whole backbone: std::string (which is also ExpString's
    // error type). Library senders complete with typed errors (ConnectionErrorEnum from Connect,
    // TransferError from Send/Recv), so `describeError` converts each one into the pipeline's error
    // channel right at the IO boundary. It returns a `just_error` sender (what let_error requires),
    // keeping failures in the error channel instead of promoting them to values mid-pipeline.
    constexpr auto describeError{ Hermes::Utils::Overloaded{
        [](const Hermes::ConnectionErrorEnum err) noexcept { return stdexec::just_error(std::format("{}", err)); },
        [](const Hermes::TransferError& err) noexcept { return stdexec::just_error(std::format("{}", err)); } } };

    const auto makeSocket{ [&](const Hermes::IpEndpoint& endpoint) noexcept {
        return Hermes::RawTlsAsyncClient::Connect(
            { endpoint, url.hostname, nullptr, &ioLoop },
            {{ .recvBufferSize = 8192 }}) | stdexec::let_error(describeError);
    } };

    const auto makeRequest{ [&](Hermes::RawTlsAsyncClient& client) noexcept {
        SharedState state{ std::make_shared<ClientState>(std::move(client)) };
        state->request = url.FormatRequest();

        const auto returnState{ [state](const size_t) noexcept { return state; } };

        // `Send` returns a Sender. We chain it using `stdexec::then` to map the resulting
        // value (bytes sent) back into our `ClientState` pointer, moving it forward down the pipeline.
        return state->client.Send(state->request) | stdexec::then(returnState) | stdexec::let_error(describeError);
    } };

    constexpr auto getResponse{ [](SharedState& state) noexcept {
        const auto returnPair{ [state](const size_t bytesReceived) noexcept {
            return std::pair{ state, bytesReceived };
        } };

        return state->client.Recv(state->chunk, Hermes::RecvModeEnum::Any)
                | stdexec::then(returnPair) | stdexec::let_error(describeError);
    } };

    constexpr auto processData{ [](const std::pair<SharedState, size_t>& data) noexcept {
        auto [state, bytesReceived]{ data };

        using RecvSender = decltype(state->client.Recv(state->response | vs::drop(0), Hermes::RecvModeEnum::All)
                                    | stdexec::let_error(describeError));
        using JustSender = decltype(stdexec::just(std::size_t{}));
        using ErrorSender = decltype(stdexec::just_error(std::string{}));

        using Sender = exec::variant_sender<JustSender, RecvSender, ErrorSender>;
        const auto retResp{ [state](std::size_t) noexcept { return state->response; } };

        // Protocol failures complete with `just_error(std::string)`, reusing the backbone's single error
        // channel instead of throwing. The variant only packs the possible completion shapes into one
        // sender type so the lambda has a single return type.
        if (bytesReceived == 0)
            return Sender{ stdexec::just_error(std::string{ "Connection Closed" }) } | stdexec::then(retResp);

        state->response.append(state->chunk.data(), bytesReceived);
        const std::string& socketView{ state->response };

        if (!socketView.starts_with("HTTP/1.1"sv))
            return Sender{ stdexec::just_error(std::string{ "Non supported version" }) } | stdexec::then(retResp);

        const auto headerEnd{ socketView.find("\r\n\r\n") };
        if (headerEnd == std::string::npos)
            return Sender{ stdexec::just_error(std::string{ "Error receiving complete message or incomplete headers" }) } | stdexec::then(retResp);

        std::string_view headers{ socketView.data(), headerEnd };
        std::string_view body{ socketView.data() + headerEnd + 4, socketView.size() - headerEnd - 4 };

        size_t bodySize{};
        const auto [ptr, err]{ std::from_chars(body.data(), body.data() + body.size(), bodySize, 16) };

        body.remove_prefix(ptr - body.data());

        if (err != std::errc{} || !body.starts_with("\r\n"))
            return Sender{ stdexec::just_error(std::string{ "Invalid body size" }) } | stdexec::then(retResp);

        body.remove_prefix(2);

        if (headers.size() < 12 || headers.substr(9, 3) != "200") {
            const auto statusCode{ headers.size() >= 12 ? headers.substr(9, 3) : "UNK" };
            return Sender{ stdexec::just_error(std::format("error code: {}", statusCode)) } | stdexec::then(retResp);
        }

        state->response = body;
        state->response.resize(bodySize);

        // Not an accurate way to implement HTTP. It's assuming a chunk based response and parsing just the first
        // chunk (needs exec::repeat_effect_until to implement it properly), but in this example it's enough.
        if (body.size() >= bodySize)
            return Sender{ stdexec::just(std::size_t{}) } | stdexec::then(retResp);

        return Sender{ state->client.Recv(state->response | vs::drop(body.size()), Hermes::RecvModeEnum::All)
                            | stdexec::let_error(describeError) }
                | stdexec::then(retResp);
    } };

    // The backbone carries exactly one error type, and it is ExpString's own error type, so the boundary
    // mapping is trivial: the received body becomes the expected value, the error message becomes the
    // unexpected one. `upon_error` moves the error completion into the value channel (it wraps plain
    // returns into `just` internally), so sync_wait always receives a value and can never throw. And
    // since every callable in the graph is noexcept, no std::exception_ptr exists anywhere in it.
    constexpr auto returnExpected{ [](std::string body) noexcept -> ExpString {
        return std::move(body);
    } };

    constexpr auto errorToExpected{ [](std::string msg) noexcept -> ExpString {
        return ExpString{ std::unexpect, std::move(msg) };
    } };

#pragma endregion


    // The execution pipeline definition. Notice how `stdexec::just()` kickstarts the lazy evaluation.
    // `let_value` is used to chain asynchronous operations, `then` is used for synchronous transformations,
    // and `upon_error` folds the single error type into the result at the function boundary.
    auto requestSender{ stdexec::just(*resolvedEndpoint)
            | stdexec::let_value(makeSocket)
            | stdexec::let_value(makeRequest)
            | stdexec::let_value(getResponse)
            | stdexec::let_value(processData)
            | stdexec::then(returnExpected)
            | stdexec::upon_error(errorToExpected) };

    // sync_wait blocks the calling thread until the entire execution graph finishes evaluating.
    auto [result]{ stdexec::sync_wait(requestSender).value() };
    return result;
}
