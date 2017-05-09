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
	mock.when(request("GET /")).freeze();
	mock.when(request("GET /")).shutdown_server();
	mock.when(request("GET /")).close_connection();
	mock.when(unexpected()).reply("500 Error");
	mock.when(unexpected()).shutdown_server();
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
public:
	exception() : _message("") {} 
	explicit exception(const char* message) : _message(message) {}
	virtual const char* what() const noexcept { return _message; }
private:
	const char* _message;
};

class network_error : public exception
{
public:
	network_error() : exception("network error"), _errno(errno) {}
	explicit network_error(const char* message) : exception(message), _errno(errno) {}

private:
	int _errno;
};

class already_running : public exception
{
public:
	already_running() : exception("server already running") {}
};

class server_is_down : public exception
{
public:
	server_is_down() : exception("server is down") {}
};

class already_connected : public exception
{
public:
	already_connected() : exception("client is already connected") {}
};

class not_connected : public exception
{
public:
	not_connected() : exception("client is not connected") {}
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
	void disconnect();
	void shutdown();
	void assign(int df);

	ssize_t read(void* buffer, size_t length);
	ssize_t write(const void* buffer, size_t length);
	bool connected() const;

	void write_all(const void* buffer, size_t length);
	void read_all(void* buffer, size_t length);

private:
	int _sock = -1;
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
	void run_client(client& s);
	virtual void serve_client(client& c) = 0;
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
	virtual void serve_client(client& c);
};

class action
{
public:
	using func_type = std::function<void(client&)>;
	void add(func_type func);
	void fire(client& cl);
private:
	std::list<func_type> _list;
};

struct expectation
{
	std::string trigger;
	action act;
	int times_fired = 0;
	int max_calls = std::numeric_limits<int>::max();

	void fire(client& cl)
	{
		act.fire(cl);
		times_fired += 1;
	}

	bool active() const
	{
		return times_fired < max_calls;
	}
};

class telnet : public server
{
public:
	telnet();

	telnet& when(std::string input);
	telnet& reply(std::string output);
	telnet& shutdown_server();
	telnet& freeze(useconds_t usec);
	telnet& once();
	telnet& times(int n);
	telnet& reply_once(std::string output);
	telnet& close_connection();

private:
	virtual void serve_client(client& c);
	void add_action(action::func_type f);
	expectation& current();

	using expect_list = std::list<expectation>;
	expect_list _expect;
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

private:
	std::unique_ptr<T> t;
};

template <typename T>
mock<T> start()
{
	return std::move(mock<T>());
}

} // namespace nemok
