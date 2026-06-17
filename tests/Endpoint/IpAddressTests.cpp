#include <gtest/gtest.h>
#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>

#include <string>
#include <string_view>
#include <format>
#include <functional>

using Hermes::IpAddress;

#pragma region Parameterized Property Tests

struct IpPropertyTestCase {
    std::string_view ipStr;
    bool shouldParse;
    bool isIpv4;
    bool isLoopback;
    bool isPrivate;
    bool isMulticast;
    bool isLinkLocal;
    bool isSiteLocal;
    bool isUnspecified;
    bool isIpv4Mapped;
    bool isDocumentation;
    bool isRoutable;
};

struct IpAddressPropertyTest : testing::TestWithParam<IpPropertyTestCase> {};

TEST_P(IpAddressPropertyTest, ValidatesParsingAndAllProperties) {
    const auto testCase{ GetParam() };
    const auto parsed{ IpAddress::TryParse(std::string{ testCase.ipStr }) };

    EXPECT_EQ(parsed.has_value(), testCase.shouldParse);

    if (parsed.has_value()) {
        EXPECT_EQ(parsed->IsIpv4()         , testCase.isIpv4);
        EXPECT_EQ(parsed->IsIpv6()         , !testCase.isIpv4);
        EXPECT_EQ(parsed->IsLoopback()     , testCase.isLoopback);
        EXPECT_EQ(parsed->IsPrivate()      , testCase.isPrivate);
        EXPECT_EQ(parsed->IsMulticast()    , testCase.isMulticast);
        EXPECT_EQ(parsed->IsLinkLocal()    , testCase.isLinkLocal);
        EXPECT_EQ(parsed->IsSiteLocal()    , testCase.isSiteLocal);
        EXPECT_EQ(parsed->IsUnspecified()  , testCase.isUnspecified);
        EXPECT_EQ(parsed->IsIpv4Mapped()   , testCase.isIpv4Mapped);
        EXPECT_EQ(parsed->IsDocumentation(), testCase.isDocumentation);
        EXPECT_EQ(parsed->IsRoutable()     , testCase.isRoutable);

        if (testCase.isRoutable && !testCase.isPrivate) {
            EXPECT_TRUE(parsed->IsPublic());
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensiveIpCases,
    IpAddressPropertyTest,
    testing::Values(
        //                 IP Str               Parse  v4     Loopb  Priv   Multi  Link   Site   Unsp   Map    Doc    Rout
        // IPv4
        IpPropertyTestCase{ "127.0.0.1",        true , true , true , false, false, false, false, false, false, false, false },
        IpPropertyTestCase{ "10.0.0.1",         true , true , false, true , false, false, true , false, false, false, false },
        IpPropertyTestCase{ "172.16.0.1",       true , true , false, true , false, false, true , false, false, false, false },
        IpPropertyTestCase{ "192.168.1.1",      true , true , false, true , false, false, true , false, false, false, false },
        IpPropertyTestCase{ "169.254.0.1",      true , true , false, false, false, true , false, false, false, false, false },
        IpPropertyTestCase{ "129.0.0.1",        true , true , false, false, false, false, false, false, false, false, true  },
        IpPropertyTestCase{ "224.0.0.1",        true , true , false, false, true , false, false, false, false, false, false },
        IpPropertyTestCase{ "255.255.255.255",  true , true , false, false, true , false, false, false, false, false, false },
        IpPropertyTestCase{ "100.64.0.1",       true , true , false, true , false, false, false, false, false, false, false }, // CGNAT mapeado como Private=true
        IpPropertyTestCase{ "0.0.0.0",          true , true , false, false, false, false, false, true , false, false, false },

        // IPv6
        IpPropertyTestCase{ "::1",              true , false, true , false, false, false, false, false, false, false, false },
        IpPropertyTestCase{ "::",               true , false, false, false, false, false, false, true , false, false, false },
        IpPropertyTestCase{ "fe80::1",          true , false, false, false, false, true , false, false, false, false, false },
        IpPropertyTestCase{ "fd0a::1",          true , false, false, true , false, false, false, false, false, false, false },
        IpPropertyTestCase{ "fec0::1",          true , false, false, false, false, false, true , false, false, false, true  }, // Routable=true
        IpPropertyTestCase{ "::ffff:10.0.0.1",  true , false, false, false, false, false, false, false, true , false, true  }, // Routable=true
        IpPropertyTestCase{ "2001:db8::1",      true , false, false, false, false, false, false, false, false, true , true  }, // Routable=true
        IpPropertyTestCase{ "ff02::1",          true , false, false, false, true , false, false, false, false, false, false },
        IpPropertyTestCase{ "8100::1",          true , false, false, false, false, false, false, false, false, false, true  },

        // Errors
        IpPropertyTestCase{ "CertainlyNot",     false, false, false, false, false, false, false, false, false, false, false },
        IpPropertyTestCase{ "",                 false, false, false, false, false, false, false, false, false, false, false }
    ),
    [](const testing::TestParamInfo<IpPropertyTestCase>& info) -> std::string {
        std::string name{ info.param.ipStr };
        if (name.empty()) "Empty";

        for (char& c : name)
            if (!std::isalnum(c)) c = '_';

        return "Ip_" + name;
    }
);

#pragma endregion

#pragma region Core behaviors & Conversions

struct IpAddressMiscTest : testing::Test {
    const IpAddress loopback4{ IpAddress::FromIpv4({ 127, 0, 0, 1 }) };
    const IpAddress public4  { IpAddress::FromIpv4({ 129, 0, 0, 1 }) };
    const IpAddress loopback6{ IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress link6    { IpAddress::FromIpv6({ 0xfe,0x80,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
};

TEST_F(IpAddressMiscTest, ConstructAndGetIp_HoldsCorrectVariantType) {
    EXPECT_TRUE(std::holds_alternative<IpAddress::Ipv4Type>(loopback4.GetIp()));
    EXPECT_TRUE(std::holds_alternative<IpAddress::Ipv6Type>(loopback6.GetIp()));

    EXPECT_EQ(loopback4.GetIp(), (IpAddress::IpVariant{ IpAddress::Ipv4Type{ 127, 0, 0, 1 } }));
}

TEST_F(IpAddressMiscTest, AsIpv6_FromIpv6_ReturnsSameBytes) {
    const IpAddress::Ipv6Type expected{ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 };
    EXPECT_EQ(loopback6.AsIpv6(), expected);
}

TEST_F(IpAddressMiscTest, AsIpv6_FromIpv4_HasIpv4MappedPrefixAndOctets) {
    const auto result{ loopback4.AsIpv6() };

    // Prefixo IPv4-mapped: 80 bits zero, 16 bits um (0xff)
    for (int i{}; i < 10; ++i) {
        EXPECT_EQ(result[i], 0) << "byte[" << i << "] should be 0";
    }

    EXPECT_EQ(result[10], 0xff);
    EXPECT_EQ(result[11], 0xff);

    // Final octets do IPv4
    EXPECT_EQ(result[12], 127);
    EXPECT_EQ(result[13],   0);
    EXPECT_EQ(result[14],   0);
    EXPECT_EQ(result[15],   1);
}

TEST_F(IpAddressMiscTest, AsIpv6_MappedAddressIsDetectedAsIpv4Mapped) {
    const IpAddress mapped{ IpAddress::FromIpv6(loopback4.AsIpv6()) };
    EXPECT_TRUE(mapped.IsIpv4Mapped());
}

#pragma endregion

#pragma region Comparisons & Hashing

TEST_F(IpAddressMiscTest, SpaceshipOperator_EqualityAndInequality) {
    const IpAddress a{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };
    const IpAddress b{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };

    EXPECT_EQ(a, b);
    EXPECT_NE(loopback4, public4);
}

TEST_F(IpAddressMiscTest, Hash_UsableAndDistinct) {
    const IpAddress a{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };
    const IpAddress b{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };

    EXPECT_EQ(std::hash<IpAddress>{}(a), std::hash<IpAddress>{}(b));
    EXPECT_NE(std::hash<IpAddress>{}(loopback4), std::hash<IpAddress>{}(public4));
    EXPECT_NE(std::hash<IpAddress>{}(loopback4), std::hash<IpAddress>{}(loopback6));
}

#pragma endregion

#pragma region Formatting

TEST_F(IpAddressMiscTest, Format_YieldsCorrectStringRepresentation) {
    EXPECT_EQ(std::format("{}", loopback4), "127.0.0.1");
    EXPECT_EQ(std::format("{}", public4), "129.0.0.1");

    EXPECT_EQ(std::format("{:}", loopback6), "::1");
    EXPECT_EQ(std::format("{:f}", loopback6), "0000:0000:0000:0000:0000:0000:0000:0001");
    EXPECT_EQ(std::format("{:}", link6), "fe80::1");
    EXPECT_EQ(std::format("{:b}", loopback6), "[::1]");
}

#pragma endregion