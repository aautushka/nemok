#pragma once

#include <thread>
#include <atomic>
#include <stdexcept>
#include <future>

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
	mock.when(GET).reply("200 OK");
	mock.when(GET).reply(200_ok);
	mock.when(GET("/").reply(200);
	mock.when(POST).reply(500);

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
	auto mock = nemok::start<nemok::memcached>();
*/

namespace nemok
{

class exception : public std::exception
{
};

class network_error : public exception
{
};

class already_running : public exception
{
};

class server_is_down : public exception
{
};

class already_connected : public exception
{
};

class not_connected : public exception
{
};

// a very simple tcp/ip client
class client
{
public:
	using port_t = uint16_t;

	client();
	~client();
	client(const client&) = delete;
	client& operator =(const client&) = delete;

	client(client&& rhs);
	client& operator =(client&& rhs);

	void connect(port_t port);

	void close();
	size_t read(void* buffer, size_t length);
	size_t write(const void* buffer, size_t length);
	bool connected() const;

	void write_all(const void* buffer, size_t length);
	void read_all(void* buffer, size_t length);

	void assign(int df);

private:
	int _sock;
};

// a primitive tcp/ip server
class server
{
public:
	using port_t = uint16_t;

	server();
	virtual ~server();

	// if zero is passed, we will pick the next free port automatically
	// the function exits once the server is ready to accept connections
	port_t start(port_t port = 0);

	// close existing connections, stop accepting new ones
	// the server may be restarted as many times as needed
	void stop();

	bool running() const;
	port_t port() const;
	int open_connections();

private:
	void run_server(std::promise<void> ready);
	void run_client(int sock);
	virtual void serve_client(client c) = 0;
	void accept_connections(int sock, std::function<void(int)> handler);
	void bind_server_socket(int sock);
	void set_socket_opts(int sock);

	std::thread _server_thread;
	port_t _server_port;
	std::atomic<port_t> _effective_port;
	std::atomic<bool> _terminate_server_flag;
	bool _server_running;
};

client connect_client(const server& server);
std::string read_string_from_client(client& cl, size_t len);


// echo server
class echo_server : public server
{
private:
	virtual void serve_client(client c);
};

} // namespace nemok
