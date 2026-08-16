#include <expected>
#include <print>

#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include "_base.hpp"

ExpString MakeRequest();

int main() {
    constexpr auto printContent{ [](std::string&& content) {
        std::println("body:\n\n{}", content);
        return std::monostate{};
    } };

    constexpr auto printError{ [](std::string&& error){
        // const int err{ WSAGetLastError() };
        const int err{ errno };

        std::println("Request Failed with '{}'\n\nWSA Code : {}", error, err);
        return std::monostate{};
    } };

    std::ignore = MakeRequest()
            .transform(printContent)
            .transform_error(printError);

    return 0;
}