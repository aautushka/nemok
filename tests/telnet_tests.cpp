#include <gtest/gtest.h>
#include "nemok/nemok.h"

struct telnet_mock_test : public ::testing::Test
{
};

TEST_F(telnet_mock_test, communicates_with_echo_server)
{
	//auto mock = nemok::start<telnet>();
}

