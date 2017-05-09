#pragma once
#include "server.h"

namespace nemok
{
namespace http
{

enum method
{
	GET,
	POST, 
	HEAD, 
	PUT, 
	DELETE,
	TRACE,
	OPTIONS, 
	CONNECT,
	PATCH
};

enum version
{
	HTTP_10,
	HTTP_11
};

class request
{
public:
	request& uri(std::string u) { _uri = u; }

private:
	std::string _uri;
	method _method = method::GET;
	version _ver = version::HTTP_11;
};

class response
{
public:
	explicit response(int code)
	{
		this->code(code);
	}
	
	response& code(int code)
	{
		_code = code;
		return *this;
	}

private:
	int _code = 200;

};

inline request get(std::string uri)
{
	return request().uri(uri);
}

std::string receive(client& c);
void send(client& c, std::string buf);
	
class http : public basic_mock<http>
{
public:
	using base_type = basic_mock<http>;

	http() {}

	using base_type::when;

	http& when(request r);
	http& reply(response r);
};

}
}

