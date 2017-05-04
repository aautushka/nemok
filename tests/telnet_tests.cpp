#include <gtest/gtest.h>
#include "nemok/nemok.h"

struct telnet_mock_test : public ::testing::Test
{
	using telnet = nemok::telnet;
};

TEST_F(telnet_mock_test, communicates_with_echo_server)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello world").reply("hola mundo");
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_EQ("hola mundo", nemok::read_all(client, 10));

}

TEST_F(telnet_mock_test, cant_read_anything_from_client_because_of_connection_shutted_down)
{
	return;
	auto mock = nemok::start<telnet>();
	mock.when("hello world").shutdown();
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_EQ("", nemok::read_all(client, 10));

}

TEST_F(telnet_mock_test, fulfils_multiple_expectations)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello ").reply("hola ");
	mock.when("world").reply("mundo");
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_EQ("hola mundo", nemok::read_all(client, 10));
}

TEST_F(telnet_mock_test, disregards_the_order)
{
	auto mock = nemok::start<telnet>();
	mock.when("world").reply("mundo");
	mock.when("hello ").reply("hola ");
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_EQ("hola mundo", nemok::read_all(client, 10));

}

TEST_F(telnet_mock_test, fires_same_expectation_multiple_times)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello").reply("hola");

	auto client = mock.connect();
	client.write("hellohello", 10);

	EXPECT_EQ("holahola", nemok::read_all(client, 8));
}
