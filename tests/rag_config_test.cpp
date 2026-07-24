#include <gtest/gtest.h>

#include "rag/rag_config.hpp"

using ragcli::rag::pick_first_positive_int;

TEST(PickFirstPositiveInt, PrefersCliWhenPositive) {
    EXPECT_EQ(pick_first_positive_int(10, 20, 30), 10);
}

TEST(PickFirstPositiveInt, FallsBackToEnvWhenCliIsZeroOrNegative) {
    EXPECT_EQ(pick_first_positive_int(0, 20, 30), 20);
    EXPECT_EQ(pick_first_positive_int(-5, 20, 30), 20);
}

TEST(PickFirstPositiveInt, FallsBackToDefaultWhenNothingIsPositive) {
    EXPECT_EQ(pick_first_positive_int(0, 0, 30), 30);
    EXPECT_EQ(pick_first_positive_int(-1, -1, 30), 30);
}
