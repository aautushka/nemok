#pragma once
#include "server.h"

namespace nemok
{
namespace http
{

const std::map<int, const char*> status_codes
{
	{100, "Continue"},
	{101, "Switching Protocols"},
	{102, "Processing"},
	{200, "OK"},
	{201, "Created"},
	{202, "Accepted"},
	{203, "Non-Authoritative Information"},
	{204, "No Content"},
	{205, "Reset Content"},
	{206, "Partial Content"},
	{207, "Multi-Status"},
	{208, "Already Reported"},
	{226, "IM Used"},
	{300, "Multiple Choices"},
	{301, "Moved Permanently"},
	{302, "Found"},
	{303, "See Other"},
	{304, "Not Modified"},
	{305, "Use Proxy"},
	{306, "Switch Proxy"},
	{307, "Temporary Redirect"},
	{308, "Permanent Redirect"},
	{400, "Bad Request"},
	{401, "Unauthorized"},
	{402, "Payment Required"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{405, "Method Not Allowed"},
	{406, "Not Acceptable"},
	{407, "Proxy Authentication Required"},
	{408, "Request Timeout"},
	{409, "Conflict"},
	{410, "Gone"},
	{411, "Length Required"},
	{412, "Precondition Failed"},
	{413, "Payload Too Large"},
	{414, "URI Too Long"},
	{415, "Unsupported Media Type"},
	{416, "Range Not Satisfiable"},
	{417, "Expectation Failed"},
	{418, "I'm a teapot"},
	{421, "Misdirected Request"},
	{422, "Unprocessable Entity"},
	{423, "Locked"},
	{424, "Failed Dependency"},
	{426, "Upgrade Required"},
	{428, "Precondition Required"},
	{429, "Too Many Requests"},
	{431, "Request Header Fields Too Large"},
	{451, "Unavailable For Legal Reasons"},
	{500, "Internal Server Error"},
	{501, "Not Implemented"},
	{502, "Bad Gateway"},
	{503, "Service Unavailable"},
	{504, "Gateway Time-out"},
	{505, "HTTP Version Not Supported"},
	{506, "Variant Also Negotiates"},
	{507, "Insufficient Storage"},
	{508, "Loop Detected"},
	{510, "Not Extended"},
	{511, "Network Authentication Required"}
};

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

