#pragma once

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include "server.h"

namespace nemok
{
namespace ev2
{

class libevent_error : public exception
{
public:
	libevent_error() : exception() {}
	explicit libevent_error(const char* message) : exception(message) {}
};

class socket
{
public:
	socket(const socket&) = delete;
	socket& operator =(const socket&) = delete;

	socket() {}

	socket(socket&& rhs)
	{
		fd_ = rhs.detach();
	}

	socket& operator =(socket&& rhs)
	{
		close();
		fd_ = rhs.detach();
		return *this;
	}

	void create()
	{
		assert(fd_ == -1);
		fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd_ == -1)
		{
			throw system_error("can't create socket");
		}
	}
	
	void bind(uint16_t port)
	{
		assert(fd_ != -1);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port); 

		if (-1 == ::bind(fd_, (sockaddr*)&addr, sizeof(addr)))
		{
			throw system_error("can't bind socket");
		}
	}

	void reuse_addr(int yes = 1)
	{
		assert(fd_ != -1);
		setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	}

	void listen()
	{
		assert(fd_ != -1);
		if (-1 == ::listen(fd_, 0))
		{
			throw system_error("can't listen socket");
		}
	}

	void close()
	{
		if (fd_ != -1)
		{
			::close(fd_);
			detach();
		}
	}

	int detach()
	{
		int ret = fd_;
		fd_ = -1;
		return ret;
	}

	void attach(int fd)
	{
		close();
		fd_ = fd;
	}

	void make_nonblocking()
	{
		assert(fd_ != -1);
		if (-1 == evutil_make_socket_nonblocking(fd_))
		{
			throw system_error("can't make socket nonblocking");
		}
	}

	~socket()
	{
		close();
	}

	socket accept()
	{
		sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int fd = ::accept(fd_, (sockaddr*)&addr, &len);
		if (fd == -1)
		{
			throw system_error("can't accept client connection");
		}

		socket ret;
		ret.attach(fd);
		return std::move(ret);
	}

	int fd() const
	{
		return fd_;
	}

	uint16_t get_port()
	{
		assert(-1 != fd_);

		sockaddr_in addr;
		socklen_t socklen = sizeof(addr);
		if (-1 == getsockname(fd_, (sockaddr*)&addr, &socklen))
		{
			throw system_error("can't get socket address");
		}

		return ntohs(addr.sin_port); 
	}

private:
	evutil_socket_t fd_ = -1;
};


class buffer
{
public:
	buffer(const buffer&) = delete;
	buffer& operator =(const buffer&) = delete;

	buffer() { }

	buffer(buffer&& rhs)
	{
		buf_ = rhs.buf_;
		rhs.buf_ = nullptr;
	}

	buffer& operator =(buffer&& rhs)
	{
		destroy();
		buf_ = rhs.buf_;
		rhs.buf_ = nullptr;
		return *this;
	}

	void create()
	{
		assert(buf_ == nullptr);
		buf_ = evbuffer_new();
		if (!buf_)
		{
			throw libevent_error("can't create event buffer");
		}
	}

	void destroy()
	{
		if (buf_)
		{
			evbuffer_free(buf_);
			buf_ = nullptr;
		}
	}

	~buffer()
	{
		destroy();
	}

	void attach(evbuffer* buf)
	{
		destroy();
		buf_ = buf;
	}

	evbuffer* detach()
	{
		auto ret = buf_;
		buf_ = nullptr;
		return ret;
	}

	int length()
	{
		assert(buf_ != nullptr);
		return evbuffer_get_length(buf_);
	}

	int remove(std::vector<uint8_t>& out)
	{
		assert(buf_ != nullptr);
		const int len = length();
		const size_t prev = out.size();
		out.resize(len + prev);
		const int removed = evbuffer_remove(buf_, &out[prev], len);
		out.resize(prev + removed);

		return removed;
	}

	void add(const void* data, size_t len)
	{
		evbuffer_add(buf_, data, len);
	}

	int write(bufferevent* ev)
	{
		assert(buf_ != nullptr);
		return bufferevent_write_buffer(ev, buf_);
	}

private:
	evbuffer* buf_ = nullptr;
};

class connection
{
public:
	connection(const connection&) = delete;
	connection& operator =(const connection&) = delete;

	connection(buffer& in, buffer& out)
		: input_(in), output_(out)
	{
	}

	int read(std::vector<uint8_t>& buf)
	{
		return input_.remove(buf);
	}

	void write(const std::vector<uint8_t>& buf)
	{
		output_.add(&buf[0], buf.size());
	}

private:
	buffer& input_;
	buffer& output_;
};

class event_loop
{
public:
	event_loop() = default;
	event_loop(const event_loop&) = delete;
	event_loop& operator =(const event_loop&) = delete;

	event_loop(event_loop&& rhs)
	{
		evbase_ = rhs.evbase_;
		rhs.evbase_ = nullptr;
	}

	event_loop& operator =(event_loop&& rhs)
	{
		destroy();
		evbase_ = rhs.evbase_;
		rhs.evbase_ = nullptr;
		return *this;
	}

	~event_loop()
	{
		destroy();
	}

	void destroy()
	{
		if (evbase_)
		{
			event_base_free(evbase_);
			evbase_ = nullptr;
		}
	}

	void create()
	{
		assert(evbase_ == nullptr);
		evbase_ = event_base_new();
		if (nullptr == evbase_)
		{
			throw libevent_error("event_base_new failure");
		}
	}

	void dispatch()
	{
		assert(evbase_ != nullptr);
		event_base_dispatch(evbase_);
	}

	void accept(socket& sock, std::function<void(socket&)> handler)
	{
		assert(evbase_ != nullptr);
		assert(!accept_handler_);

		accept_handler_ = handler;
		accept_socket_ = &sock;

		auto h = new decltype(handler)(std::move(handler));
		accept_event_ = event_new(evbase_, sock.fd(), EV_READ | EV_PERSIST, on_accept, this);
		event_add(accept_event_, nullptr);
	}

	void read(socket sock, std::function<void(connection&)> handler)
	{
		assert(evbase_ != nullptr);

		read_socket_ = std::move(sock);
		read_handler_ = std::move(handler);
		output_buffer_.create();

		// TODO: what's the flag, how do I manage the socket lifecycle
		bufferevent* ev = bufferevent_socket_new(evbase_, read_socket_.fd(), BEV_OPT_CLOSE_ON_FREE);
		if (!ev)
		{
			throw libevent_error("can't create buffer event");
		}

		bufferevent_setcb(ev, on_read, on_write, on_error, this);
		bufferevent_enable(ev, EV_READ);
	}

	void stop()
	{
		//event_base_loopexit(evbase_, nullptr);	
		event_base_loopbreak(evbase_);
	}

private:
	event_base* evbase_ = nullptr;
	std::function<void(socket&)> accept_handler_;
	socket* accept_socket_ = nullptr;
	event* accept_event_ = nullptr;

	socket read_socket_;
	std::function<void(connection&)> read_handler_;
	buffer output_buffer_;

	static void on_accept(evutil_socket_t fd, short ev, void* arg)
	{
		event_loop& self = *static_cast<event_loop*>(arg);
		self.on_accept(fd);
	}

	static void on_read(bufferevent* ev, void* arg)
	{
		event_loop& self = *static_cast<event_loop*>(arg);
		self.on_read(ev);
	}

	static void on_write(bufferevent* ev, void* arg)
	{
	}

	static void on_error(bufferevent* ev, short err, void* arg)
	{
		// TODO: close the connection
	}

	void on_accept(evutil_socket_t fd)
	{
		socket sock;
		sock.attach(fd);
		accept_handler_(sock);
		sock.detach();
	}

	void on_read(bufferevent* ev)
	{
		auto input = bufferevent_get_input(ev);

		buffer input_buffer;
		input_buffer.attach(input);
		connection conn(input_buffer, output_buffer_);
		read_handler_(conn);
		input_buffer.detach();

		if (-1 == output_buffer_.write(ev))

		{
			throw libevent_error("can't write output buffer");
		}		
	}
};

class acceptor
{
public:
	using handler_type = std::function<void(socket)>;

	acceptor() = default;
	acceptor(const acceptor&) = delete;
	acceptor& operator =(const acceptor&) = delete;

	void accept(socket server_socket)
	{
		loop_.create();
		loop_.accept(server_socket, [&](socket& s){this->on_accept(s);});
		loop_.dispatch();
	}

	void stop()
	{
		loop_.stop();
	}

	acceptor& subscribe(handler_type handler)
	{
		handler_ = handler;
	}

	acceptor& operator +=(handler_type handler)
	{
		return subscribe(handler);
	}

private:
	void on_accept(socket& server_socket)
	{
		socket client_socket = server_socket.accept();
		client_socket.make_nonblocking();
		handler_(std::move(client_socket));
	}

	handler_type handler_;
	event_loop loop_;
};


class server
{
public:
	server()
	{
	}

	~server()
	{
	}

	server(const server&) = delete;
	server& operator =(const server&) = delete;

	void start(uint16_t port)
	{
		port_ = port;

		std::promise<void> server_ready;
		std::future<void> ready_future = server_ready.get_future();

		server_thread_ = std::thread([&](auto p){this->run_server(std::move(p));}, std::move(server_ready));
		ready_future.wait();
	}

	void stop()
	{
		acceptor_.stop();

		// we need to make this connection in order to break from the loop
		// I failed to find a better solution
		connect_client();
	}
	
	void wait()
	{
		if (server_thread_.joinable())
		{
			server_thread_.join();
		}
	}

	bool running() const
	{
	}

	uint16_t port() const
	{
		return port_;
	}

	int open_connections()
	{
		return 0;
	}

	nemok::client connect_client()
	{
		nemok::client ret;
		ret.connect(port_);
		return std::move(ret);
	}

private:
	void run_server(std::promise<void> ready)
	{
		socket s;

		s.create();
		s.bind(port_);
		s.listen();
		s.reuse_addr();
		s.make_nonblocking();

		port_ = s.get_port();
		ready.set_value();

		acceptor_ += [&](socket s){this->on_client_connection(std::move(s));};
		acceptor_.accept(std::move(s));
	}

	void on_client_connection(socket client_socket)
	{
		using namespace std::placeholders;
		std::thread t(std::bind(&server::run_client, this, _1), std::move(client_socket));
		t.detach();
	}

	void run_client(socket client_socket)
	{
		event_loop loop;
		loop.create();
		loop.read(std::move(client_socket), [&](connection& c){handle_client(c);});
		loop.dispatch();
	}

	void handle_client(connection& conn)
	{
		std::vector<uint8_t> data;
		conn.read(data);
		conn.write(data);
	}

	std::atomic<uint16_t> port_;
	std::thread server_thread_;
	acceptor acceptor_;
};

} // namespace ev
} // namespace nemok
