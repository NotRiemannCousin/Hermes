#pragma once

#include <string>
#include <format>

namespace Hermes { enum class ConnectionErrorEnum; }

struct Url {
    std::string scheme;
    std::string hostname;
    std::string path;

    [[nodiscard]] std::string FormatRequest() const {
        return std::format(
            "GET /{} HTTP/1.1\r\n"
            "Accept-Encoding: identity\r\n"
            "User-Agent: Hermes/0.2\r\n"
            "Connection: close\r\n"
            "Host: {}\r\n\r\n",
            path, hostname);
    }
};

inline Url url{ "https", "api.discogs.com", "artists/4001234" };

using ExpString = std::expected<std::string, std::string>;