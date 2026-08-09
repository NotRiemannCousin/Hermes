#include <gtest/gtest.h>
#include <Hermes/Utils/DropLast.hpp>

#include <string>
#include <vector>
#include <deque>
#include <list>
#include <ranges>

namespace rg = std::ranges;
namespace vs = std::views;

using Hermes::Utils::dropLast;

#pragma region Parameterized: element-count behavior

struct DropLastTestCase {
    std::string name;
    std::string input;
    std::string expected;
};

struct DropLastTest : testing::TestWithParam<DropLastTestCase> {};

TEST_P(DropLastTest, DropsExactlyTheLastElement) {
    const auto testCase{ GetParam() };

    std::string result{};
    result.assign_range(testCase.input | dropLast);

    EXPECT_EQ(result, testCase.expected);
}

INSTANTIATE_TEST_SUITE_P(
    VariousLengths,
    DropLastTest,
    testing::Values(
        DropLastTestCase{ "Empty"        ,  ""      , ""       },
        DropLastTestCase{ "SingleElement", "a"      , ""       },
        DropLastTestCase{ "TwoElements"  , "ab"     , "a"      },
        DropLastTestCase{ "ThreeElements", "abc"    , "ab"     },
        DropLastTestCase{ "ManyElements" , "abcdefg", "abcdef" }
    ),
    [](const testing::TestParamInfo<DropLastTestCase>& info) { return info.param.name; }
);

#pragma endregion


#pragma region Container-agnostic behavior

TEST(DropLastTest, WorksOnVector) {
    const std::vector input{ 1, 2, 3, 4 };

    std::vector<int> result{};
    result.assign_range(input | dropLast);

    EXPECT_EQ(result, (std::vector{ 1, 2, 3 }));
}

TEST(DropLastTest, WorksOnDeque) {
    const std::deque input{ 10, 20, 30 };

    std::vector<int> result{};
    result.assign_range(input | dropLast);

    EXPECT_EQ(result, (std::vector{ 10, 20 }));
}

TEST(DropLastTest, WorksOnForwardListLikeInputRange) {
    const std::list input{ 5, 6, 7 };

    std::vector<int> result{};
    result.assign_range(input | dropLast);

    EXPECT_EQ(result, (std::vector{ 5, 6 }));
}

TEST(DropLastTest, EmptyVectorYieldsEmpty) {
    const std::vector<int> input{};

    std::vector<int> result{};
    result.assign_range(input | dropLast);

    EXPECT_TRUE(result.empty());
}

TEST(DropLastTest, SingleElementVectorYieldsEmpty) {
    const std::vector input{ 42 };

    std::vector<int> result{};
    result.assign_range(input | dropLast);

    EXPECT_TRUE(result.empty());
}

#pragma endregion


#pragma region Composability with other range adaptors

TEST(DropLastTest, ComposesWithTransform) {
    const std::vector input{ 1, 2, 3, 4 };

    std::vector<int> result{};
    result.assign_range(input | vs::transform([](const int v) { return v * 10; }) | dropLast);

    EXPECT_EQ(result, (std::vector{ 10, 20, 30 }));
}

TEST(DropLastTest, ComposesWithFilterThenDrop) {
    const std::vector input{ 1, 2, 3, 4, 5, 6 };

    std::vector<int> result{};
    result.assign_range(input | vs::filter([](const int v) { return v % 2 == 0; }) | dropLast);

    // Evens: 2, 4, 6 -> drop last -> 2, 4
    EXPECT_EQ(result, (std::vector{ 2, 4 }));
}

TEST(DropLastTest, DoubleDropLastRemovesLastTwo) {
    const std::vector input{ 1, 2, 3, 4, 5 };

    std::vector<int> result{};
    result.assign_range(input | dropLast | dropLast);

    EXPECT_EQ(result, (std::vector{ 1, 2, 3 }));
}

TEST(DropLastTest, ManualFunctionCallSyntaxMatchesPipeSyntax) {
    const std::vector input{ 1, 2, 3 };

    std::vector<int> viaPipe{};
    viaPipe.assign_range(input | dropLast);

    std::vector<int> viaCall{};
    viaCall.assign_range(Hermes::Utils::DropLastAdaptor{}(input));

    EXPECT_EQ(viaPipe, viaCall);
}

#pragma endregion


#pragma region Sentinel / iterator protocol correctness

TEST(DropLastTest, BeginEqualsEndOnEmptyRange) {
    const std::vector<int> input{};
    auto view{ input | dropLast };

    EXPECT_EQ(view.begin(), view.end());
}

TEST(DropLastTest, BeginNotEqualsEndWhenElementsRemain) {
    const std::vector input{ 1, 2 };
    auto view{ input | dropLast };

    EXPECT_NE(view.begin(), view.end());
}

TEST(DropLastTest, IsAnInputRange) {
    static_assert(rg::input_range<Hermes::Utils::DropLastView<std::vector<int>>>);
    SUCCEED();
}

TEST(DropLastTest, PostIncrementBehavesLikePreIncrement) {
    const std::vector input{ 1, 2, 3 };
    auto view{ input | dropLast };

    auto it{ view.begin() };
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
}

#pragma endregion