#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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
		shutdown();
		::close(_sock);
		_sock = -1;
	}
}

void client::shutdown()
{
	if (-1 != _sock)
	{
		::shutdown(_sock, SHUT_RDWR);
	}
}

bool client::connected() const
{
	return -1 != _sock;
}

void client::connect(port_t port)
{
	disconnect();

	socket new_socket;
	new_socket.create();
	new_socket.connect(port);

	_sock = new_socket.detach();
}

int wait_while_ready(pollfd& fd)
{
	while (true)
	{
		int ret = 0;
		switch (ret = poll(&fd, 1, 100))
		{
			case 0: // timeout
				break;
			case -1: // error
				throw network_error("can't poll a socket");
			default: // data ready
				return ret;
		}
	}
}

ssize_t client::read_some(void* buffer, size_t length)
{
	if (!connected())
	{
		throw not_connected();
	}

	ssize_t bytes = -1;
	pollfd poll_data;
	poll_data.fd = _sock;
	poll_data.events = POLLIN | POLLERR | POLLHUP;

	do
	{
		if (POLLIN == wait_while_ready(poll_data))
		{
			bytes = ::read(_sock, buffer, length);
			if (0 == bytes)
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	while (bytes == -1 && (errno == EINTR || errno == EAGAIN));

	if (bytes == -1)
	{
		throw network_error("can't read from a socket");
	}

	return bytes;
}

ssize_t client::write_some(const void* buffer, size_t length)
{
	if (!connected())
	{
		throw not_connected();
	}

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
		written += write_some(buf + written, len - written);
	}

}

void client::read_all(void* buffer, size_t len)
{
	uint8_t* const buf = static_cast< uint8_t*>(buffer);
	size_t bytes_read = 0;
	while (bytes_read != len)
	{
		ssize_t ret = read_some(buf + bytes_read, len - bytes_read); 
		if (ret > 0)
		{
			bytes_read += ret;
		}
		else // can't read any more, perhaps the socket was closed
		{
			throw network_error();
		}
	}
}

}
