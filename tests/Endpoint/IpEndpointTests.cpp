#include <gtest/gtest.h>
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <unordered_map>

using Hermes::IpAddress;
using Hermes::IpEndpoint;
using Hermes::IpEndpointParams;

#pragma region Utils

static IpAddress I_MakeIpv4(const std::uint8_t a, const std::uint8_t b, const std::uint8_t c, const std::uint8_t d) {
    return IpAddress::FromIpv4({ a, b, c, d });
}

static IpAddress I_MakeIpv6Loopback() {
    return IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 });
}

#pragma endregion

#pragma region Core & Formatting (Parameterized)

struct EndpointTestCase {
    IpAddress ip;
    std::uint16_t port;
    std::string expectedFormat;
    std::string expectedFullFormat;
};

struct IpEndpointParamTest : testing::TestWithParam<EndpointTestCase> {};

TEST_P(IpEndpointParamTest, ValidatesConstructionAndFormatting) {
    const auto testCase{ GetParam() };
    const IpEndpoint endpoint{ testCase.ip, testCase.port };

    EXPECT_EQ(endpoint.GetIp(), testCase.ip);
    EXPECT_EQ(endpoint.GetPort(), testCase.port);

    EXPECT_EQ(std::format("{}", endpoint), testCase.expectedFormat);

    if (!testCase.expectedFullFormat.empty()) {
        EXPECT_EQ(std::format("{:f}", endpoint), testCase.expectedFullFormat);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensiveEndpointCases,
    IpEndpointParamTest,
    testing::Values(
        EndpointTestCase{ I_MakeIpv4(127, 0, 0, 1),    80,    "127.0.0.1:80",    "" },
        EndpointTestCase{ I_MakeIpv4(129, 0, 0, 1),    443,   "129.0.0.1:443",   "" },
        EndpointTestCase{ I_MakeIpv4(127, 0, 0, 1),    0,     "127.0.0.1:0",     "" },
        EndpointTestCase{ I_MakeIpv4(127, 0, 0, 1),    65535, "127.0.0.1:65535", "" },
        EndpointTestCase{ I_MakeIpv6Loopback(),        80,    "[::1]:80",        "[0000:0000:0000:0000:0000:0000:0000:0001]:80" }
    )
);

#pragma endregion

#pragma region Comparisons, Hashing & Static Builders

struct IpEndpointMiscTest : testing::Test {
    const IpAddress loopback4{ I_MakeIpv4(127, 0, 0, 1) };
    const IpAddress public4  { I_MakeIpv4(129, 0, 0, 1) };
    const IpAddress loopback6{ I_MakeIpv6Loopback() };

    const IpEndpoint ep80    { loopback4, 80 };
    const IpEndpoint ep443   { loopback4, 443 };
    const IpEndpoint ep80pub { public4, 80 };
    const IpEndpoint ep80v6  { loopback6, 80 };
    const IpEndpoint epZero  { loopback4, 0 };
};

TEST_F(IpEndpointMiscTest, Build_MatchesDirectConstruct) {
    const IpEndpoint viaBuild{ IpEndpoint::Build({ loopback4, 80 }) };
    EXPECT_EQ(viaBuild, ep80);
    EXPECT_EQ(viaBuild.GetIp(), loopback4);
}

TEST_F(IpEndpointMiscTest, CopyConstructAndAssign_PreservesData) {
    const IpEndpoint copy{ ep80 };
    EXPECT_EQ(copy, ep80);

    IpEndpoint assigned{};
    assigned = ep443;
    EXPECT_EQ(assigned, ep443);
}

TEST_F(IpEndpointMiscTest, Params_EqualityAndInequality) {
    const IpEndpointParams a{ loopback4, 80 };
    const IpEndpointParams b{ loopback4, 80 };
    const IpEndpointParams diffPort{ loopback4, 443 };
    const IpEndpointParams diffIp{ public4, 80 };

    EXPECT_EQ(a, b);
    EXPECT_NE(a, diffPort);
    EXPECT_NE(a, diffIp);
}

TEST_F(IpEndpointMiscTest, Endpoint_ComparisonsAndOrdering) {
    EXPECT_NE(ep80, ep443);
    EXPECT_NE(ep80, ep80pub);
    EXPECT_NE(ep80, ep80v6);

    EXPECT_LT(ep80, ep443);
    EXPECT_LT(epZero, ep80);
}

TEST_F(IpEndpointMiscTest, Hash_UsableAsUnorderedMapKey) {
    std::unordered_map<IpEndpoint, std::string> map{};
    map[ep80]  = "http";
    map[ep443] = "https";

    EXPECT_EQ(map.at(ep80),  "http");
    EXPECT_EQ(map.at(ep443), "https");

    EXPECT_EQ(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(IpEndpoint{ loopback4, 80 }));
    EXPECT_NE(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(ep443));
    EXPECT_NE(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(ep80pub));
    EXPECT_NE(std::hash<IpEndpoint>{}(ep80), std::hash<IpEndpoint>{}(ep80v6));
}

#pragma endregion