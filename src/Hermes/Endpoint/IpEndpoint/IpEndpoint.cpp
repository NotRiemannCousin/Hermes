#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinAPI.hpp>

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

    ConnectionResultOper IpEndpoint::FromSockAddr(SocketInfoAddr infoAddr) {
        auto [addr, _, type] = infoAddr;

        if (type == AddressFamilyEnum::INET6) {
            const auto *in6 = reinterpret_cast<sockaddr_in6*>(&addr);

            if (in6->sin6_family != _tus(AddressFamilyEnum::INET6))
                return unexpected{ ConnectionErrorEnum::UNKNOWN };

            ip = IpAddress::FromIPv6(bit_cast<IpAddress::IPv6Type>(in6->sin6_addr));

            return std::monostate{};
        }

        if (type == AddressFamilyEnum::INET) {
            const auto *in = reinterpret_cast<sockaddr_in*>(&addr);

            if (in->sin_family != _tus(AddressFamilyEnum::INET6))
                return unexpected{ ConnectionErrorEnum::UNKNOWN };

            ip = IpAddress::FromIPv4(bit_cast<IpAddress::IPv4Type>(in->sin_addr));

            return std::monostate{};
        }

        return unexpected{ ConnectionErrorEnum::UNKNOWN };
    }

    ConnectionResult<SocketInfoAddr> IpEndpoint::ToSockAddr() const {
        if (ip.IsEmpty())
            return unexpected{ ConnectionErrorEnum::UNKNOWN }; // TODO: improve error handling

        sockaddr addr{};
        size_t size{};
        AddressFamilyEnum addressFamily{};

        std::visit([&]<typename T>(T &&arg) {
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

    optional<IpEndpoint> IpEndpoint::TryResolve(const string& url, const string& service, SocketTypeEnum socketType) {
        Network::Initialize();

        const addrinfo hints = {
            .ai_family = static_cast<int>(AddressFamilyEnum::UNSPEC),
            .ai_socktype = (int) socketType
        };

        addrinfo *result = nullptr;
        if (int err = getaddrinfo(string(url).c_str(), service.c_str(), &hints, &result); err != 0)
            return std::nullopt;

        if (result == nullptr)
            return std::nullopt;

        auto addr = result->ai_addr;
        if (addr == nullptr)
            return std::nullopt;

        if (addr->sa_family == (int)AddressFamilyEnum::INET6) {
            auto ipv6 = reinterpret_cast<sockaddr_in6 *>(addr);
            return IpEndpoint{
                IpAddress::FromIPv6(std::bit_cast<IpAddress::IPv6Type>(ipv6->sin6_addr)),
                ntohs(ipv6->sin6_port)
            };
        }

        if (addr->sa_family == (int)AddressFamilyEnum::INET) {
            auto ipv4 = reinterpret_cast<sockaddr_in *>(addr);
            return IpEndpoint{
                IpAddress::FromIPv4(std::bit_cast<IpAddress::IPv4Type>(ipv4->sin_addr)),
                ntohs(ipv4->sin_port)
            };
        }

        return std::nullopt;
    }
} // namespace Hermes
