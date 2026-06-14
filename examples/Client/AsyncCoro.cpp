// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Socket/Async/AsyncClientSocket.hpp>

#include <stdexec/execution.hpp>
#include <exec/task.hpp>

#include <string_view>
#include <string>
#include <array>

#include "_base.hpp"

namespace rg = std::ranges;
namespace vs = std::views;

// C++20 Coroutines significantly simplify asynchronous programming by suspending and resuming
// the execution frame context natively. This entirely eliminates the need for shared state
// structures (like `ClientState` using `std::shared_ptr`) because local variables safely
// outlive the suspension points (`co_await`).

extern exec::task<ExpString> MakeRequestAsync(Hermes::FastIoLoop& ioLoop) try {
    using namespace std::literals::string_view_literals;

    // 1. Resolve the target endpoint.
    auto res{ Hermes::IpEndpoint::TryResolve(url.hostname, url.scheme) };
    if (!res) throw res.error();

    const auto& endpoint{ *res };

    // 2. Establish the asynchronous connection.
    // The coroutine suspends here until the TCP handshake and TLS negotiation are complete.
    auto client{ co_await Hermes::RawTlsAsyncClient::Connect(
        Hermes::TlsSocketData<>{ endpoint, url.hostname },
        {{ .recvBufferSize = 8192, .scheduler = &ioLoop }}
    ) };

    // 3. Format and send the HTTP request.
    co_await client.Send(url.FormatRequest());

    // 4. Receive the initial response chunk.
    // We read up to 8192 bytes. This usually covers the HTTP headers and the beginning of the body.
    std::array<char, 8192> chunk{};
    const size_t bytesReceived{ co_await client.Recv(chunk, Hermes::RecvModeEnum::Any) };

    if (bytesReceived == 0)
        co_return std::unexpected{ "Connection Closed" };

    std::string response{};
    response.append(chunk.data(), bytesReceived);
    const std::string& socketView{ response };

    // Validate HTTP version
    if (!socketView.starts_with("HTTP/1.1"sv))
        throw std::string{ "Non supported version" };

    const auto headerEnd{ socketView.find("\r\n\r\n") };
    if (headerEnd == std::string::npos)
        throw std::string{ "Error receiving complete message or incomplete headers" };

    std::string_view headers{ socketView.data(), headerEnd };
    std::string_view body{ socketView.data() + headerEnd + 4, socketView.size() - headerEnd - 4 };

    // Decode the chunked transfer size (hexadecimal).
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

    // Save the first chunk of the body before resizing the main response buffer.
    std::string finalBody{ body };
    finalBody.resize(bodySize);

    // 5. Receive the remainder of the payload.
    // RecvModeEnum::All ensures the socket keeps reading until the exact requested size is met.
    if (body.size() < bodySize)
        co_await client.Recv(finalBody | vs::drop(body.size()), Hermes::RecvModeEnum::All);

    // Return the successfully extracted body.
    co_return finalBody;
}
// Native try-catch blocks replace the `let_error` pipeline mapping, unwinding
// stdexec errors into standard exception scopes.
// (Yeah I really didn't like it but this is the default for stdexec).
catch (const Hermes::ConnectionErrorEnum& e) { co_return std::unexpected{ std::format("{}", e) }; }
catch (const Hermes::TransferError& e) { co_return std::unexpected{ std::format("{}", e) }; }
catch (const std::string&           s) { co_return std::unexpected{ s }; }
catch (const char*                  s) { co_return std::unexpected{ s }; }
catch (const std::exception&        e) { co_return std::unexpected{ e.what() }; }
catch (...) { co_return std::unexpected{ "unknown exception" }; }


extern ExpString MakeRequest() {
    // Synchronize and extract the value from the tuple returned by stdexec::sync_wait.

    Hermes::FastIoLoop loop{ 1 };

    auto [result]{ stdexec::sync_wait(MakeRequestAsync(loop)).value() };
    return result;
}