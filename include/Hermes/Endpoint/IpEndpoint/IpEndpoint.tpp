#pragma once
#include <optional>
#include <iostream>
#include <string>

namespace std {
    template <>
    struct hash<Hermes::IpEndpoint> {
        size_t operator()(const Hermes::IpEndpoint &endpoint) const noexcept {
            size_t result{ std::hash<Hermes::IpAddress>{}(endpoint.m_ip) };

            Hermes::Utils::HashCombine(result, std::hash<uint16_t>{}(endpoint.m_port));;

            return result;
        }
    };

    template <>
    struct formatter<Hermes::IpEndpoint> {
        using Endpoint = Hermes::IpEndpoint;

        constexpr auto parse(auto& ctx) {
            auto it{ ctx.begin() };

            if (it != ctx.end() && *it == 'f')
                ++it, m_ipv6Reduced = false;

            return it;
        }

        template<class FormatContext>
        auto format(const Endpoint &endpoint, FormatContext &ctx) const {
            if (m_ipv6Reduced)
                return std::format_to(ctx.out(), "{:b}:{}", endpoint.m_ip, endpoint.m_port);
            return std::format_to(ctx.out(), "{:fb}:{}", endpoint.m_ip, endpoint.m_port);
        }

    private:
        bool m_ipv6Reduced{ true };
    };
}

inline std::ostream& operator<<(std::ostream& os, const Hermes::IpEndpoint& ep) {
    std::format_to(std::ostreambuf_iterator{ os }, "{}", ep);
    return os;
}