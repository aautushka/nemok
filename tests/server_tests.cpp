#include <gtest/gtest.h>
#include "nemok/nemok.h"

template <int N>
class one_shot_echo : public nemok::server
{
private:
	virtual void serve_client(nemok::client& c)
	{
		std::vector<uint8_t> buffer(N);
		c.read_all(&buffer[0], N);
		c.write_all(&buffer[0], N);
	}
};

struct server_test : public ::testing::Test
{
	void start()
	{
		server.start();
		client = std::move(nemok::connect_client(server));
	}

	void stop()
	{
		server.stop();
		server.wait();
	}
	
	~server_test()
	{
		stop();
	}

	one_shot_echo<11> server;
	nemok::client client;
};

TEST_F(server_test, communicates_with_echo_server)
{
	start();
	client.write_all("hello world", 11);

	EXPECT_EQ("hello world", nemok::read_all(client, 11));
}

TEST_F(server_test, starts_server_using_the_mock_object)
{
	auto mock = nemok::start<one_shot_echo<11>>();
	auto client = mock.connect();
	
	client.write_all("hello world", 11);

	EXPECT_EQ("hello world", nemok::read_all(client, 11));
}

TEST_F(server_test, handles_multiple_connections)
{
	start();

	auto client2 = nemok::connect_client(server);
	
	client.write_all("hello world", 11);
	client2.write_all("hola mundo!", 11);

	EXPECT_EQ("hello world", nemok::read_all(client, 11));
	EXPECT_EQ("hola mundo!", nemok::read_all(client2, 11));
}

TEST_F(server_test, talks_to_server_after_restart)
{
	start();
	client.write_all("hello world", 11);
	stop();

	start();
	client.write_all("hola mundo!", 11);

	EXPECT_EQ("hola mundo!", nemok::read_all(client, 11));
}
