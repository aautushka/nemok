#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <strings.h>
#include <poll.h>
#include <cassert>

#include <iostream>

#include "server.h"

namespace nemok
{

client::client()
	: _sock(-1)
{
}

client::~client()
{
	disconnect();
}

void client::disconnect()
{
	if (-1 != _sock)
	{
		::close(_sock);
		_sock = -1;
	}
}

bool client::connected() const
{
	return -1 != _sock;
}

void client::connect(port_t port)
{
	assert(!connected());

	_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_sock == -1)
	{
		throw network_error("can't create a socket");
	}

	struct sockaddr_in addr;
	bzero((char*)&addr, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (-1 == ::connect(_sock, (sockaddr*)&addr, sizeof(addr)))
	{
		throw network_error("can't connect a socket");
	}
}

ssize_t client::read(void* buffer, size_t length)
{
	assert(connected());

	ssize_t bytes = -1;
	pollfd poll_data;
	poll_data.fd = _sock;
	poll_data.events = POLLIN;

	do
	{
		switch (poll(&poll_data, 1, 100))
		{
			case 0: // timeout
				continue;
			case -1: // error
				throw network_error("can't poll a socket");
			default: // data ready
			       break;	
		}
		bytes = ::read(_sock, buffer, length);
	}
	while (bytes == -1 && (errno == EINTR || errno == EAGAIN));

	if (bytes == -1)
	{
		throw network_error("can't read from a socket");
	}

	return bytes;
}

ssize_t client::write(const void* buffer, size_t length)
{
	assert(connected());
	ssize_t bytes = -1;
	do
	{
		bytes = ::write(_sock, buffer, length);
	}
	while (bytes == -1 && (errno == EINTR || errno == EAGAIN));

	if (bytes == -1)
	{
		throw network_error("can't write to a socket");
	}

	return bytes;
}

client::client(client&& rhs)
{
	*this = std::move(rhs);
}

client& client::operator =(client&& rhs)
{
	std::swap(_sock, rhs._sock);
	rhs.disconnect();
	return *this;
}

void client::assign(int fd)
{
	client rhs;
	rhs._sock = fd;
	*this = std::move(rhs);
}

void client::write_all(const void* buffer, size_t len)
{
	const uint8_t* const buf = static_cast<const uint8_t*>(buffer);
	size_t written = 0;
	while (written != len)
	{
		written += write(buf + written, len - written);
	}

}

void client::read_all(void* buffer, size_t len)
{
	uint8_t* const buf = static_cast< uint8_t*>(buffer);
	size_t bytes_read = 0;
	while (bytes_read != len)
	{
		bytes_read += read(buf + bytes_read, len - bytes_read);
	}
}

}
