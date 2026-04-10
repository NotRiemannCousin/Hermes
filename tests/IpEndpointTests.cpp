#include <gtest/gtest.h>
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::IpEndpointParams;


struct IpEndpointTest : testing::Test {
    // Addresses
    const IpAddress loopback4    { IpAddress::FromIpv4({ 127,   0,   0, 1 }) };
    const IpAddress public4      { IpAddress::FromIpv4({ 129,   0,   0, 1 }) };
    const IpAddress loopback6    { IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };

    // Endpoints
    const IpEndpoint ep80        { loopback4, 80    };
    const IpEndpoint ep443       { loopback4, 443   };
    const IpEndpoint ep80pub     { public4,   80    };
    const IpEndpoint ep80v6      { loopback6, 80    };
    const IpEndpoint epZeroPort  { loopback4, 0     };
    const IpEndpoint epMaxPort   { loopback4, 65535 };
};


#pragma region Construction & Getters

TEST_F(IpEndpointTest, DefaultConstruct_ZeroPort) {
    const IpEndpoint ep{};
    EXPECT_EQ(ep.GetPort(), 0);
}

TEST_F(IpEndpointTest, GetIp_ReturnsCorrectIpv4) {
    EXPECT_EQ(ep80.GetIp(), loopback4);
}

TEST_F(IpEndpointTest, GetIp_ReturnsCorrectIpv6) {
    EXPECT_EQ(ep80v6.GetIp(), loopback6);
}

TEST_F(IpEndpointTest, GetPort_Returns80) {
    EXPECT_EQ(ep80.GetPort(), 80);
}

TEST_F(IpEndpointTest, GetPort_Returns443) {
    EXPECT_EQ(ep443.GetPort(), 443);
}

TEST_F(IpEndpointTest, GetPort_ReturnsZero) {
    EXPECT_EQ(epZeroPort.GetPort(), 0);
}

TEST_F(IpEndpointTest, GetPort_ReturnsMax) {
    EXPECT_EQ(epMaxPort.GetPort(), 65535);
}

TEST_F(IpEndpointTest, CopyConstruct_EqualToOriginal) {
    const IpEndpoint copy{ ep80 };
    EXPECT_EQ(copy, ep80);
}

TEST_F(IpEndpointTest, CopyAssign_EqualToOriginal) {
    IpEndpoint copy{ loopback4, 9999 };
    copy = ep80;
    EXPECT_EQ(copy, ep80);
}

#pragma endregion

#pragma region Build

TEST_F(IpEndpointTest, Build_ReturnsEndpointWithCorrectIp) {
    const IpEndpoint ep{ IpEndpoint::Build({ loopback4, 8080 }) };
    EXPECT_EQ(ep.GetIp(), loopback4);
}

TEST_F(IpEndpointTest, Build_ReturnsEndpointWithCorrectPort) {
    const IpEndpoint ep{ IpEndpoint::Build({ loopback4, 8080 }) };
    EXPECT_EQ(ep.GetPort(), 8080);
}

TEST_F(IpEndpointTest, Build_Ipv6_CorrectIpAndPort) {
    const IpEndpoint ep{ IpEndpoint::Build({ loopback6, 9000 }) };
    EXPECT_EQ(ep.GetIp(),   loopback6);
    EXPECT_EQ(ep.GetPort(), 9000);
}

TEST_F(IpEndpointTest, Build_MatchesDirectConstruct) {
    const IpEndpoint via_build    { IpEndpoint::Build({ loopback4, 1234 }) };
    const IpEndpoint via_construct{ loopback4, 1234 };
    EXPECT_EQ(via_build, via_construct);
}

#pragma endregion

#pragma region IpEndpointParams comparisons

TEST_F(IpEndpointTest, Params_Equality_SameIpAndPort) {
    const IpEndpointParams a{ loopback4, 80 };
    const IpEndpointParams b{ loopback4, 80 };
    EXPECT_EQ(a, b);
}

TEST_F(IpEndpointTest, Params_Inequality_DifferentPort) {
    const IpEndpointParams a{ loopback4,  80 };
    const IpEndpointParams b{ loopback4, 443 };
    EXPECT_NE(a, b);
}

TEST_F(IpEndpointTest, Params_Inequality_DifferentIp) {
    const IpEndpointParams a{ loopback4, 80 };
    const IpEndpointParams b{ public4,   80 };
    EXPECT_NE(a, b);
}

#pragma endregion

#pragma region Endpoint comparisons

TEST_F(IpEndpointTest, Equality_SameIpAndPort) {
    const IpEndpoint a{ loopback4, 80 };
    const IpEndpoint b{ loopback4, 80 };
    EXPECT_EQ(a, b);
}

TEST_F(IpEndpointTest, Inequality_DifferentPort) {
    EXPECT_NE(ep80, ep443);
}

TEST_F(IpEndpointTest, Inequality_DifferentIp) {
    EXPECT_NE(ep80, ep80pub);
}

TEST_F(IpEndpointTest, Inequality_DifferentIpVersion) {
    EXPECT_NE(ep80, ep80v6);
}

TEST_F(IpEndpointTest, Ordering_SameIp_LowerPortIsLess) {
    EXPECT_LT(ep80, ep443);
}

TEST_F(IpEndpointTest, Ordering_ZeroPort_LessThanNonZero) {
    EXPECT_LT(epZeroPort, ep80);
}

#pragma endregion

#pragma region Hashing

TEST_F(IpEndpointTest, Hash_SameEndpoint_SameHash) {
    const IpEndpoint a{ loopback4, 80 };
    const IpEndpoint b{ loopback4, 80 };
    EXPECT_EQ(std::hash<IpEndpoint>{}(a), std::hash<IpEndpoint>{}(b));
}

TEST_F(IpEndpointTest, Hash_DifferentPort_DifferentHash) {
    EXPECT_NE(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(ep443));
}

TEST_F(IpEndpointTest, Hash_DifferentIp_DifferentHash) {
    EXPECT_NE(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(ep80pub));
}

TEST_F(IpEndpointTest, Hash_Ipv4VsIpv6_DifferentHash) {
    EXPECT_NE(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(ep80v6));
}

TEST_F(IpEndpointTest, Hash_UsableAsUnorderedMapKey) {
    std::unordered_map<IpEndpoint, std::string> map;
    map[ep80]  = "http";
    map[ep443] = "https";
    EXPECT_EQ(map.at(ep80),  "http");
    EXPECT_EQ(map.at(ep443), "https");
}

#pragma endregion

#pragma region Formatting

TEST_F(IpEndpointTest, Format_Ipv4_Port80) {
    EXPECT_EQ(std::format("{}", ep80), "127.0.0.1:80");
}

TEST_F(IpEndpointTest, Format_Ipv4_Port443) {
    EXPECT_EQ(std::format("{}", ep443), "127.0.0.1:443");
}

TEST_F(IpEndpointTest, Format_Ipv4_PublicIp) {
    EXPECT_EQ(std::format("{}", ep80pub), "129.0.0.1:80");
}

TEST_F(IpEndpointTest, Format_Ipv4_ZeroPort) {
    EXPECT_EQ(std::format("{}", epZeroPort), "127.0.0.1:0");
}

TEST_F(IpEndpointTest, Format_Ipv4_MaxPort) {
    EXPECT_EQ(std::format("{}", epMaxPort), "127.0.0.1:65535");
}

TEST_F(IpEndpointTest, Format_Ipv6_FullForm) {
    EXPECT_EQ(std::format("{}", ep80v6), "0000:0000:0000:0000:0000:0000:0000:0001:80");
}

#pragma endregion