#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>
#include <Hermes/_base/Network.hpp>
#include <algorithm>
#include <bit>

using std::string;
using std::string_view;
using std::unexpected;
using std::expected;
using std::optional;

// Most of these things weren't useful to me lol

namespace Hermes {

    //----------------------------------------------------------------------------------------------------
    // Construct
    //----------------------------------------------------------------------------------------------------

    IpAddress::IpAddress(const IpVariant& d) : data(d) {}

    IpAddress IpAddress::Empty() { return IpAddress(std::monostate{}); }

    IpAddress IpAddress::FromIPv4(const IPv4Type &data) { return IpAddress(data); }

    //----------------------------------------------------------------------------------------------------
    // Construct
    //----------------------------------------------------------------------------------------------------

    IpAddress IpAddress::FromIPv6(const IPv6Type &data) { return IpAddress(data); }

    std::optional<IpAddress> IpAddress::TryParse(const string& str) {
        Network::Initialize();

        sockaddr_in6 sa6{};
        if (inet_pton(static_cast<int>(AddressFamilyEnum::INET6), str.c_str(), &(sa6.sin6_addr)) != 0)
            return FromIPv6(std::bit_cast<IPv6Type>(sa6.sin6_addr));

        sockaddr_in sa{};
        if (inet_pton(static_cast<int>(AddressFamilyEnum::INET), str.c_str(), &(sa.sin_addr)) != 0)
            return FromIPv4(std::bit_cast<IPv4Type>(sa.sin_addr.S_un.S_addr));

        return std::nullopt;
    }

    // std::optional<IpAddress> IpAddress::TryResolve(string_view Uri) {
    //     Network::Initialize();
    //
    //     constexpr static addrinfo hints = {.ai_family = static_cast<int>(AddressFamilyEnum::UNSPEC), .ai_socktype = (int) SocketTypeEnum::STREAM};
    //
    //     addrinfo *result = nullptr;
    //     // string is copied because it mays be not null-terminated
    //     if (int err = getaddrinfo(string(Uri).data(), nullptr, &hints, &result); err != 0)
    //         return std::nullopt;
    //
    //     if (result == nullptr)
    //         return std::nullopt;
    //
    //     auto addr = result->ai_addr;
    //     if (addr == nullptr)
    //         return std::nullopt;
    //
    //     if (addr->sa_family == AddressFamilyEnum::INET) {
    //         auto ipv4 = reinterpret_cast<sockaddr_in *>(addr);
    //         return FromIPv4(std::bit_cast<IPv4Type>(ipv4->sin_addr));
    //     }
    //
    //     if (addr->sa_family == AddressFamilyEnum::INET6) {
    //         auto ipv6 = reinterpret_cast<sockaddr_in6 *>(addr);
    //         return FromIPv6(std::bit_cast<IPv6Type>(ipv6->sin6_addr));
    //     }
    //
    //     return std::nullopt;
    // }

    //----------------------------------------------------------------------------------------------------
    // Getters
    //----------------------------------------------------------------------------------------------------

    IpAddress::IpClassEnum IpAddress::GetClass() const {
        if (!IsValid())
            return IpClassEnum::INVALID;
        if (IsIPv6())
            return IpClassEnum::IPV6;

        auto &&ipv4 = get<IPv4Type>(data);
        if ((ipv4[0] & 0x80) == 0x00)
            return IpClassEnum::A;
        if ((ipv4[0] & 0xc0) == 0x80)
            return IpClassEnum::B;
        if ((ipv4[0] & 0xe0) == 0xc0)
            return IpClassEnum::C;
        if ((ipv4[0] & 0xf0) == 0xe0)
            return IpClassEnum::D;
        if ((ipv4[0] & 0xf8) == 0xf0)
            return IpClassEnum::E;

        return IpClassEnum::INVALID;
    }


    //----------------------------------------------------------------------------------------------------
    // Checks
    //----------------------------------------------------------------------------------------------------

    bool IpAddress::IsEmpty() const { return std::holds_alternative<std::monostate>(data); }


    bool IpAddress::IsIPv4()  const { return std::holds_alternative<IpAddress::IPv4Type>(data); }

    bool IpAddress::IsIPv6()  const { return std::holds_alternative<IpAddress::IPv6Type>(data); }


    bool IpAddress::IsValid() const {
        if (IsEmpty())
            return false;

        if (IsIPv4()) {
            auto ipv4 = get<IPv4Type>(data);

            if (ipv4 == IPv4Type{})
                return false;
            if (ipv4[0] == 0)
                return false;
            if (ipv4[0] == 127)
                return false;
            if (ipv4[0] == 169 && ipv4[1] == 254)
                return false;
            if (ipv4[0] >= 224)
                return false;
        } else {
            auto ipv6 = get<IPv6Type>(data);

            if (ipv6 == IPv6Type{})
                return false;
            if ((ipv6[0] & 0xfe) == 0xfc)
                return false;
            if (ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80)
                return false;
            if (ipv6[0] == 0xff)
                return false;
        }

        return true;
    }


    bool IpAddress::IsPublic() const {
        if (!IsValid())
            return false;

        if (IsIPv4()) {
            auto ipv4 = get<IPv4Type>(data);

            if (ipv4[0] == 10)
                return false;
            if (ipv4[0] == 172 && ipv4[1] == std::clamp(ipv4[1], 16_uc, 31_uc))
                return false;
            if (ipv4[0] == 192 && ipv4[1] == 168)
                return false;
            if (ipv4[0] == 100 && (ipv4[1] >= 64 && ipv4[1] <= 127)) // CGNAT 100.64.0.0/10
                return false;
            return true;
        } else {
            auto ipv6 = get<IPv6Type>(data);

            if ((ipv6[0] & 0xfe) == 0xfc)
                return false;
            if (ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80)
                return false;
            return true;
        }
    }

    bool IpAddress::IsLoopback() const {
        if (IsIPv4()) {
            auto ipv4 = get<IPv4Type>(data);
            return ipv4[0] == 127;
        }
        static constexpr IPv6Type _ipv6Loopback{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        return _ipv6Loopback == get<IPv6Type>(data);
    }


    bool IpAddress::IsMulticast() const {
        if (IsEmpty())
            return false;

        if (IsIPv4()) {
            auto ipv4 = get<IPv4Type>(data);

            if (ipv4[0] >= 224 && ipv4[0] <= 239)
                return true;
            if (ipv4 == IPv4Type{255, 255, 255, 255})
                return true;
        } else {
            auto ipv6 = get<IPv6Type>(data);

            if (ipv6[0] == 0xff)
                return true;
        }

        return false;
    }

    bool IpAddress::IsUnspecified() const {
        if (!IsEmpty())
            return true;

        if (IsIPv6())
            return get<IPv4Type>(data) == IPv4Type{};

        return get<IPv6Type>(data) == IPv6Type{};
    }

    bool IpAddress::IsLinkLocal() const noexcept {
        if (IsEmpty())
            return false;

        if (IsIPv4()) {
            auto ipv4 = get<IPv4Type>(data);
            return ipv4[0] == 169 && ipv4[1] == 254;
        } else {
            auto ipv6 = get<IPv6Type>(data);
            return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80;
        }
    }

    bool IpAddress::IsSiteLocal() const noexcept {
        if (IsEmpty())
            return false;

        if (IsIPv4()) {
            auto ipv4 = get<IPv4Type>(data);
            return ipv4[0] == 169 && ipv4[1] == 254;
        } else {
            auto ipv6 = get<IPv6Type>(data);
            return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0xc0;
        }
    }

} // namespace Hermes
