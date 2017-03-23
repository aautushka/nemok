#pragma once

#include <thread>
#include <atomic>

/*
	auto mock = nemok::start<nemok::http>();

	mock.on(request("GET /").reply("500 Server Error"); 
	mock.when(request("POST /").reply("200 OK);
	mock.when(content("hello world")).reply("200 OK");
	mock.when(request().with_content("hello world").reply("200 OK"));
	mock.when(request("GET /")).hang();
	mock.when(request("GET /")).crash();
	mock.when(request("GET /")).close_connection();
	mock.when(request("GET /")).crawl(200ms);
	mock.when(unexpected()).reply("500 Error");
	mock.when(unexpected()).crash();

	mock.expect(request("GET /").reply("200 OK");
	
	mock
		.when(request().with_header("Host: google.com")).reply("200 OK")
		.when(request().with_header("Host: yahoo.com")).reply("500 Error")
		.when(request().with_header("Host: bing.com")).reply("404 Not Found");

	mock
		.when(request().with_header("Host: google.com").reply("200 OK)
		.when_not().reply("500 Error);	
		
 
	auto mock = nemok::trace<nemok::http>();
	auto mock = nemok::start<nemok::https>();
	auto mock = nemok::start<nemok::nats>();
*/

namespace nemok
{
// a primitive tcp/ip server
class server
{
public:
	using port_t = unsigned short;

	server();
	virtual ~server();

	// if zero is passed, we will pick the next free port automatically
	// the function exits once the server is ready to accept connections
	port_t start(port_t port = 0);

	// close existing connections, stop accepting new ones
	// the server may be restarted as many times as needed
	void stop();

	bool running();
	port_t port() const;
	int open_connections();

protected:

private:
	void run_server();

	std::thread _server_thread;
	std::atomic<bool> _server_running;
};

// http server
class http : public server
{
public:
};

} // namespace nemok
