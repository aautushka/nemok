#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>

#include <vector>
#include <thread>
#include <iostream>
#include <cassert>
#include <cstring>

#include "server.h"

namespace nemok
{
server::server()
	: _server_port(0)
	, _effective_port(0)
	, _server_running(false)
	, _terminate_server_flag(false)
{
}

server::~server()
{
	stop();
}

server::port_t server::start(port_t port)
{
	if (_server_running)
	{
		throw already_running();
	}

	_terminate_server_flag = false;
	_server_port = port;

	std::promise<void> server_ready;
	std::future<void> ready_future = server_ready.get_future();

	_server_thread = std::thread([&](auto p){this->run_server(std::move(p));}, std::move(server_ready));
	ready_future.wait();
	_server_running = true;

	return _effective_port;
}

void server::stop()
{
	// TODO: this method may be called from different threads
	// need to make it thread-safe
	if (_server_running)
	{
		_terminate_server_flag = true;
	}
}

void server::wait()
{
	if (_server_thread.joinable())
	{
		_server_thread.join();
		_server_thread = std::thread();
	}
}

bool server::running() const
{
	return _server_running;
}

server::port_t server::port() const
{
	return _effective_port ;
}

int server::open_connections()
{
	return 0;
}

class client_pool
{
public:
	void add_client(std::function<void(void)> handler)
	{
		_threads.emplace_back(handler);
	}

	~client_pool()
	{
		for (auto& t : _threads)
		{
			t.join();
		}
	}

private:
	std::vector<std::thread> _threads;
};

class before_leaving
{
public:
	explicit before_leaving(std::function<void(void)> f)
		: _func(f)
	{
	}

	before_leaving(before_leaving&) = delete;
	before_leaving& operator =(before_leaving&) = delete;

	~before_leaving()
	{
		_func();
	}
private:
	std::function<void(void)> _func;
};

void server::run_server(std::promise<void> ready)
{
	try
	{
		client_pool clients;

		int s = socket(AF_INET, SOCK_STREAM, 0);
		before_leaving close_socket([=](){close(s);});

		if (s == -1)
		{
			throw network_error();
		}

		set_socket_opts(s);
		bind_server_socket(s);

		if (-1 == listen(s, 5))
		{
			throw network_error();
		}

		fcntl(s, F_SETFL, O_NDELAY);

		ready.set_value();

		accept_connections(s, [&](int client_socket){
			clients.add_client([=](){
				this->run_client(client_socket);});});
	}
	catch (std::exception&)
	{
		ready.set_exception(std::current_exception());
	}

	_server_running = false;
	_terminate_server_flag = false;
	_effective_port = 0;
}

void server::set_socket_opts(int sock)
{
	int flag = 1;
	
	// TODO: do we really need TCP_NODELTA?
	//setsockopt(s, IPPROTO_TCP, TCP_NODELTA, &flag, sizeof(flag));

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

void server::bind_server_socket(int sock)
{
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_server_port);
	
	if (-1 == bind(sock, (struct sockaddr*)&addr, sizeof(addr)))
	{
		throw network_error();
	}

	socklen_t socklen = sizeof(addr);
	if (-1 == getsockname(sock, (sockaddr*)&addr, &socklen))
	{
		throw network_error();
	}

	_effective_port = ntohs(addr.sin_port); 
}

void server::accept_connections(int server_socket, std::function<void(int)> handler)
{
	while (!_terminate_server_flag)
	{
		sockaddr_in client_addr;
		socklen_t size = sizeof(client_addr);
		int client_socket = accept(server_socket, (sockaddr*)&client_addr, &size);
		if (client_socket == -1 && errno == EWOULDBLOCK) 
		{
			::usleep(1);
			continue;
		}

		handler(client_socket);
	}
}

void server::run_client(int client_socket)
{
	try
	{
		client c;
		c.assign(client_socket);
		serve_client(std::move(c));
	}
	catch (exception& e)
	{
	}
}

void echo::serve_client(client c)
{
	std::vector<uint8_t> buffer(1024);
	size_t bytes_received = 0;
	while (bytes_received == c.read(&buffer[0], buffer.size()))
	{
		c.write(&buffer[0], bytes_received);
	}
}

client connect_client(const server& server)
{
	client ret;
	ret.connect(server.port());
	return std::move(ret);
}

std::string read_some(client& cl, size_t len)
{
	std::string ret;
	if (len > 0)
	{
		ret.resize(len);
		size_t bytes_read = cl.read(&ret[0], ret.size());
		ret.resize(bytes_read);
	}
	return std::move(ret); 
}

std::string read_all(client& cl, size_t len)
{
	std::string ret;
	if (len > 0)
	{
		ret.resize(len);
		cl.read_all(&ret[0], ret.size());
	}
	return std::move(ret); 
}

telnet& telnet::when(std::string input)
{
	auto e = std::make_pair(std::move(input), [](client&){}); 
	_expectations.emplace_back(std::move(e));

	return *this;
}

telnet& telnet::reply(std::string output)
{
	assert(!_expectations.empty());
	
	_expectations.back().second = [=](client& c){c.write(output.c_str(), output.size());};

	return *this;
}

template <typename T, typename V>
bool starts_with(const T& str, const V& test)
{
	return str.size() >= test.size() && 0 == memcmp(&str[0], &test[0], test.size());
}

void telnet::serve_client(client cl)
{
	ssize_t bytes = 0;
	do
	{
		bytes = cl.read(&_buffer[0], _buffer.size());
		assert(bytes >= 0);
	
		if (bytes > 0)
		{
			_input.insert(_input.end(), &_buffer[0], &_buffer[0] + bytes); 

			auto i = _expectations.begin();
			while (i != _expectations.end())
			{
				if (starts_with(_input, i->first))
				{
					_input.erase(std::begin(_input), std::begin(_input) + i->first.size());
					i->second(cl);

					i = _expectations.begin();
					continue;
				}
				++i;
			}
		}
	}
	while (bytes > 0); // zero value mark end of stream
}

telnet::telnet()
{
	_buffer.resize(1024);
}

telnet& telnet::shutdown()
{
	assert(!_expectations.empty());
	_expectations.back().second = [=](auto&){this->stop();};
	return *this;
}

} // namespace nemok

