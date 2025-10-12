#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi.hpp>

using std::uint16_t;
using std::uint8_t;
using std::bit_cast;

namespace Hermes {
    //----------------------------------------------------------------------------------------------------
    // IpEndpoint
    //----------------------------------------------------------------------------------------------------

    IpEndpoint::IpEndpoint(IpEndpoint&&) noexcept = default;
    IpEndpoint::IpEndpoint(const IpEndpoint&) noexcept = default;

    IpEndpoint::IpEndpoint(IpAddress ip, int port)
        : ip(ip), port(port) {}

    ConnectionResult<IpEndpoint> IpEndpoint::FromSockAddr(SocketInfoAddr infoAddr) {
        auto [addr, _, type] = infoAddr;

        if (type == AddressFamilyEnum::INET6) {
            const auto *in6 = reinterpret_cast<const sockaddr_in6*>(&addr);

            if (in6->sin6_family != _tus(AddressFamilyEnum::INET6))
                return unexpected{ ConnectionErrorEnum::UNKNOWN };

            const IpAddress address{ IpAddress::FromIPv6(bit_cast<IpAddress::IPv6Type>(in6->sin6_addr)) };
            const int port{ in6->sin6_port };

            return IpEndpoint(address, ntohs(port));
        }

        if (type == AddressFamilyEnum::INET) {
            const auto *in = reinterpret_cast<sockaddr_in*>(&addr);

            if (in->sin_family != _tus(AddressFamilyEnum::INET))
                return unexpected{ ConnectionErrorEnum::UNKNOWN };

            const IpAddress address{ IpAddress::FromIPv4(bit_cast<IpAddress::IPv4Type>(in->sin_addr)) };
            const int port{ in->sin_port };

            return IpEndpoint(address, ntohs(port));
        }

        return unexpected{ ConnectionErrorEnum::UNKNOWN };
    }

    ConnectionResult<SocketInfoAddr> IpEndpoint::ToSockAddr() const {
        if (ip.IsEmpty())
            return unexpected{ ConnectionErrorEnum::UNKNOWN }; // TODO: improve error handling

        sockaddr_storage addr{};
        size_t size{};
        AddressFamilyEnum addressFamily{};

        std::visit([&]<class T>(T &&arg) {
            using T0 = std::decay_t<T>;

            if constexpr (std::is_same_v<T0, IpAddress::IPv6Type>) {
                addressFamily = AddressFamilyEnum::INET6;

                size = sizeof(sockaddr_in6);
                auto *in6 = reinterpret_cast<sockaddr_in6*>(&addr);
                in6->sin6_family = _tus(addressFamily);
                in6->sin6_port = htons(port);
                in6->sin6_addr = bit_cast<in6_addr>(arg);
            } else if constexpr (std::is_same_v<T0, IpAddress::IPv4Type>) {
                addressFamily = AddressFamilyEnum::INET;

                size = sizeof(sockaddr_in);
                auto *in = reinterpret_cast<sockaddr_in*>(&addr);
                in->sin_family = _tus(addressFamily);
                in->sin_port   = htons(port);
                in->sin_addr   = bit_cast<in_addr>(arg);
            }
        }, ip.data);

        return SocketInfoAddr{ addr, size, addressFamily };
    }


    ConnectionResult<IpEndpoint> IpEndpoint::TryResolve(const string& url, const string& service, SocketTypeEnum socketType) {
        Network::Initialize();

        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = static_cast<int>(socketType);

        addrinfo* result_ptr = nullptr;
        if (const int err_code{ getaddrinfo(url.c_str(), service.c_str(), &hints, &result_ptr) }; err_code != 0 ) {
            switch (err_code) {
                case EAI_NONAME:
                    return std::unexpected(ConnectionErrorEnum::RESOLVE_HOST_NOT_FOUND);
                case EAI_SERVICE:
                    return std::unexpected(ConnectionErrorEnum::RESOLVE_SERVICE_NOT_FOUND);
                case EAI_AGAIN:
                    return std::unexpected(ConnectionErrorEnum::RESOLVE_TEMPORARY_FAILURE);
                default:
                    return std::unexpected(ConnectionErrorEnum::RESOLVE_FAILED);
            }
        }


        static auto addrinfo_deleter = [](addrinfo* p) { if (p) freeaddrinfo(p); };
        const std::unique_ptr<addrinfo, decltype(addrinfo_deleter)> result(result_ptr, addrinfo_deleter);

        if (!result || !result->ai_addr)
            return std::unexpected(ConnectionErrorEnum::RESOLVE_NO_ADDRESS_FOUND);

        sockaddr* addr = result->ai_addr;
        if (addr->sa_family == AF_INET6) {
            const auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr);
            return IpEndpoint{
                IpAddress::FromIPv6(std::bit_cast<IpAddress::IPv6Type>(ipv6->sin6_addr)),
                ntohs(ipv6->sin6_port)
            };
        }

        if (addr->sa_family == AF_INET) {
            const auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr);
            return IpEndpoint{
                IpAddress::FromIPv4(std::bit_cast<IpAddress::IPv4Type>(ipv4->sin_addr)),
                ntohs(ipv4->sin_port)
            };
        }

        return std::unexpected(ConnectionErrorEnum::UNSUPPORTED_ADDRESS_FAMILY);
    }

    bool IpEndpoint::operator==(const IpEndpoint &endpoint) const {
        return this->port == endpoint.port && this->ip == endpoint.ip;
    }

    IpEndpoint& IpEndpoint::operator=(const IpEndpoint& endpoint) noexcept = default;
} // namespace Hermes
