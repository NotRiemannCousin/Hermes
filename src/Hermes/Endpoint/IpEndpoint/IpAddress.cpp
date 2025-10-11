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

    bool IpAddress::operator==(const IpAddress &ip) const {
        return data == ip.data;
    }

    //----------------------------------------------------------------------------------------------------
    // Getters
    //----------------------------------------------------------------------------------------------------


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
            const auto ipv4{ get<IPv4Type>(data) };

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
            const auto ipv6{ get<IPv6Type>(data) };

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
            const auto ipv4{ get<IPv4Type>(data) };

            if (ipv4[0] == 10)
                return false;
            if (ipv4[0] == 172 && ipv4[1] == std::clamp(ipv4[1], 16_uc, 31_uc))
                return false;
            if (ipv4[0] == 192 && ipv4[1] == 168)
                return false;
            if (ipv4[0] == 100 && (ipv4[1] >= 64 && ipv4[1] <= 127)) // CGNAT 100.64.0.0/10
                return false;
            return true;
        }
        const auto ipv6{ get<IPv6Type>(data) };

        if ((ipv6[0] & 0xfe) == 0xfc)
            return false;
        if (ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80)
            return false;
        return true;
    }

    bool IpAddress::IsPrivate() const {
        return !IsPublic();
    }

    bool IpAddress::IsLoopback() const {
        if (IsIPv4()) {
            const auto& ipv4 = get<IPv4Type>(data);
            return ipv4[0] == 127;
        }
        static constexpr IPv6Type _ipv6Loopback{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        return _ipv6Loopback == get<IPv6Type>(data);
    }


    bool IpAddress::IsMulticast() const {
        if (IsEmpty())
            return false;

        if (IsIPv4()) {
            const auto ipv4{ get<IPv4Type>(data) };

            if (ipv4[0] >= 224 && ipv4[0] <= 239)
                return true;
            if (ipv4 == IPv4Type{255, 255, 255, 255})
                return true;
        } else {
            const auto ipv6{ get<IPv6Type>(data) };

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
            const auto ipv4{ get<IPv4Type>(data) };
            return ipv4[0] == 169 && ipv4[1] == 254;
        }

        const auto ipv6{ get<IPv6Type>(data) };
        return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80;
    }

    bool IpAddress::IsSiteLocal() const noexcept {
        if (IsEmpty())
            return false;

        if (IsIPv4()) {
            const auto ipv4{ std::get<IPv4Type>(data) };
            return ipv4[0] == 172 && (ipv4[1] >= 16 && ipv4[1] <= 31);
        }

        const auto ipv6{ std::get<IPv6Type>(data) };
        return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0xc0;
    }

    bool IpAddress::IsIPv4Mapped() const noexcept {
        if (!std::holds_alternative<IPv6Type>(data))
            return false;

        const auto& v6 = std::get<IPv6Type>(data);

        for (int i = 0; i < 10; ++i)
            if (v6[i] != 0) return false;

        return v6[10] == 0xff && v6[11] == 0xff;
    }

    bool IpAddress::IsDocumentation() const noexcept {
        if (!std::holds_alternative<IPv6Type>(data))
            return false;

        const auto& v6 = std::get<IPv6Type>(data);
        return v6[0] == 0x20
            && v6[1] == 0x01
            && v6[2] == 0x0d
            && v6[3] == 0xb8;
    }
} // namespace Hermes
