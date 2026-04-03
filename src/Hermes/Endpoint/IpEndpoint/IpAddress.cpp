#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>
#include <Hermes/_base/Network.hpp>
#include <algorithm>
#include <bit>

using std::string;
using std::string_view;
using std::optional;


using Hermes::IpAddress;

//----------------------------------------------------------------------------------------------------
// Construct
//----------------------------------------------------------------------------------------------------

IpAddress::IpAddress(const IpVariant& d) : _data(d) {}

IpAddress IpAddress::FromIpv4(const Ipv4Type &data) { return IpAddress{ data }; }

//----------------------------------------------------------------------------------------------------
// Construct
//----------------------------------------------------------------------------------------------------

IpAddress IpAddress::FromIpv6(const Ipv6Type &data) { return IpAddress{ data }; }

std::optional<IpAddress> IpAddress::TryParse(const string& str) {
    Network::Initialize();

    // ReSharper disable once CppTooWideScopeInitStatement
    sockaddr_in6 sa6{};
    if (inet_pton(static_cast<int>(AddressFamilyEnum::Inet6), str.c_str(), &sa6.sin6_addr) != 0)
        return FromIpv6(std::bit_cast<Ipv6Type>(sa6.sin6_addr));

    // ReSharper disable once CppTooWideScopeInitStatement
    sockaddr_in sa{};
    if (inet_pton(static_cast<int>(AddressFamilyEnum::Inet), str.c_str(), &sa.sin_addr) != 0)
        return FromIpv4(std::bit_cast<Ipv4Type>(sa.sin_addr));

    return std::nullopt;
}

IpAddress::IpVariant IpAddress::GetIp() const {
    return _data;
}

//----------------------------------------------------------------------------------------------------
// Getters
//----------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------
// Checks
//----------------------------------------------------------------------------------------------------


bool IpAddress::IsIpv4()  const { return std::holds_alternative<Ipv4Type>(_data); }

bool IpAddress::IsIpv6()  const { return std::holds_alternative<Ipv6Type>(_data); }


bool IpAddress::IsRoutable() const {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) {
                if (ipv4 == Ipv4Type{})
                    return false;
                if (ipv4[0] == 0)
                    return false;
                if (ipv4[0] == 127)
                    return false;
                if (ipv4[0] == 169 && ipv4[1] == 254)
                    return false;
                if (ipv4[0] >= 224)
                    return false;

                return true;
            },
            [](const Ipv6Type ipv6) {
                if (ipv6 == Ipv6Type{})
                    return false;
                if ((ipv6[0] & 0xfe) == 0xfc)
                    return false;
                if (ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80)
                    return false;
                if (ipv6[0] == 0xff)
                    return false;

                return true;
            }
        } , _data);
}


bool IpAddress::IsPublic() const {
    return IsRoutable() && std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) {
                if (ipv4[0] == 10)
                    return false;
                if (ipv4[0] == 172 && ipv4[1] >= 16 && ipv4[1] <= 31)
                    return false;
                if (ipv4[0] == 192 && ipv4[1] == 168)
                    return false;
                if (ipv4[0] == 100 && ipv4[1] >= 64 && ipv4[1] <= 127) // CGNAT 100.64.0.0/10
                    return false;
                return true;
            },
            [](const Ipv6Type ipv6) {
                if ((ipv6[0] & 0xfe) == 0xfc)
                    return false;
                if (ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80)
                    return false;
                return true;
            },
        }, _data);
    }

bool IpAddress::IsPrivate() const {
    return std::visit(Utils::Overloaded{
        [](const Ipv4Type ipv4) {
            return ipv4[0] == 10 ||
                  (ipv4[0] == 172 && ipv4[1] >= 16 && ipv4[1] <= 31) ||
                  (ipv4[0] == 192 && ipv4[1] == 168);
        },
        [](const Ipv6Type ipv6) { return (ipv6[0] & 0xfe) == 0xfc; }
    }, _data);
}

bool IpAddress::IsLoopback() const {
    static constexpr Ipv6Type _ipv6Loopback{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) { return ipv4[0] == 127;        },
            [](const Ipv6Type ipv6) { return ipv6 == _ipv6Loopback; },
        }, _data);
}


bool IpAddress::IsMulticast() const {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) {
                if (ipv4[0] >= 224 && ipv4[0] <= 239)
                    return true;
                if (ipv4 == Ipv4Type{255, 255, 255, 255})
                    return true;
                return false;
            },
            [](const Ipv6Type ipv6) { return ipv6[0] == 0xff; },
        }, _data);
}

bool IpAddress::IsUnspecified() const {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) { return ipv4 == Ipv4Type{}; },
            [](const Ipv6Type ipv6) { return ipv6 == Ipv6Type{}; },
        }, _data);
}

bool IpAddress::IsLinkLocal() const noexcept {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) { return ipv4[0] == 169 && ipv4[1] == 254;            },
            [](const Ipv6Type ipv6) { return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80; }
        }, _data);
}

bool IpAddress::IsSiteLocal() const noexcept {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) {
                if (ipv4[0] == 10) return true;
                if (ipv4[0] == 172 && ipv4[1] >= 16 && ipv4[1] <= 31) return true;
                if (ipv4[0] == 192 && ipv4[1] == 168) return true;
                return false;
            },
            [](const Ipv6Type ipv6) { return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0xc0; },
        }, _data);
}

bool IpAddress::IsIpv4Mapped() const noexcept {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type _   ) { return false; },
            [](const Ipv6Type ipv6) {
                for (int i{}; i < 10; ++i)
                    if (ipv6[i] != 0) return false;

                return ipv6[10] == 0xff && ipv6[11] == 0xff;
            },
        }, _data);
}

bool IpAddress::IsDocumentation() const noexcept {
    return std::visit(
        Utils::Overloaded{
            [](const Ipv4Type ipv4) {
                if (ipv4[0] == 192 && ipv4[1] == 0  && ipv4[2] == 2  ) return true;
                if (ipv4[0] == 198 && ipv4[1] == 51 && ipv4[2] == 100) return true;
                if (ipv4[0] == 203 && ipv4[1] == 0  && ipv4[2] == 113) return true;
                return false;
            },
            [](const Ipv6Type ipv6) {
                return ipv6[0] == 0x20 && ipv6[1] == 0x01 && ipv6[2] == 0x0d && ipv6[3] == 0xb8;
            },
        }, _data);
}