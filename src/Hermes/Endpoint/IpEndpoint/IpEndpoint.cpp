// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/WinApi.hpp>

#include "Hermes/Utils/Overloads.hpp"

using std::uint16_t;
using std::uint8_t;
using std::bit_cast;

using std::string;

namespace Hermes {
    //----------------------------------------------------------------------------------------------------
    // IpEndpoint
    //----------------------------------------------------------------------------------------------------

    IpEndpoint::IpEndpoint(const IpAddress ip, const std::uint16_t port)
        : _ip{ ip }, _port{ port } {}

    IpEndpoint IpEndpoint::Build(IpEndpointParams params) noexcept {
        return IpEndpoint{ params.ip, params.port };
    }

    ConnectionResult<IpEndpoint> IpEndpoint::FromSockAddr(SocketInfoAddr infoAddr) {
        auto [addr, _, type]{ infoAddr };

        if (type == AddressFamilyEnum::Inet6) {
            const auto *in6{ reinterpret_cast<const sockaddr_in6*>(&addr) };

            if (in6->sin6_family != AddressFamilyEnum::Inet6)
                return std::unexpected{ ConnectionErrorEnum::Unknown };

            const IpAddress address{ IpAddress::FromIpv6(bit_cast<IpAddress::Ipv6Type>(in6->sin6_addr)) };
            const int port{ in6->sin6_port };

            return IpEndpoint(address, ntohs(port));
        }

        if (type == AddressFamilyEnum::Inet) {
            const auto *in{ reinterpret_cast<sockaddr_in*>(&addr) };

            if (in->sin_family != AddressFamilyEnum::Inet)
                return std::unexpected{ ConnectionErrorEnum::Unknown };

            const IpAddress address{ IpAddress::FromIpv4(bit_cast<IpAddress::Ipv4Type>(in->sin_addr)) };
            const int port{ in->sin_port };

            return IpEndpoint(address, ntohs(port));
        }

        return std::unexpected{ ConnectionErrorEnum::Unknown };
    }

    ConnectionResult<SocketInfoAddr> IpEndpoint::ToSockAddr() const {
        sockaddr_storage addr{};
        size_t size{};
        AddressFamilyEnum addressFamily{};

        std::visit(Utils::Overloaded{
            [&](const IpAddress::Ipv4Type arg) {
                addressFamily = AddressFamilyEnum::Inet;

                size = sizeof(sockaddr_in);
                auto *in = reinterpret_cast<sockaddr_in*>(&addr);
                in->sin_family = _tus(addressFamily);
                in->sin_port   = htons(_port);
                in->sin_addr   = bit_cast<in_addr>(arg);
            },
            [&](const IpAddress::Ipv6Type arg) {
                addressFamily = AddressFamilyEnum::Inet6;

                size = sizeof(sockaddr_in6);
                auto *in6 = reinterpret_cast<sockaddr_in6*>(&addr);
                in6->sin6_family = _tus(addressFamily);
                in6->sin6_port   = htons(_port);
                in6->sin6_addr   = bit_cast<in6_addr>(arg);
            }
        }, _ip.GetIp());

        return SocketInfoAddr{ addr, size, addressFamily };
    }


    ConnectionResult<IpEndpoint> IpEndpoint::TryResolve(const string& url, const string& service, SocketTypeEnum socketType) {
        Network::Initialize();

        const addrinfo hints{
            .ai_family   = static_cast<int>(AddressFamilyEnum::Unspec),
            .ai_socktype = static_cast<int>(socketType),
        };

        addrinfo* resultPtr{};
        if (const int errCode{ getaddrinfo(url.c_str(), service.c_str(), &hints, &resultPtr) }; errCode) {
            switch (errCode) {
                case EAI_NONAME:  return std::unexpected{ ConnectionErrorEnum::ResolveHostNotFound };
                case EAI_SERVICE: return std::unexpected{ ConnectionErrorEnum::ResolveServiceNotFound };
                case EAI_AGAIN:   return std::unexpected{ ConnectionErrorEnum::ResolveTemporaryFailure };
                default:          return std::unexpected{ ConnectionErrorEnum::ResolveFailed };
            }
        }


        static constexpr auto s_addrInfoDeleter = [](addrinfo* p) { if (p) freeaddrinfo(p); };
        const std::unique_ptr<addrinfo, decltype(s_addrInfoDeleter)> result{ resultPtr, s_addrInfoDeleter };

        if (!result || !result->ai_addr)
            return std::unexpected{ ConnectionErrorEnum::ResolveNoAddressFound };

        sockaddr* addr{ result->ai_addr };
        if (addr->sa_family == AF_INET6) {
            const auto* ipv6{ reinterpret_cast<sockaddr_in6*>(addr) };
            return IpEndpoint{
                IpAddress::FromIpv6(std::bit_cast<IpAddress::Ipv6Type>(ipv6->sin6_addr)),
                ntohs(ipv6->sin6_port)
            };
        }

        if (addr->sa_family == AF_INET) {
            const auto* ipv4{ reinterpret_cast<sockaddr_in*>(addr) };
            return IpEndpoint{
                IpAddress::FromIpv4(std::bit_cast<IpAddress::Ipv4Type>(ipv4->sin_addr)),
                ntohs(ipv4->sin_port)
            };
        }

        return std::unexpected{ ConnectionErrorEnum::UnsupportedAddressFamily };
    }

    IpAddress IpEndpoint::GetIp() const noexcept {
        return _ip;
    }

    std::uint16_t IpEndpoint::GetPort() const noexcept {
        return _port;
    }
}
