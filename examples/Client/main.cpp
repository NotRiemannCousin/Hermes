#include <expected>
#include <print>

#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include "_base.hpp"

std::expected<std::string, std::string> MakeRequest();

std::string MapHermesError(const Hermes::ConnectionErrorEnum error) {
    using Error = Hermes::ConnectionErrorEnum;
    switch (error) {
        case Error::ResolveHostNotFound:
        case Error::ResolveServiceNotFound:
        case Error::ResolveTemporaryFailure:
        case Error::ResolveFailed:
        case Error::ResolveNoAddressFound:
        case Error::UnsupportedAddressFamily:
            return "Could not resolve endpoint";

        case Error::HandshakeFailed:
        case Error::ConnectionFailed:
        case Error::CertificateError:
        case Error::Unknown:
            return "Could not connect to endpoint";

        case Error::ConnectionClosed:
        case Error::ReceiveFailed:
        case Error::DecryptionFailed:
        case Error::SendFailed:
            return "Could not send to endpoint";
        default:
            return "unknown error";
    }
}

int main() {
    constexpr auto s_printContent{ [](std::string&& content) {
        std::println("body:\n\n{}", content);
        return std::monostate{};
    } };

    constexpr auto s_printError{ [](std::string&& error){
        const int err{ WSAGetLastError() };

        std::println("Request Failed with '{}'\n\nWSA Code : {}", error, err);
        return std::monostate{};
    } };

    std::ignore = MakeRequest()
            .transform(s_printContent)
            .transform_error(s_printError);

    return 0;
}