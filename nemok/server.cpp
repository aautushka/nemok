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
	using handler = std::function<void(client&)>;

	void add_client(client c, handler h)
	{
		_threads.emplace_back(std::make_pair(std::move(c), std::thread()));
		_threads.back().second = std::thread(std::bind(h, std::ref(_threads.back().first)));
	}

	~client_pool()
	{
		for (auto& t : _threads)
		{
			auto& cl = t.first;
			auto& tr = t.second;
			if (tr.joinable())
			{
				// TODO: this is not thread safe
				// may cause a race condition
				cl.shutdown();
				tr.join();
			}
		}
	}

private:
	using pair = std::pair<client, std::thread>; 
	std::list<pair> _threads;
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
		int s = socket(AF_INET, SOCK_STREAM, 0);

		if (s == -1)
		{
			throw network_error("can't create a socket");
		}

		before_leaving close_socket([=](){::shutdown(s, SHUT_RDWR); ::close(s);});
		client_pool clients;

		set_socket_opts(s);
		bind_server_socket(s);

		if (-1 == listen(s, 5))
		{
			throw network_error("can't listen on a socket");
		}

		fcntl(s, F_SETFL, O_NDELAY);

		ready.set_value();

		accept_connections(s, [&](int client_socket)
		{
			client c;
		       	c.assign(client_socket);

			using namespace std::placeholders;
			clients.add_client(std::move(c), std::bind(&server::run_client, this, _1));
		});
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
		throw network_error("can't bind a socket");
	}

	socklen_t socklen = sizeof(addr);
	if (-1 == getsockname(sock, (sockaddr*)&addr, &socklen))
	{
		throw network_error("can't get socket address");
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

		if (client_socket != -1)
		{
			handler(client_socket);
		}
		else
		{
			std::cout << "accept error " << errno << std::endl;
		}
	}
}

void server::run_client(client& c)
{
	try
	{
		serve_client(c);
	}
	catch (exception& e)
	{
	}
}

void echo::serve_client(client& c)
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
	expectation e;
	e.trigger = std::move(input);

	_expect.emplace_back(std::move(e));

	return *this;
}

telnet& telnet::reply(std::string output)
{
	assert(!_expect.empty());
	
	add_action([=](auto& c){c.write(output.c_str(), output.size());});

	return *this;
}

template <typename T, typename V>
bool starts_with(const T& str, const V& test)
{
	return str.size() >= test.size() && 0 == memcmp(&str[0], &test[0], test.size());
}

void telnet::serve_client(client& cl)
{
	ssize_t bytes = 0;
	do
	{
		bytes = cl.read(&_buffer[0], _buffer.size());
		assert(bytes >= 0);
	
		if (bytes > 0)
		{
			_input.insert(_input.end(), &_buffer[0], &_buffer[0] + bytes); 

			auto e = _expect.begin();
			while (e != _expect.end())
			{
				if (starts_with(_input, e->trigger))
				{
					_input.erase(std::begin(_input), std::begin(_input) + e->trigger.size());
					e->fire(cl);

					expectation temp = std::move(*e);
					_expect.erase(e);
					_expect.emplace_back(std::move(temp));	

					e = _expect.begin();
					continue;
				}
				++e;
			}
		}
	}
	while (bytes > 0); // zero value mark end of stream
}

telnet::telnet()
{
	_buffer.resize(1024);
}

void telnet::add_action(action::func_type f)
{
	_expect.back().act.add(std::move(f));
}

telnet& telnet::shutdown()
{
	assert(!_expect.empty());
	add_action([=](auto&){this->stop();});
	return *this;
}

telnet& telnet::freeze(useconds_t usec)
{
	assert(!_expect.empty());
	add_action([=](auto&){::usleep(usec);});
	return *this;
}

void action::add(func_type func)
{
	_list.emplace_back(std::move(func));
}

void action::fire(client& cl)
{
	for (auto& f: _list)
	{
		f(cl);
	}
}
} // namespace nemok

