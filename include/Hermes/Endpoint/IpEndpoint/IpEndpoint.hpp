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

        //!  @brief Construct an IpEndpoint with an port and IP.
        //!  @param ip the IP of the endpoint.
        //!  @param port the port of the endpoint.
        explicit IpEndpoint(IpAddress ip = IpAddress::Empty(), int port = 0);



        //! @brief Tries to construct an Endpoint from low-level socket information.
        //! @param infoAddr The socket information used to construct the endpoint.
        //! @return An IpEndpoint on success, or an ConnectionErrorEnum on failure.
        static ConnectionResult<IpEndpoint> FromSockAddr(SocketInfoAddr infoAddr);


        //! @brief Tries to construct low-level socket information from this endpoint.
        //! @return An SocketInfoAddr on success, or an ConnectionErrorEnum on failure.
        ConnectionResult<SocketInfoAddr> ToSockAddr() const;


        //!  @brief Tries to resolve a hostname and service into an IP endpoint.
        //!  @param url The hostname or IP address string to resolve (e.g., "google.com").
        //!  @param service The service name (e.g., "http") or port number as a string (e.g., "80").
        //!  @param socketType The desired socket type for the resolution (e.g., STREAM for TCP).
        //!  @return An IpEndpoint on success, or a ConnectionErrorEnum on failure.
        static ConnectionResult<IpEndpoint> TryResolve(const string& url, const string& service = "", SocketTypeEnum socketType = SocketTypeEnum::STREAM);


        bool operator==(const IpEndpoint &) const;

        IpAddress ip{ IpAddress::Empty() };
        int port{ -1 };
    };

    static_assert(EndpointConcept<IpEndpoint>);
} // namespace Hermes


namespace std {
    template<>
    struct hash<Hermes::IpEndpoint> {
        size_t operator()(const Hermes::IpEndpoint &endpoint) const noexcept {
            return std::hash<Hermes::IpAddress>{}(endpoint.ip) ^ (std::hash<int>{}(endpoint.port) << 1);
        }
    };

    template<>
    struct formatter<Hermes::IpEndpoint> {
        using Endpoint = Hermes::IpEndpoint;

        constexpr auto parse(auto&) { }

        auto format(const Endpoint &endpoint, std::format_context &ctx) const {
            return std::format_to(ctx.out(), "{}:{}", endpoint.ip, endpoint.port);
        }
    };
}