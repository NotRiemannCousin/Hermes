#pragma once
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>
#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>

#include <optional>
#include <string>

using std::expected;
using std::optional;
using std::string_view;
using std::unexpected;

namespace Hermes {
    struct IpEndpoint {
        IpEndpoint(IpEndpoint&&) noexcept;
        IpEndpoint(const IpEndpoint&) noexcept;

        explicit IpEndpoint(IpAddress ip = IpAddress::Empty(), int port = 0);

        ConnectionResultOper FromSockAddr(SocketInfoAddr infoAddr);
        ConnectionResult<SocketInfoAddr> ToSockAddr() const;

        static optional<IpEndpoint> TryResolve(const string& url, const string& service = "", SocketTypeEnum socketType = SocketTypeEnum::STREAM);

        IpAddress ip;
        int port;
    };

    static_assert(EndpointConcept<IpEndpoint>);
} // namespace Hermes


template<>
struct std::formatter<Hermes::IpEndpoint> {
    using Endpoint = Hermes::IpEndpoint;

    constexpr auto parse(auto&) { }

    auto format(const Endpoint &endpoint, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "{}:{}", endpoint.ip, endpoint.port);
    }
};