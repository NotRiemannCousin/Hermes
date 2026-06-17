#include <gtest/gtest.h>
#include <Hermes/Utils/UntilMatch.hpp>
#include <string_view>
#include <string>

using Hermes::Utils::ExclusiveUntilMatch;
using Hermes::Utils::InclusiveUntilMatch;

struct UntilMatchTestCase {
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
        UntilMatchTestCase{ "GET / HTTP/1.1\r\nHost: loc", "\r\n", "GET / HTTP/1.1", "GET / HTTP/1.1\r\n" },
        UntilMatchTestCase{ "hello-world",                 "\r\n", "hello-world",    "hello-world" },
        UntilMatchTestCase{ "PREFIX-rest",                 "PREFIX", "",             "PREFIX" }
    )
);