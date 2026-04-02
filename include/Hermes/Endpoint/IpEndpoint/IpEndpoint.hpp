#pragma once
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>
#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>
#include <Hermes/Utils/Hash.hpp>

#include <optional>
#include <string>


namespace Hermes {
    struct IpEndpointParams {
        IpAddress     ip;
        std::uint16_t port;

        std::strong_ordering operator<=>(const IpEndpointParams&) const = default;
    };

    struct IpEndpoint {
        IpEndpoint(const IpEndpoint&) noexcept = default;
        IpEndpoint& operator=(const IpEndpoint&) noexcept = default;

        //!  @brief Construct an IpEndpoint with a port and IP.
        //!  @param ip the IP of the endpoint.
        //!  @param port the port of the endpoint.
        explicit IpEndpoint(IpAddress ip = {}, std::uint16_t port = 0);

        //! @brief Construct an IpEndpoint from IpEndpointParams.
        static IpEndpoint Build(IpEndpointParams params) noexcept;

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
        static ConnectionResult<IpEndpoint> TryResolve(const std::string& url, const std::string& service = "",
                SocketTypeEnum socketType = SocketTypeEnum::Stream);


        [[nodiscard]] std::strong_ordering operator<=>(const IpEndpoint &) const = default;


        [[nodiscard]] IpAddress GetIp() const noexcept;
        [[nodiscard]] std::uint16_t GetPort() const noexcept;

        friend struct std::formatter<IpEndpoint>;
        friend struct std::hash<IpEndpoint>;

    private:

        IpAddress     _ip{};
        std::uint16_t _port{};
    };

    static_assert(EndpointConcept<IpEndpoint>);
}


#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.tpp>

static_assert(std::formattable<Hermes::IpEndpoint, char>);
static_assert(Hermes::Utils::Hashable<Hermes::IpEndpoint>);