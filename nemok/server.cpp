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
		socket s;
		s.create();
		client_pool clients;

		s.reuse_addr();
		bind_server_socket(s);
		s.listen();

		fcntl(s.fd(), F_SETFL, O_NDELAY);

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

void server::bind_server_socket(socket& sock)
{
	sock.bind(_server_port);
	_effective_port = sock.get_port();
}

void server::accept_connections(socket& server_socket, std::function<void(int)> handler)
{
	while (!_terminate_server_flag)
	{
		socket client_socket = server_socket.try_accept();
		if (client_socket.bad() && errno == EWOULDBLOCK)
		{
			::usleep(1);
			continue;
		}

		if (!client_socket.bad())
		{
			handler(client_socket.fd());
			client_socket.detach();
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
	buffer_type buffer(1024);
	size_t bytes_received = 0;
	while (bytes_received == c.read_some(&buffer[0], buffer.size()))
	{
		c.write_all(&buffer[0], bytes_received);
	}
}

client connect_client(const server& server)
{
	client ret;
	ret.connect(server.port());
	return std::move(ret);
}

void write_client(client& c, std::string buf)
{
	c.write(buf.c_str(), buf.size());
}

std::string read_some(client& cl, size_t len)
{
	std::string ret;
	if (len > 0)
	{
		ret.resize(len);
		size_t bytes_read = cl.read_some(&ret[0], ret.size());
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

matcher& matcher::when(trigger_type&& trigger)
{
	if (!_current.empty())
	{
		_expect.create(std::move(_current));
	}

	_current = expectation();
	_current.trigger = std::move(trigger);

	return *this;
}

void matcher::match(buffer_type& input, client& cl)
{
	if (!_current.empty())
	{
		_expect.create(std::move(_current));
	}

	_expect.walk_stream(input, cl);
}

void matcher::add_action(action::func_type f)
{
	current().act.add(std::move(f));
}

matcher& matcher::close_connection()
{
	add_action([=](auto& conn){conn.disconnect();});
	return *this;
}

matcher& matcher::exec(action_type&& act)
{
	add_action(std::move(act));
	return *this;
}

matcher& matcher::freeze(useconds_t usec)
{
	add_action([=](auto&){::usleep(usec);});
	return *this;
}

matcher& matcher::once()
{
	current().max_calls = 1;
	return *this;
}

matcher& matcher::times(int n)
{
	current().max_calls = n;
	return *this;
}

matcher& matcher::order(int n)
{
	current().order = n;
	return *this;
}

telnet& telnet::reply_once(std::string output)
{
	return reply(std::move(output)).once();
}

telnet& telnet::when(std::string input)
{
	return base_type::when(starts_with(input));
}

telnet& telnet::reply(std::string output)
{
	exec([=](auto& c){c.write_all(output.c_str(), output.size());});

	return *this;
}

expectation& matcher::current()
{
	return _current;
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

void expect_list::walk_stream(buffer_type& input, client& cl)
{
	if (!input.empty())
	{
		auto l = _data.begin();
		while (l != _data.end())
		{
			auto e = l->second.begin();
			while (e != l->second.end())
			{
				if (e->trigger(input))
				{
					e->fire(cl);

					if (!e->active())
					{
						l->second.erase(e);
					}
					else
					{
						expectation temp = std::move(*e);
						l->second.erase(e);
						l->second.emplace_back(std::move(temp));	
					}

					e = l->second.begin();
					continue;
				}
				++e;
			}
			++l;
		}
	}
}

bool expect_list::empty() const
{
	return _data.empty();
}

expectation& expect_list::create(expectation&& e)
{
	const int ord = e.order;
	_data[ord].emplace_back(std::move(e));
	return _data[ord].back();
}

bool starts_with::operator ()(buffer_type& input)
{
	if (input.size() >= _test.size() && 0 == memcmp(&input[0], &_test[0], _test.size()))
	{
		input.erase(std::begin(input), std::begin(input) + _test.size());
		return true;
	}

	return false;
}

} // namespace nemok

