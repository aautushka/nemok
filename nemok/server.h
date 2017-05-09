#pragma once

#include <thread>
#include <atomic>
#include <stdexcept>
#include <future>
#include <list>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <regex.h>
#include <cstring>
#include <cassert>

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

	auto mock = nemok::start<telnet>();
	mock.expect("hello").reply("world");
 	mock.expect(line()).reply("done");
	mock.expect(line("hello")).reply("done");
	mock.expect(regex("[^\n]+\n")).reply("done");
	mock.expect("[^\n]+\n"_re).reply("done");
*/

namespace nemok
{

using buffer_type = std::vector<uint8_t>;

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

class posix_regex
{
public:
	posix_regex()
	{
	}

	~posix_regex()
	{
		free_regex();
	}

	posix_regex(const char* pattern, int flags)
	{
		compile(pattern);
	}

	explicit posix_regex(const char* pattern)
	{
		compile(pattern);
	}

	void compile(const char* pattern)
	{
		compile(pattern, REG_EXTENDED);
	}

	void compile(const char* pattern, int flags)
	{
		reset();
		_regex.reset(new regex_t);
		if (regcomp(_regex.get(), pattern, flags))
		{
			reset();
		}
	}

	posix_regex(const posix_regex&) = delete;
	posix_regex& operator =(const posix_regex&) = delete;


	posix_regex(posix_regex&& rhs)
	{
		std::swap(_regex, rhs._regex);
	}

	posix_regex& operator =(posix_regex&& rhs)
	{
		std::swap(_regex, rhs._regex);
		return *this;
	}

	bool match(const char* input) const
	{
		if (_regex)
		{
			return 0 == regexec(_regex.get(), input, 0, nullptr, 0);
		}

		return false;
	}

	bool match(const char* input, std::pair<const char*, const char*>& match)
	{
		if (_regex)
		{
			regmatch_t m;
			if (0 == regexec(_regex.get(), input, 1, &m, 0))
			{
				match = std::make_pair(input + m.rm_so, input + m.rm_eo);
				return true;
			}
		}

		return false;
	}

	void reset()
	{
		free_regex();
	}

private:
	void free_regex()
	{
		if (_regex)
		{
			regfree(_regex.get());
			_regex.reset();
		}
	}

	std::unique_ptr<regex_t> _regex = nullptr;
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

	ssize_t read_some(void* buffer, size_t length);
	ssize_t write_some(const void* buffer, size_t length);

	bool connected() const;

	void write_all(const void* buffer, size_t length);
	void read_all(void* buffer, size_t length);

	void write(const void* buffer, size_t len) { write_all(buffer, len); }
	void read(void* buffer, size_t len) { read_all(buffer, len);}

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
void write_client(client& cl, std::string buf);


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
	using trigger_type = std::function<bool(buffer_type&)>;
	using action_type = std::function<void(client&)>;

	trigger_type trigger;
	action act;
	int times_fired = 0;
	int max_calls = std::numeric_limits<int>::max();
	int order = 100;

	void fire(client& cl)
	{
		act.fire(cl);
		times_fired += 1;
	}

	bool active() const
	{
		return times_fired < max_calls;
	}

	bool empty() const
	{
		return !trigger; 
	}
};

class expect_list
{
public:

	void walk_stream(buffer_type& buf, client& cl);
	bool empty() const;
	expectation& create(expectation&& e);

private:
	using list = std::list<expectation>;
	using priority_list = std::map<int, list>;
	priority_list _data;
};

class line
{
public:
	line& terminator(char ch) {_term = ch;}
	line() { }
	explicit line(std::string exact_line) : _line(exact_line) {}

private:
	char _term = '\n'; 
	std::string _line;
};

class any_line
{
public:
	bool operator ()(buffer_type& input)
	{
		auto newline = std::find(input.begin(), input.end(), uint8_t('\n'));
		if (newline != input.end())
		{
			input.erase(input.begin(), ++newline);
			return true;
		}

		return false;
	}
private:
};

class starts_with
{
public:
	explicit starts_with(std::string test): _test(std::move(test)) {}
	bool operator ()(buffer_type& input);

private:
	std::string _test;
};



class regex
{
public:
	explicit regex(std::string expr)
	{
		_re_str = std::move(expr);
	}

	bool operator ()(buffer_type& input)
	{
		posix_regex re(_re_str.c_str());

		std::pair<const char*, const char*> match;
		std::string copy(input.begin(), input.end());
		if (re.match(copy.c_str(), match))
		{
			auto beg = input.begin();
			auto end = beg;
			std::advance(end, match.second - copy.c_str());
			input.erase(beg, end);
			return true;
		}

		return false;
	}
private:

	std::string _re_str;
};

namespace lit
{
inline regex operator "" _re(const char* expr, size_t sz)
{
	assert(sz == strlen(expr));
	return regex(expr);
}
}

class matcher
{
public:
	using trigger_type = expectation::trigger_type;
	using action_type = expectation::action_type;

	matcher() {}

	matcher& when(trigger_type&& trigger);
	matcher& exec(action_type&& act);
	matcher& freeze(useconds_t usec);
	matcher& once();
	matcher& times(int n);
	matcher& order(int n);
	matcher& close_connection();

	void match(buffer_type& input, client& cl);

private:
	void add_action(action::func_type f);
	expectation& current();

	expect_list _expect;
	expectation _current;
};

template <typename T>
class basic_mock : public server
{
public:
	using trigger_type = expectation::trigger_type;
	using action_type = expectation::action_type;

	basic_mock() {}
	T& when(trigger_type&& trigger)
	{
		_matcher.when(std::move(trigger));
		return static_cast<T&>(*this);
	}

	T& exec(action_type&& act)
	{
		_matcher.exec(std::move(act));
		return static_cast<T&>(*this);
	}

	T& shutdown_server()
	{
		return exec([=](auto&){this->stop();});
	}


	T& freeze(useconds_t usec)
	{
		_matcher.freeze(usec);
		return static_cast<T&>(*this);
	}

	T& once()
	{
		_matcher.once();
		return static_cast<T&>(*this);
	}

	T& times(int n)
	{
		_matcher.times(n);
		return static_cast<T&>(*this);
	}

	T& order(int n)
	{
		_matcher.order(n);
		return static_cast<T&>(*this);
	}

	T& close_connection()
	{
		_matcher.close_connection();
		return static_cast<T&>(*this);
	}

private:
	virtual void serve_client(client& cl)
	{
		buffer_type input;
		buffer_type buffer(1024);
		matcher matcher_copy = _matcher;
		ssize_t bytes = 0;
		do
		{
			bytes = cl.read_some(&buffer[0], buffer.size());
			if (bytes > 0)
			{
				input.insert(input.end(), &buffer[0], &buffer[0] + bytes); 
				matcher_copy.match(input, cl);
			}
		}
		while (bytes > 0); // zero value mark end of stream
	}

	matcher _matcher;
};

class telnet : public basic_mock<telnet>
{
public:
	using base_type = basic_mock<telnet>;

	telnet() {}

	telnet& when(std::string input);
	telnet& reply(std::string output);
	telnet& reply_once(std::string output);

	using base_type::when;
};

template <typename T>
class mock
{
public:
	using trigger_type = expectation::trigger_type;

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

	template <typename U>
	T& when(U u)
	{
		return t->when(std::move(u));
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
