#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include "_base.hpp"

using Hermes::IpAddress;
using Hermes::IpEndpoint;


inline void IpAddressTests() {
    TestBattery("Ip Tests");

    const IpAddress emptyIp       = IpAddress::Empty();

    // IPv4
    const IpAddress loopbackIpv4   { IpAddress::FromIPv4({127, 0, 0, 1}) };
    const IpAddress privateIpv4    { IpAddress::FromIPv4({10, 0, 0, 1}) };
    const IpAddress siteLocalIpv4  { IpAddress::FromIPv4({172, 16, 0, 1}) };
    const IpAddress linkLocalIpv4  { IpAddress::FromIPv4({169, 254, 0, 1}) };
    const IpAddress publicIpv4     { IpAddress::FromIPv4({129, 0, 0, 1}) };
    const IpAddress multicastIpv4  { IpAddress::FromIPv4({224, 0, 0, 1}) };

    // IPv6
    const IpAddress loopbackIpv6       { IpAddress::FromIPv6({0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}) };
    const IpAddress unspecifiedIpv6    { IpAddress::FromIPv6({0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}) };
    const IpAddress linkLocalIpv6      { IpAddress::FromIPv6({0xfe,0x80,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}) };
    const IpAddress uniqueLocalIpv6    { IpAddress::FromIPv6({0xfd,0x0a,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}) };
    const IpAddress ipv4MappedIpv6     { IpAddress::FromIPv6({0,0,0,0, 0,0,0,0, 0,0,0xff,0xff, 10,0,0,1}) };
    const IpAddress documentationIpv6  { IpAddress::FromIPv6({0x20,0x01,0x0d,0xb8, 0,0,0,0, 0,0,0,0, 0,0,0,1}) };
    const IpAddress multicastIpv6      { IpAddress::FromIPv6({0xff,0x02,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}) };
    const IpAddress globalUnicastIpv6  { IpAddress::FromIPv6({0x81,0x00,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}) };

    const auto parsed { IpAddress::TryParse("::ffff:0:0") };
    const auto invalidParsed { IpAddress::TryParse("Certainly not an IP") };

    Test("Construct Default Empty Address",std::holds_alternative<std::monostate>     (emptyIp.data));
    Test("Construct Default IPv4 Address", std::holds_alternative<IpAddress::IPv4Type>(loopbackIpv4.data));
    Test("Construct Default IPv6 Address", std::holds_alternative<IpAddress::IPv6Type>(loopbackIpv6.data));

    Test("Assert IPv4 Value", loopbackIpv4.data == IpAddress::IpVariant{IpAddress::IPv4Type{127, 0, 0, 1}});
    Test("Assert IPv6 Value", loopbackIpv6.data == IpAddress::IpVariant{IpAddress::IPv6Type{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}});

    Test("Loopback IPv4", loopbackIpv4.IsLoopback());
    Test("Loopback IPv6", loopbackIpv6.IsLoopback());


    Test("Construct Default Empty Address", std::holds_alternative<std::monostate>(emptyIp.data));
    Test("Construct Default IPv4 Address", std::holds_alternative<IpAddress::IPv4Type>(loopbackIpv4.data));
    Test("Construct Default IPv6 Address", std::holds_alternative<IpAddress::IPv6Type>(loopbackIpv6.data));

    Test("Assert IPv4 Value", loopbackIpv4.data == IpAddress::IpVariant{IpAddress::IPv4Type{127, 0, 0, 1}});
    Test("Assert IPv6 Value", loopbackIpv6.data == IpAddress::IpVariant{IpAddress::IPv6Type{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1}});

    Test("Loopback IPv4", loopbackIpv4.IsLoopback());
    Test("Loopback IPv6", loopbackIpv6.IsLoopback());

    Test("Public IPv4", publicIpv4.IsPublic());
    Test("Private IPv4", privateIpv4.IsPrivate());
    Test("Site-local IPv4", siteLocalIpv4.IsSiteLocal());
    Test("Link-local IPv4", linkLocalIpv4.IsLinkLocal());
    Test("Multicast IPv4", multicastIpv4.IsMulticast());

    Test("Unspecified IPv6", unspecifiedIpv6.IsUnspecified());
    Test("Link-local IPv6", linkLocalIpv6.IsLinkLocal());
    Test("Unique Local IPv6", uniqueLocalIpv6.IsPrivate());
    Test("IPv4-mapped IPv6", ipv4MappedIpv6.IsIPv4Mapped());
    Test("Documentation IPv6", documentationIpv6.IsDocumentation());
    Test("Multicast IPv6", multicastIpv6.IsMulticast());
    Test("Global Unicast IPv6", globalUnicastIpv6.IsPublic());

    Test("TryParse Valid IPv6", parsed.has_value());
    Test("TryParse Invalid string", !invalidParsed.has_value());
}





bool TestFromSockAddr();
bool TestToSockAddr();

inline void IpEndpointTests() {
    TestBattery("IpEndpoint Tests");

    const IpEndpoint emptyEndpoint;

    const auto resolvedEndpoint{ IpEndpoint::TryResolve("localhost") };
    const auto invalidResolvedEndpoint{ IpEndpoint::TryResolve("Certainly not an URL")
};

    Test("Empty Endpoint", emptyEndpoint.ip.IsEmpty() && emptyEndpoint.port == 0);

    Test("Resolve Valid Hostname", resolvedEndpoint
        && resolvedEndpoint->ip.IsLoopback()
        && resolvedEndpoint->port == 0);
    Test("Resolve Invalid Hostname", !invalidResolvedEndpoint.has_value());

    Test("FromSockAddr", TestFromSockAddr());
    Test("ToSockAddr", TestToSockAddr());

    static_assert(Hermes::EndpointConcept<IpEndpoint>, "IpEndpoint must satisfy EndpointConcept");
}



inline bool TestFromSockAddr() {
    constexpr int port{ 12345 };

    sockaddr_in sa4{};
    sa4.sin_family = AF_INET;
    sa4.sin_port = htons(port);
    sa4.sin_addr.s_addr = htonl(0x7F000001); // 127.0.0.1

    using SocketInfoAddr = std::tuple<sockaddr_storage, size_t, AddressFamilyEnum>;
    const SocketInfoAddr infoAddr4{*reinterpret_cast<sockaddr_storage*>(&sa4), sizeof(sa4), AddressFamilyEnum::INET};

    const auto& res4{ IpEndpoint::FromSockAddr(infoAddr4) };
    const bool ipv4Ok{
        res4.has_value()
        && res4->ip.IsIPv4()
        && res4->ip.IsLoopback()
        && res4->port == port
    };

    // IPv6
    sockaddr_in6 sa6{};
    sa6.sin6_family = AF_INET6;
    sa6.sin6_port = htons(port);
    sa6.sin6_addr.u = in6addr_loopback.u; // ::1

    const SocketInfoAddr infoAddr6{*reinterpret_cast<sockaddr_storage*>(&sa6), sizeof(sa6), AddressFamilyEnum::INET6};

    const auto& res6{ IpEndpoint::FromSockAddr(infoAddr6) };
    const bool ipv6Ok{
        res6.has_value()
        && res6->ip.IsIPv6()
        && res6->ip.IsLoopback()
        && res6->port == port
    };

    return ipv4Ok && ipv6Ok;
}

inline bool TestToSockAddr() {
    const auto resolvedEndpoint{ IpEndpoint::TryResolve("localhost") };
    if (!resolvedEndpoint)
        return false;

    const auto res{ resolvedEndpoint->ToSockAddr() };
    if (!res)
        return false;

    const auto sockAddrEndpoint{ IpEndpoint::FromSockAddr(*res) };
    if (!sockAddrEndpoint)
        return false;

    return resolvedEndpoint == sockAddrEndpoint;
}