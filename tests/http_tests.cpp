#include <gtest/gtest.h>
#include <chrono>
#include "nemok/nemok.h"

struct http_mock_test : public ::testing::Test
{
	using req = nemok::http::request;
	using resp = nemok::http::response;
	using http = nemok::http;
};

TEST_F(http_mock_test, replies_to_a_request_according_to_specified_expectation)
{
	auto mock = nemok::start<http>();
	mock.when(http::get("/")).reply(resp(200));
	
	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));

}

