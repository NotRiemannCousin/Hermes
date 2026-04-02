#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>
#include <Hermes/_base/Network.hpp>
#include <algorithm>
#include <bit>

using std::string;
using std::string_view;
using std::optional;

// Most of these things weren't useful to me lol

namespace Hermes {

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
            return FromIpv4(std::bit_cast<Ipv4Type>(sa.sin_addr.S_un.S_addr));

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


    bool IpAddress::IsValid() const {
        if (IsIpv4()) {
            const auto ipv4{ get<Ipv4Type>(_data) };

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
        } else {
            const auto ipv6{ get<Ipv6Type>(_data) };

            if (ipv6 == Ipv6Type{})
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

        if (IsIpv4()) {
            const auto ipv4{ get<Ipv4Type>(_data) };

            if (ipv4[0] == 10)
                return false;
            if (ipv4[0] == 172 && ipv4[1] == std::clamp(ipv4[1], 16_uc, 31_uc))
                return false;
            if (ipv4[0] == 192 && ipv4[1] == 168)
                return false;
            if (ipv4[0] == 100 && ipv4[1] >= 64 && ipv4[1] <= 127) // CGNAT 100.64.0.0/10
                return false;
            return true;
        }
        const auto ipv6{ get<Ipv6Type>(_data) };

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
        if (IsIpv4()) {
            const auto& ipv4{ get<Ipv4Type>(_data) };
            return ipv4[0] == 127;
        }
        static constexpr Ipv6Type _ipv6Loopback{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        return _ipv6Loopback == get<Ipv6Type>(_data);
    }


    bool IpAddress::IsMulticast() const {
        if (IsIpv4()) {
            const auto ipv4{ get<Ipv4Type>(_data) };

            if (ipv4[0] >= 224 && ipv4[0] <= 239)
                return true;
            if (ipv4 == Ipv4Type{255, 255, 255, 255})
                return true;
        } else {
            // ReSharper disable once CppTooWideScopeInitStatement
            if (get<Ipv6Type>(_data)[0] == 0xff)
                return true;
        }

        return false;
    }

    bool IpAddress::IsUnspecified() const {
        if (IsIpv6())
            return get<Ipv4Type>(_data) == Ipv4Type{};

        return get<Ipv6Type>(_data) == Ipv6Type{};
    }

    bool IpAddress::IsLinkLocal() const noexcept {
        if (IsIpv4()) {
            const auto ipv4{ get<Ipv4Type>(_data) };
            return ipv4[0] == 169 && ipv4[1] == 254;
        }

        const auto ipv6{ get<Ipv6Type>(_data) };
        return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0x80;
    }

    bool IpAddress::IsSiteLocal() const noexcept {
        if (IsIpv4()) {
            const auto ipv4{ std::get<Ipv4Type>(_data) };
            return ipv4[0] == 172 && ipv4[1] >= 16 && ipv4[1] <= 31;
        }

        const auto ipv6{ std::get<Ipv6Type>(_data) };
        return ipv6[0] == 0xfe && (ipv6[1] & 0xc0) == 0xc0;
    }

    bool IpAddress::IsIpv4Mapped() const noexcept {
        if (!std::holds_alternative<Ipv6Type>(_data))
            return false;

        const auto& v6{ std::get<Ipv6Type>(_data) };

        for (int i{}; i < 10; ++i)
            if (v6[i] != 0) return false;

        return v6[10] == 0xff && v6[11] == 0xff;
    }

    bool IpAddress::IsDocumentation() const noexcept {
        if (!std::holds_alternative<Ipv6Type>(_data))
            return false;

        const auto& v6{ std::get<Ipv6Type>(_data) };
        return v6[0] == 0x20
            && v6[1] == 0x01
            && v6[2] == 0x0d
            && v6[3] == 0xb8;
    }
}
