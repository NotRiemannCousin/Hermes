#include <expected>
#include <print>

#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include "_base.hpp"

ExpString MakeRequest();

int main() {
    constexpr auto getError{ [](std::string&& error) {
        const int err{ Hermes::GetError().error() };

        return ExpString{ std::format("Request Failed with '{}'\n\nWSA Code : {}", error, err) };
    } };

    std::println("{}", MakeRequest().or_else(getError).value());

    return 0;
}