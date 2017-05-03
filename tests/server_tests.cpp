#include <gtest/gtest.h>
#include "nemok/nemok.h"

template <int N>
class one_shot_echo : public nemok::server
{
private:
	virtual void serve_client(nemok::client c)
	{
		std::vector<uint8_t> buffer(N);
		size_t received = c.read(&buffer[0], N);
		assert(received == N);

		size_t sent = c.write(&buffer[0], N);
		assert(sent == N);
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
	client.write("hello world", 11);

	EXPECT_EQ("hello world", nemok::read_string_from_client(client, 11));
}
