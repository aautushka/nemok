#include <gtest/gtest.h>

struct test : public ::testing::Test
{
};

TEST_F(test, failure)
{
	EXPECT_TRUE(false);
}
