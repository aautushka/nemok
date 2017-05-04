#pragma once

#include <thread>
#include <atomic>
#include <stdexcept>
#include <future>
#include <list>
#include <vector>
#include <memory>

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
	ssize_t read(void* buffer, size_t length);
	ssize_t write(const void* buffer, size_t length);
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

	server(server&) = delete;
	server& operator =(server&) = delete;

	// if zero is passed, we will pick the next free port automatically
	// the function exits once the server is ready to accept connections
	port_t start(port_t port = 0);

	// close existing connections, stop accepting new ones
	// the server may be restarted as many times as needed
	void stop();
	void wait();

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
	std::atomic<bool> _server_running;
};

client connect_client(const server& server);
std::string read_all(client& cl, size_t len);
std::string read_some(client& cl, size_t len);


// echo server
class echo : public server
{
private:
	virtual void serve_client(client c);
};

class telnet : public server
{
public:
	telnet();

	telnet& when(std::string input);
	telnet& reply(std::string output);
	telnet& shutdown();

private:
	virtual void serve_client(client c);

	using action = std::function<void(client&)>;
	using expectation = std::pair<std::string, action>;
	using expect_list = std::list<expectation>;

	expect_list _expectations;
	std::vector<uint8_t> _input;
	std::vector<uint8_t> _buffer;
};

template <typename T>
class mock
{
public:
	mock()
	{
		t.reset(new T);
		t->start();
	}

	~mock()
	{
		if (t)
		{
			t->stop();
			t->wait();
		}
	}

	mock(mock& rhs) = delete;
	mock& operator =(mock& rhs) = delete;

	mock(mock&& rhs)
	{
		*this = std::move(rhs);
	}

	mock& operator =(mock&& rhs) 
	{
		t = std::move(rhs.t);
		return *this;
	}

	client connect()
	{
		return connect_client(*t);
	}

	T& when(std::string input)
	{
		return t->when(input);
	}

	T& reply(std::string output)
	{
		return t->reply(output);
	}

private:
	std::unique_ptr<T> t;
};

template <typename T>
mock<T> start()
{
	return std::move(mock<T>());
}

} // namespace nemok
