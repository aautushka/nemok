#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <strings.h>
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
	close();
}

void client::close()
{
	if (-1 != _sock)
	{
		::close(_sock);
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
		throw network_error();
	}

	struct sockaddr_in addr;
	bzero((char*)&addr, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (-1 == ::connect(_sock, (sockaddr*)&addr, sizeof(addr)))
	{
		throw network_error();
	}
}

size_t client::read(void* buffer, size_t length)
{
	assert(connected());

	ssize_t bytes = -1;

	do
	{
		bytes = ::read(_sock, buffer, length);
	}
	while (bytes == -1 && errno == EINTR);

	if (bytes == -1)
	{
		throw network_error();
	}

	return bytes;
}

size_t client::write(const void* buffer, size_t length)
{
	assert(connected());
	ssize_t bytes = -1;
	do
	{
		bytes = ::write(_sock, buffer, length);
	}
	while (bytes == -1 && errno == EINTR);

	if (bytes == -1)
	{
		throw network_error();
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
	rhs.close();
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
		std::cout << "written " << written << std::endl;
	}
}

void client::read_all(void* buffer, size_t len)
{
	uint8_t* const buf = static_cast< uint8_t*>(buffer);
	size_t bytes_read = 0;
	while (bytes_read != len)
	{
		bytes_read += read(buf + bytes_read, len - bytes_read);
		std::cout << "read " << bytes_read << std::endl;
	}
}

}
