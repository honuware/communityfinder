#include "bigint_array_literal.h"

#include <gtest/gtest.h>

namespace TableHelpers {
namespace {

TEST(BigintArrayLiteralTest, JoinsIdsInOrder) {
    EXPECT_EQ(BigintArrayLiteral({3, 1, 2}), "{3,1,2}");
    EXPECT_EQ(BigintArrayLiteral({42}), "{42}");
}

TEST(BigintArrayLiteralTest, EmptyInputIsTheEmptyArray) {
    EXPECT_EQ(BigintArrayLiteral({}), "{}");
}

}  // namespace
}  // namespace TableHelpers
