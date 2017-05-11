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
	mock.when(http::GET("/")).reply(resp(200));
	
	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, matches_any_get_request)
{
	auto mock = nemok::start<http>();
	mock.when(http::GET()).reply(resp(200));
	
	auto client = mock.connect();
	http::send(client, "GET /hello/world HTTP/1.1\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, matches_any_unexpected_request)
{
	auto mock = nemok::start<http>();
	mock.when(http::unexpected()).reply(resp(500));
	
	auto client = mock.connect();
	http::send(client, "HEAD / HTTP/1.1\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 500 Internal Server Error\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, handles_multiple_requests)
{
	auto mock = nemok::start<http>();
	mock.when(http::GET("/foo")).reply(resp(200));
	mock.when(http::GET("/bar")).reply(resp(404));
	
	auto client = mock.connect();
	http::send(client, "GET /foo HTTP/1.1\r\n\r\n");
	http::send(client, "GET /bar HTTP/1.1\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
	EXPECT_EQ("HTTP/1.1 404 Not Found\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, matches_a_request_with_known_content)
{
	auto mock = nemok::start<http>();
	mock.when(http::GET().content("hello world")).reply(resp(200));
	mock.when(http::unexpected()).reply(resp(404));
	
	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, fails_to_match_content)
{
	auto mock = nemok::start<http>();
	mock.when(http::GET().content("hello")).reply(resp(200));
	mock.when(http::unexpected()).reply(resp(404));
	
	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 404 Not Found\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, matches_multiple_requests_having_payload)
{
	auto mock = nemok::start<http>();
	mock.when(http::POST()).reply(200);

	auto client = mock.connect();
	http::send(client, "POST / HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world");
	http::send(client, "POST / HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, matches_request_with_known_header)
{
	auto mock = nemok::start<http>();
	mock.when(http::GET().header("User-Agent", "curl")).reply(200);

	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\nUser-Agent: curl\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, fails_to_match_the_expected_http_header)
{
	auto mock = nemok::start<http>();
	
	// expect 'curl'
	mock.when(http::GET().header("User-Agent", "curl")).reply(200);
	mock.when_unexpected().reply(404);

	auto client = mock.connect();
	
	// send 'wget' - the expectation should fail
	http::send(client, "GET / HTTP/1.1\r\nUser-Agent: wget\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 404 Not Found\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, matches_multiple_headers)
{
	auto mock = nemok::start<http>();

	mock.when(http::GET()
			.header("User-Agent", "curl")
			.header("Accept-Encoding", "gzip"))
		.reply(200);

	mock.when_unexpected().reply(500);

	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\nUser-Agent: curl\r\nAccept-Encoding: gzip\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

TEST_F(http_mock_test, when_matching_http_headers_skips_over_unexpected_ones)
{
	auto mock = nemok::start<http>();
	mock.when(http::GET().header("User-Agent", "curl")).reply(200);

	auto client = mock.connect();
	http::send(client, "GET / HTTP/1.1\r\nUser-Agent: curl\r\nAccept-Encoding: gzip\r\n\r\n");

	EXPECT_EQ("HTTP/1.1 200 OK\r\n\r\n", http::receive(client));
}

