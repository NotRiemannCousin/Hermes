#include <gtest/gtest.h>

#include <Hermes/Utils/DropLast.hpp>
#include <Hermes/Utils/Hash.hpp>
#include <Hermes/Utils/Overloads.hpp>
#include <Hermes/Utils/ToBytes.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <variant>

using namespace Hermes::Utils;
namespace rg = std::ranges;
namespace vs = std::views;


#pragma region DropLast Tests

struct DropLastTest : testing::Test {};

TEST_F(DropLastTest, Adaptor_MultipleElements_DropsTheLastOne) {
    const std::vector<int> vec{ 1, 2, 3, 4 };
    auto view{ vec | dropLast };

    std::vector<int> result;
    result.assign_range(view);

    EXPECT_EQ(result, (std::vector<int>{ 1, 2, 3 }));
}

TEST_F(DropLastTest, Adaptor_SingleElement_ReturnsEmptyRange) {
    const std::vector<int> vec{ 42 };
    auto view{ vec | dropLast };

    EXPECT_TRUE(view.begin() == view.end());
}

TEST_F(DropLastTest, Adaptor_EmptyRange_ReturnsEmptyRangeSafe) {
    const std::vector<int> vec{};
    auto view{ vec | dropLast };

    EXPECT_TRUE(view.begin() == view.end());
}

TEST_F(DropLastTest, Adaptor_StringView_DropsLastChar) {
    using std::string_view_literals::operator""sv;
    const auto sv{ "Hermes!"sv };
    
    auto view{ sv | dropLast };
    
    std::string result;
    result.assign_range(view);

    EXPECT_EQ(result, "Hermes");
}

#pragma endregion


#pragma region Hash Tests

struct HashTest : testing::Test {};

struct UnhashableStruct {
    int x;
};

TEST_F(HashTest, HashableConcept_ValidTypes_EvaluatesTrue) {
    static_assert(Hashable<int>);
    static_assert(Hashable<std::string>);
    static_assert(Hashable<std::string_view>);
    SUCCEED();
}

TEST_F(HashTest, HashableConcept_InvalidType_EvaluatesFalse) {
    static_assert(!Hashable<UnhashableStruct>);
    SUCCEED();
}

TEST_F(HashTest, HashCombine_DifferentValues_ChangesSeed) {
    std::size_t seed1{ 0 };
    HashCombine(seed1, 42);
    EXPECT_NE(seed1, 0u);

    std::size_t seed2{ 0 };
    HashCombine(seed2, 43);
    EXPECT_NE(seed1, seed2);
}

TEST_F(HashTest, HashCombine_OrderOfOperationsMatters) {
    std::size_t seed1{ 0 };
    HashCombine(seed1, 1);
    HashCombine(seed1, 2);

    std::size_t seed2{ 0 };
    HashCombine(seed2, 2);
    HashCombine(seed2, 1);

    EXPECT_NE(seed1, seed2);
}

#pragma endregion


#pragma region Overloads Tests

struct OverloadsTest : testing::Test {};

TEST_F(OverloadsTest, Visit_Variant_CallsCorrectLambdaForType) {
    std::variant<int, std::string> v1{ 42 };
    std::variant<int, std::string> v2{ "hello" };

    auto visitor{ Overloaded{
        [](int i) { return i * 2; },
        [](const std::string& s) { return static_cast<int>(s.length()); }
    }};

    EXPECT_EQ(std::visit(visitor, v1), 84);
    EXPECT_EQ(std::visit(visitor, v2), 5);
}

#pragma endregion


#pragma region ToBytes Tests

struct ToBytesTest : testing::Test {};

TEST_F(ToBytesTest, StringView_ConvertsToSpanOfBytes) {
    using std::string_view_literals::operator""sv;
    const auto sv{ "ABC"sv }; 
    
    const auto span{ ToBytes(sv) };

    ASSERT_EQ(span.size(), 3u);
    EXPECT_EQ(span[0], static_cast<std::byte>('A'));
    EXPECT_EQ(span[1], static_cast<std::byte>('B'));
    EXPECT_EQ(span[2], static_cast<std::byte>('C'));
}

TEST_F(ToBytesTest, VectorOfChar_ConvertsToSpanOfBytes) {
    const std::vector<char> vec{ 'x', 'y' };
    const auto span{ ToBytes(vec) };

    ASSERT_EQ(span.size(), 2u);
    EXPECT_EQ(span[0], static_cast<std::byte>('x'));
    EXPECT_EQ(span[1], static_cast<std::byte>('y'));
}

#pragma endregion


#pragma region UntilMatch Tests

struct UntilMatchTest : testing::Test {
    static constexpr std::string_view crlf{ "\r\n" };
    static constexpr std::string_view prefix{ "PREFIX" };
};

TEST_F(UntilMatchTest, Exclusive_MatchFound_YieldsElementsUntilPattern) {
    using std::string_view_literals::operator""sv;
    constexpr auto text{ "GET / HTTP/1.1\r\nHost: loc"sv };
    
    std::string result;
    result.assign_range(text | ExclusiveUntilMatch(crlf));
    
    EXPECT_EQ(result, "GET / HTTP/1.1"sv);
}

TEST_F(UntilMatchTest, Inclusive_MatchFound_YieldsElementsIncludingPattern) {
    using std::string_view_literals::operator""sv;
    constexpr auto text{ "GET / HTTP/1.1\r\nHost: loc"sv };
    
    std::string result;
    result.assign_range(text | InclusiveUntilMatch(crlf));
    
    EXPECT_EQ(result, "GET / HTTP/1.1\r\n"sv);
}

TEST_F(UntilMatchTest, NoMatch_YieldsEntireRange) {
    using std::string_view_literals::operator""sv;
    constexpr auto text{ "hello-world"sv };
    
    std::string result;
    result.assign_range(text | ExclusiveUntilMatch(crlf));
    
    EXPECT_EQ(result, "hello-world"sv);
}

TEST_F(UntilMatchTest, Inclusive_MatchAtVeryBeginning_YieldsOnlyPattern) {
    using std::string_view_literals::operator""sv;
    constexpr auto text{ "PREFIX-rest-of-data"sv };
    
    auto view{ text | InclusiveUntilMatch(prefix) };
    
    std::string result;
    result.assign_range(view);
    
    EXPECT_EQ(result, "PREFIX"sv);
}

TEST_F(UntilMatchTest, Exclusive_MatchAtVeryBeginning_YieldsEmpty) {
    using std::string_view_literals::operator""sv;
    constexpr auto text{ "PREFIX-rest-of-data"sv };
    
    std::string result;
    result.assign_range(text | ExclusiveUntilMatch(prefix));
    
    EXPECT_TRUE(result.empty());
}

#pragma endregion