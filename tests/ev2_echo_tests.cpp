#include <gtest/gtest.h>
#include "nemok/ev2.h"

struct ev2_echo_test : public ::testing::Test
{
	void start()
	{
		server.start(0);
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
	auto client = server.connect_client();
	client.write_all("hello world", 11);

	EXPECT_EQ("hello world", nemok::read_all(client, 11));
}

