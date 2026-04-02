#pragma once
#include <optional>
#include <string>

namespace std {
    template<>
    struct hash<Hermes::IpEndpoint> {
        size_t operator()(const Hermes::IpEndpoint &endpoint) const noexcept {
            return std::hash<Hermes::IpAddress>{}(endpoint._ip) ^ (std::hash<int>{}(endpoint._port) << 1);
        }
    };

    template<>
    struct formatter<Hermes::IpEndpoint> {
        using Endpoint = Hermes::IpEndpoint;

        constexpr auto parse(auto& ctx) { return ctx.begin(); }

        template<class FormatContext>
        auto format(const Endpoint &endpoint, FormatContext &ctx) const {
            return std::format_to(ctx.out(), "{}:{}", endpoint._ip, endpoint._port);
        }
    };
}
