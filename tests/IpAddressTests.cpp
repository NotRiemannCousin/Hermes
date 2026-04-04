#include <gtest/gtest.h>
#include <Hermes/Endpoint/IpEndpoint/IpAddress.hpp>

using Hermes::IpAddress;


struct IpAddressTest : testing::Test {
    // IPv4
    const IpAddress loopback4    { IpAddress::FromIpv4({ 127,   0,   0,   1 }) };
    const IpAddress private4     { IpAddress::FromIpv4({  10,   0,   0,   1 }) };
    const IpAddress siteLocal4   { IpAddress::FromIpv4({ 172,  16,   0,   1 }) };
    const IpAddress linkLocal4   { IpAddress::FromIpv4({ 169, 254,   0,   1 }) };
    const IpAddress public4      { IpAddress::FromIpv4({ 129,   0,   0,   1 }) };
    const IpAddress multicast4   { IpAddress::FromIpv4({ 224,   0,   0,   1 }) };
    const IpAddress broadcast4   { IpAddress::FromIpv4({ 255, 255, 255, 255 }) };
    const IpAddress cgnat4       { IpAddress::FromIpv4({ 100,  64,   0,   1 }) };

    // IPv6
    const IpAddress loopback6      { IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress unspecified6   { IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }) };
    const IpAddress linkLocal6     { IpAddress::FromIpv6({ 0xfe,0x80,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress uniqueLocal6   { IpAddress::FromIpv6({ 0xfd,0x0a,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress siteLocal6     { IpAddress::FromIpv6({ 0xfe,0xc0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress ipv4Mapped6    { IpAddress::FromIpv6({ 0,0,0,0, 0,0,0,0, 0,0,0xff,0xff, 10,0,0,1 }) };
    const IpAddress documentation6 { IpAddress::FromIpv6({ 0x20,0x01,0x0d,0xb8, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress multicast6     { IpAddress::FromIpv6({ 0xff,0x02,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
    const IpAddress globalUnicast6 { IpAddress::FromIpv6({ 0x81,0x00,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }) };
};


#pragma region Construction & version detection

TEST_F(IpAddressTest, ConstructFromIpv4_HoldsIpv4Variant) {
    EXPECT_TRUE(std::holds_alternative<IpAddress::Ipv4Type>(loopback4.GetIp()));
}

TEST_F(IpAddressTest, ConstructFromIpv6_HoldsIpv6Variant) {
    EXPECT_TRUE(std::holds_alternative<IpAddress::Ipv6Type>(loopback6.GetIp()));
}

TEST_F(IpAddressTest, IsIpv4_TrueForIpv4) {
    EXPECT_TRUE(loopback4.IsIpv4());
    EXPECT_FALSE(loopback4.IsIpv6());
}

TEST_F(IpAddressTest, IsIpv6_TrueForIpv6) {
    EXPECT_TRUE(loopback6.IsIpv6());
    EXPECT_FALSE(loopback6.IsIpv4());
}

TEST_F(IpAddressTest, GetIp_ReturnsCorrectIpv4Value) {
    EXPECT_EQ(loopback4.GetIp(),
              (IpAddress::IpVariant{ IpAddress::Ipv4Type{ 127, 0, 0, 1 } }));
}

TEST_F(IpAddressTest, GetIp_ReturnsCorrectIpv6Value) {
    EXPECT_EQ(loopback6.GetIp(),
              (IpAddress::IpVariant{ IpAddress::Ipv6Type{ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 } }));
}

#pragma endregion

#pragma region Parsing

TEST_F(IpAddressTest, TryParse_ValidIpv4) {
    const auto result{ IpAddress::TryParse("127.0.0.1") };
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->IsIpv4());
    EXPECT_TRUE(result->IsLoopback());
}

TEST_F(IpAddressTest, TryParse_ValidIpv6) {
    const auto result{ IpAddress::TryParse("::ffff:0:0") };
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->IsIpv6());
}

TEST_F(IpAddressTest, TryParse_FullIpv6) {
    const auto result{ IpAddress::TryParse("2001:db8::1") };
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->IsIpv6());
}

TEST_F(IpAddressTest, TryParse_InvalidString_ReturnsNullopt) {
    EXPECT_FALSE(IpAddress::TryParse("Certainly not an IP").has_value());
}

TEST_F(IpAddressTest, TryParse_EmptyString_ReturnsNullopt) {
    EXPECT_FALSE(IpAddress::TryParse("").has_value());
}

#pragma endregion

#pragma region Loopback

TEST_F(IpAddressTest, IsLoopback_Ipv4Loopback) {
    EXPECT_TRUE(loopback4.IsLoopback());
}

TEST_F(IpAddressTest, IsLoopback_Ipv6Loopback) {
    EXPECT_TRUE(loopback6.IsLoopback());
}

TEST_F(IpAddressTest, IsLoopback_PublicIpv4_ReturnsFalse) {
    EXPECT_FALSE(public4.IsLoopback());
}

#pragma endregion

#pragma region Public / Private

TEST_F(IpAddressTest, IsPublic_PublicIpv4) {
    EXPECT_TRUE(public4.IsPublic());
}

TEST_F(IpAddressTest, IsPrivate_Rfc1918_10Block) {
    EXPECT_TRUE(private4.IsPrivate());
}

TEST_F(IpAddressTest, IsPrivate_Rfc1918_172Block) {
    EXPECT_TRUE(siteLocal4.IsPrivate());
}

TEST_F(IpAddressTest, IsPrivate_Rfc1918_192Block) {
    const IpAddress ip{ IpAddress::FromIpv4({ 192, 168, 1, 1 }) };
    EXPECT_TRUE(ip.IsPrivate());
}

TEST_F(IpAddressTest, IsPrivate_Cgnat_100_64Block) {
    EXPECT_TRUE(cgnat4.IsPrivate());
}

TEST_F(IpAddressTest, IsPublic_GlobalUnicastIpv6) {
    EXPECT_TRUE(globalUnicast6.IsPublic());
}

TEST_F(IpAddressTest, IsPrivate_UniqueLocalIpv6) {
    EXPECT_TRUE(uniqueLocal6.IsPrivate());
}

#pragma endregion

#pragma region Multicast

TEST_F(IpAddressTest, IsMulticast_Ipv4MulticastRange) {
    EXPECT_TRUE(multicast4.IsMulticast());
}

TEST_F(IpAddressTest, IsMulticast_Ipv4Broadcast) {
    EXPECT_TRUE(broadcast4.IsMulticast());
}

TEST_F(IpAddressTest, IsMulticast_Ipv6) {
    EXPECT_TRUE(multicast6.IsMulticast());
}

TEST_F(IpAddressTest, IsMulticast_PublicIpv4_ReturnsFalse) {
    EXPECT_FALSE(public4.IsMulticast());
}

#pragma endregion

#pragma region Link-local

TEST_F(IpAddressTest, IsLinkLocal_Ipv4_169_254) {
    EXPECT_TRUE(linkLocal4.IsLinkLocal());
}

TEST_F(IpAddressTest, IsLinkLocal_Ipv6_Fe80) {
    EXPECT_TRUE(linkLocal6.IsLinkLocal());
}

TEST_F(IpAddressTest, IsLinkLocal_PublicIpv4_ReturnsFalse) {
    EXPECT_FALSE(public4.IsLinkLocal());
}

#pragma endregion

#pragma region Site-local

TEST_F(IpAddressTest, IsSiteLocal_Ipv4_172Block) {
    EXPECT_TRUE(siteLocal4.IsSiteLocal());
}

TEST_F(IpAddressTest, IsSiteLocal_Ipv6_FecBlock) {
    EXPECT_TRUE(siteLocal6.IsSiteLocal());
}

TEST_F(IpAddressTest, IsSiteLocal_PublicIpv4_ReturnsFalse) {
    EXPECT_FALSE(public4.IsSiteLocal());
}

#pragma endregion

#pragma region Unspecified

TEST_F(IpAddressTest, IsUnspecified_Ipv6AllZeroes) {
    EXPECT_TRUE(unspecified6.IsUnspecified());
}

TEST_F(IpAddressTest, IsUnspecified_Ipv4AllZeroes) {
    const IpAddress unspecified4{ IpAddress::FromIpv4({ 0, 0, 0, 0 }) };
    EXPECT_TRUE(unspecified4.IsUnspecified());
}

TEST_F(IpAddressTest, IsUnspecified_NonZeroIpv4_ReturnsFalse) {
    EXPECT_FALSE(loopback4.IsUnspecified());
}

TEST_F(IpAddressTest, IsUnspecified_NonZeroIpv6_ReturnsFalse) {
    EXPECT_FALSE(loopback6.IsUnspecified());
}

#pragma endregion

#pragma region IPv4-mapped IPv6

TEST_F(IpAddressTest, IsIpv4Mapped_CorrectPrefix) {
    EXPECT_TRUE(ipv4Mapped6.IsIpv4Mapped());
}

TEST_F(IpAddressTest, IsIpv4Mapped_RegularIpv4_ReturnsFalse) {
    EXPECT_FALSE(loopback4.IsIpv4Mapped());
}

TEST_F(IpAddressTest, IsIpv4Mapped_RegularIpv6_ReturnsFalse) {
    EXPECT_FALSE(loopback6.IsIpv4Mapped());
}

#pragma endregion

#pragma region Documentation

TEST_F(IpAddressTest, IsDocumentation_2001Db8Prefix) {
    EXPECT_TRUE(documentation6.IsDocumentation());
}

TEST_F(IpAddressTest, IsDocumentation_RegularIpv6_ReturnsFalse) {
    EXPECT_FALSE(globalUnicast6.IsDocumentation());
}

TEST_F(IpAddressTest, IsDocumentation_Ipv4_ReturnsFalse) {
    EXPECT_FALSE(public4.IsDocumentation());
}

#pragma endregion

#pragma region Comparisons & hashing

TEST_F(IpAddressTest, SpaceshipOperator_EqualAddresses) {
    const IpAddress a{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };
    const IpAddress b{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };
    EXPECT_EQ(a, b);
}

TEST_F(IpAddressTest, SpaceshipOperator_DifferentAddresses) {
    EXPECT_NE(loopback4, public4);
}

TEST_F(IpAddressTest, Hash_SameAddress_SameHash) {
    const IpAddress a{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };
    const IpAddress b{ IpAddress::FromIpv4({ 10, 0, 0, 1 }) };
    EXPECT_EQ(std::hash<IpAddress>{}(a), std::hash<IpAddress>{}(b));
}

TEST_F(IpAddressTest, Hash_DifferentAddresses_DifferentHash) {
    // Not guaranteed by spec but should hold for non-pathological inputs
    EXPECT_NE(std::hash<IpAddress>{}(loopback4), std::hash<IpAddress>{}(public4));
}

#pragma endregion

#pragma region Formatting

TEST_F(IpAddressTest, Format_Ipv4) {
    EXPECT_EQ(std::format("{}", loopback4), "127.0.0.1");
}

TEST_F(IpAddressTest, Format_Ipv4_Public) {
    EXPECT_EQ(std::format("{}", public4), "129.0.0.1");
}

TEST_F(IpAddressTest, Format_Ipv6_Reduced) {
    // ::1 in reduced form
    EXPECT_EQ(std::format("{:f}", loopback6), "::1");
}

TEST_F(IpAddressTest, Format_Ipv6_Full) {
    EXPECT_EQ(std::format("{}", loopback6), "0000:0000:0000:0000:0000:0000:0000:0001");
}

#pragma endregion