#include <gtest/gtest.h>
#include <chrono>
#include "nemok/nemok.h"

struct telnet_mock_test : public ::testing::Test
{
	using telnet = nemok::telnet;

	std::chrono::microseconds measure(std::function<void(void)> f)
	{
		auto before = std::chrono::steady_clock::now();
		f();
		auto after = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::microseconds>(after - before);
	}
};

TEST_F(telnet_mock_test, communicates_with_echo_server)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello world").reply("hola mundo");
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_EQ("hola mundo", nemok::read_all(client, 10));

}

TEST_F(telnet_mock_test, cant_read_anything_from_client_because_of_server_shut_down)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello world").shutdown_server();
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_THROW(nemok::read_all(client, 10), nemok::network_error);

}

TEST_F(telnet_mock_test, fulfills_multiple_expectations)
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

TEST_F(telnet_mock_test, freezes_the_server)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello").freeze(100000).reply("hola");

	auto client = mock.connect();

	auto duration= measure([&]()
	{ 
		client.write("hello", 5);
		nemok::read_all(client, 4);
	});

	EXPECT_GE(duration, std::chrono::microseconds(100000));
}

TEST_F(telnet_mock_test, fires_previously_unmatches_expectation)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello").reply("hola!");
	mock.when("hello").reply("hels!");
	
	auto client = mock.connect();
	client.write("hellohello", 10);

	EXPECT_EQ("hola!hels!", nemok::read_all(client, 10));
}

TEST_F(telnet_mock_test, fires_expectations_in_predictable_order)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello").reply("+");
	mock.when("hello").reply("-");
	
	auto client = mock.connect();
	client.write("hellohellohellohello", 20);

	EXPECT_EQ("+-+-", nemok::read_all(client, 4));
}

TEST_F(telnet_mock_test, excludes_once_matched_expectation)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello").reply_once("+");
	mock.when("hello").reply("-");
	
	auto client = mock.connect();
	client.write("hellohellohellohello", 20);

	EXPECT_EQ("+---", nemok::read_all(client, 4));
}

TEST_F(telnet_mock_test, cant_read_anything_from_client_because_of_connection_being_closed)
{
	auto mock = nemok::start<telnet>();
	mock.when("hello world").close_connection();
	
	auto client = mock.connect();
	client.write("hello world", 11);

	EXPECT_THROW(nemok::read_all(client, 10), nemok::network_error);
}

TEST_F(telnet_mock_test, calls_a_specific_expectation_the_exact_number_of_times)
{
	auto mock = nemok::start<telnet>();
	mock.when("A").reply("+").times(2);
	mock.when("A").reply("-");
	
	auto client = mock.connect();
	client.write("AAAAAA", 6);

	EXPECT_EQ("+-+---", nemok::read_all(client, 6));
}
