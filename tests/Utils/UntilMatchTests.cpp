#include <gtest/gtest.h>
#include <Hermes/Utils/UntilMatch.hpp>
#include <string_view>
#include <string>
#include <vector>
#include <list>

using Hermes::Utils::ExclusiveUntilMatch;
using Hermes::Utils::InclusiveUntilMatch;
using Hermes::Utils::UntilMatch;
using Hermes::Utils::ExtractTo;

namespace rg = std::ranges;

#pragma region Parameterized Property Tests

struct UntilMatchTestCase {
    std::string_view name;
    std::string_view input;
    std::string_view pattern;
    std::string_view expectedExclusive;
    std::string_view expectedInclusive;
};

struct UntilMatchTest : testing::TestWithParam<UntilMatchTestCase> {};

TEST_P(UntilMatchTest, EvaluatesBothModesCorrectly) {
    const auto testCase{ GetParam() };

    std::string exclusiveResult{};
    exclusiveResult.assign_range(testCase.input | ExclusiveUntilMatch(testCase.pattern));
    EXPECT_EQ(exclusiveResult, testCase.expectedExclusive);

    std::string inclusiveResult{};
    inclusiveResult.assign_range(testCase.input | InclusiveUntilMatch(testCase.pattern));
    EXPECT_EQ(inclusiveResult, testCase.expectedInclusive);
}

INSTANTIATE_TEST_SUITE_P(
    VariousPatterns,
    UntilMatchTest,
    testing::Values(
        UntilMatchTestCase{ "HttpRequestLine", "GET / HTTP/1.1\r\nHost: loc", "\r\n"  , "GET / HTTP/1.1", "GET / HTTP/1.1\r\n" },
        UntilMatchTestCase{ "NoMatchAtAll"   , "hello-world"                , "\r\n"  , "hello-world"   , "hello-world"        },
        UntilMatchTestCase{ "MatchAtStart"   , "PREFIX-rest"                , "PREFIX", ""              , "PREFIX"             },

        UntilMatchTestCase{ "PatternBigger", "BIG"   , "BIGGER", "BIG", "BIG" },
        UntilMatchTestCase{ "EmptyInput"   , ""      , "filled", ""   , ""    },
        UntilMatchTestCase{ "EmptyPattern" , "filled", ""      , ""   , ""    },

        UntilMatchTestCase{ "MatchAtVeryEnd"            , "abc\r\n", "\r\n", "abc", "abc\r\n" },
        UntilMatchTestCase{ "PatternIsEntireInput"      , "\r\n"   , "\r\n", ""   , "\r\n"    },
        UntilMatchTestCase{ "SingleCharPattern"         , "a,b,c"  , ","   , "a"  , "a,"      },
        UntilMatchTestCase{ "FirstOfMultipleOccurrences", "aXbXcXd", "X"   , "a"  , "aX"      },

        UntilMatchTestCase{ "ThreeByteOverlappingPrefix", "aabaabX"         , "aabX", "aab", "aabaabX"   },
        UntilMatchTestCase{ "MultiByteBoundaryPattern"  , "0123\r\n4567\r\n", "\r\n", "0123", "0123\r\n" }
    ),
    [](const testing::TestParamInfo<UntilMatchTestCase>& info) { return std::string{ info.param.name }; }
);

#pragma endregion


#pragma region Default template argument (exclusive by default)

TEST(UntilMatchTest, DefaultsToExclusive) {
    using namespace std::literals::string_view_literals;

    std::string result{};
    result.assign_range("field:value\r\n"sv | UntilMatch(":"sv));

    EXPECT_EQ(result, "field");
}

#pragma endregion


#pragma region Container-agnostic behavior

TEST(UntilMatchTest, WorksOnVectorOfInts) {
    const std::vector input{ 1, 2, 3, 99, 98, 4, 5 };
    const std::vector pattern{ 99, 98 };

    std::vector<int> result{};
    result.assign_range(input | ExclusiveUntilMatch(pattern));

    EXPECT_EQ(result, (std::vector{ 1, 2, 3 }));
}

TEST(UntilMatchTest, WorksOnForwardListInput) {
    const std::list input{ 'h', 'i', '-', 't', 'h', 'e', 'r', 'e' };

    std::string result{};
    result.assign_range(input | ExclusiveUntilMatch(std::string_view{ "-" }));

    EXPECT_EQ(result, "hi");
}

#pragma endregion


#pragma region Iterator / sentinel protocol

TEST(UntilMatchTest, IsAnInputRange) {
    using View = Hermes::Utils::UntilMatchView<std::string_view, std::string_view, false>;
    static_assert(rg::input_range<View>);
    SUCCEED();
}

TEST(UntilMatchTest, BeginEqualsEndWhenPatternIsWholeInput) {
    using namespace std::literals::string_view_literals;

    auto view{ "\r\n"sv | ExclusiveUntilMatch("\r\n"sv) };
    EXPECT_EQ(view.begin(), view.end());
}

TEST(UntilMatchTest, BeginNotEqualsEndWhenContentPrecedesMatch) {
    using namespace std::literals::string_view_literals;

    auto view{ "a\r\n"sv | ExclusiveUntilMatch("\r\n"sv) };
    EXPECT_NE(view.begin(), view.end());
}

#pragma endregion


#pragma region ExtractTo

TEST(ExtractToTest, CopiesFullRangeIntoSizedContainer) {
    using namespace std::literals::string_view_literals;

    auto view{ "abc\r\ndef"sv | ExclusiveUntilMatch("\r\n"sv) };
    const auto result{ ExtractTo<std::string>(view) };

    EXPECT_EQ(result, "abc");
}

#pragma endregion