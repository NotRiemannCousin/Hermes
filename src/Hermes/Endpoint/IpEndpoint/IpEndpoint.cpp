// ReSharper disable CppPassValueParameterByConstReference
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/_base/Network.hpp>
#include <Hermes/_base/WinApi/WinApi.hpp>

using std::uint16_t;
using std::uint8_t;
using std::bit_cast;

using std::string;

using namespace Hermes;

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
    size_t size{ sizeof(sockaddr_in6) };

    const auto ipv6{ _ip.AsIpv6() };

    auto *in6{ reinterpret_cast<sockaddr_in6*>(&addr) };
    in6->sin6_family = _tus(AddressFamilyEnum::Inet6);
    in6->sin6_port   = htons(_port);
    in6->sin6_addr   = bit_cast<in6_addr>(ipv6);

    return SocketInfoAddr{ addr, size, AddressFamilyEnum::Inet6 };
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

    for (addrinfo* ptr{ result.get() }; ptr != nullptr; ptr = ptr->ai_next) {
        sockaddr* addr{ ptr->ai_addr };

        if (addr->sa_family == AddressFamilyEnum::Inet6) {
            const auto* ipv6{ reinterpret_cast<sockaddr_in6*>(addr) };
            return IpEndpoint{
                IpAddress::FromIpv6(std::bit_cast<IpAddress::Ipv6Type>(ipv6->sin6_addr)),
                ntohs(ipv6->sin6_port)
            };
        }

        if (addr->sa_family == AddressFamilyEnum::Inet) {
            const auto* ipv4{ reinterpret_cast<sockaddr_in*>(addr) };
            return IpEndpoint{
                IpAddress::FromIpv4(std::bit_cast<IpAddress::Ipv4Type>(ipv4->sin_addr)),
                ntohs(ipv4->sin_port)
            };
        }
    }

    return std::unexpected{ ConnectionErrorEnum::UnsupportedAddressFamily };
}

IpAddress IpEndpoint::GetIp() const noexcept {
    return _ip;
}

std::uint16_t IpEndpoint::GetPort() const noexcept {
    return _port;
}