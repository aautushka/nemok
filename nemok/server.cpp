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

#include "server.h"

namespace nemok
{
server::server()
	: _server_port(0)
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

	std::promise<void> server_ready;
	std::future<void> ready_future = server_ready.get_future();

	_server_thread = std::thread([&](auto p){this->run_server(std::move(p));}, std::move(server_ready));
	ready_future.wait();
	_server_running = true;
}

void server::stop()
{
	if (_server_running)
	{
		_terminate_server_flag = true;
		_server_thread.join();
		_server_running = false;
	}
}

bool server::running() const
{
	return _server_running;
}

server::port_t server::port() const
{
	return _server_port;
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

		accept_connections(s, [&](int s){
			clients.add_client([=](){
				this->run_client(s);});});
	}
	catch (std::exception&)
	{
		ready.set_exception(std::current_exception());
	}
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

	_server_port = ntohs(addr.sin_port); 
}

void server::accept_connections(int sock, std::function<void(int)> handler)
{
	while (!_terminate_server_flag)
	{
		sockaddr_in client_addr;
		socklen_t size = sizeof(client_addr);
		int client = accept(sock, (sockaddr*)&client_addr, &size);
		if (client == -1 && errno == EWOULDBLOCK) 
		{
			::usleep(1);
			continue;
		}

		handler(client);
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

void echo_server::serve_client(client c)
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

std::string read_string_from_client(client& cl, size_t len)
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

} // namespace nemok

