#pragma once
#include <optional>
#include <iostream>
#include <string>

namespace std {
    template <>
    struct hash<Hermes::IpEndpoint> {
        size_t operator()(const Hermes::IpEndpoint &endpoint) const noexcept {
            size_t result{ std::hash<Hermes::IpAddress>{}(endpoint._ip) };

            Hermes::Utils::HashCombine(result, std::hash<uint16_t>{}(endpoint._port));;

            return result;
        }
    };

    template <>
    struct formatter<Hermes::IpEndpoint> {
        using Endpoint = Hermes::IpEndpoint;

        constexpr auto parse(auto& ctx) {
            auto it{ ctx.begin() };

            if (it != ctx.end() && *it == 'f')
                ++it, _ipv6Reduced = false;

            return it;
        }

        template<class FormatContext>
        auto format(const Endpoint &endpoint, FormatContext &ctx) const {
            if (_ipv6Reduced)
                return std::format_to(ctx.out(), "{:b}:{}", endpoint._ip, endpoint._port);
            return std::format_to(ctx.out(), "{:fb}:{}", endpoint._ip, endpoint._port);
        }

    private:
        bool _ipv6Reduced{ true };
    };
}

inline std::ostream& operator<<(std::ostream& os, const Hermes::IpEndpoint& ep) {
    std::format_to(std::ostreambuf_iterator{ os }, "{}", ep);
    return os;
}