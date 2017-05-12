#include <gtest/gtest.h>
#include "nemok/ev2.h"

struct ev2_echo_test : public ::testing::Test
{
	void start()
	{
		server.start(0);
		client = server.connect_client();
	}

	void stop()
	{
		server.stop();
		server.wait();
	}
	
	~ev2_echo_test()
	{
		stop();
	}

	nemok::ev2::server server;
	nemok::client client;
};

TEST_F(ev2_echo_test, communicates_with_echo_server)
{
	start();

	client.write_all("hello world", 11);

	EXPECT_EQ("hello world", nemok::read_all(client, 11));
}

TEST_F(ev2_echo_test, sends_multiple_requests_to_server)
{
	start();

	client.write_all("hello", 5);
	EXPECT_EQ("hello", nemok::read_all(client, 5));

	client.write_all("world", 5);
	EXPECT_EQ("world", nemok::read_all(client, 5));
}

TEST_F(ev2_echo_test, handles_multiple_connections)
{
	start();

	auto client2 = server.connect_client();
	
	client.write_all("hello world", 11);
	client2.write_all("hola mundo!", 11);

	EXPECT_EQ("hello world", nemok::read_all(client, 11));
	EXPECT_EQ("hola mundo!", nemok::read_all(client2, 11));
}

