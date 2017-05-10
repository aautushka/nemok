#pragma once
#include <sstream>
#include "server.h"

namespace nemok
{

const std::map<int, const char*> http_status_codes
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

inline std::string desc_http_code(int code)
{
	auto i = http_status_codes.find(code);
	return i == std::end(http_status_codes) ? "" : i->second;
}

enum http_version
{
	HTTP_10,
	HTTP_11
};

enum http_method
{
	HTTP_GET,
	HTTP_POST, 
	HTTP_HEAD, 
	HTTP_PUT, 
	HTTP_DELETE,
	HTTP_TRACE,
	HTTP_OPTIONS, 
	HTTP_CONNECT,
	HTTP_PATCH
};

inline std::string http_method_to_str(http_method method)
{
	switch (method)
	{
	case HTTP_GET: return "GET";
	case HTTP_POST: return "POST";
	case HTTP_HEAD: return "HEAD";
	case HTTP_PUT: return "PUT"; 
	case HTTP_DELETE: return "DELETE";
	case HTTP_TRACE: return "TRACE";
	case HTTP_OPTIONS: return "OPTIONS";
	case HTTP_CONNECT: return "CONNECT";
	case HTTP_PATCH: return "PATCH";
	default: assert(false);
	};

	return "";
}

inline std::ostream& operator <<(std::ostream& stream, http_method method)
{
	return stream << http_method_to_str(method);
}

inline std::string http_version_to_str(http_version ver)
{
	switch (ver)
	{
		case HTTP_11: return "HTTP/1.1";
		case HTTP_10: return "HTTP/1.0";
		default: assert(false);
	};

	return "";
}

inline std::ostream& operator <<(std::ostream& stream, http_version ver)
{
	return stream << http_version_to_str(ver);
}

class http_request
{
public:
	http_request& uri(std::string u) { uri_ = u; }

	std::string str() const
	{
		std::stringstream ss;
		ss << method_ << " " << uri_ << " " << ver_ << "\r\n\r\n";
		return ss.str();
	}

private:
	std::string uri_;
	http_method method_ = HTTP_GET;
	http_version ver_ = HTTP_11;
};

class http_response
{
public:
	explicit http_response(int code)
	{
		this->code(code);
	}
	
	http_response& code(int code)
	{
		code_ = code;
		return *this;
	}

	std::string str() const
	{
		std::stringstream ss;
		ss << ver_ << " " << code_ << " " << desc_http_code(code_) << "\r\n\r\n";
		return ss.str();
	}

private:
	int code_ = 200;
	http_version ver_ = HTTP_11;
};


class http : public basic_mock<http>
{
public:
	using base_type = basic_mock<http>;
	using response = http_response;
	using request = http_request;

	http() {}

	using base_type::when;

	http& when(request r);
	http& reply(response r);

	static std::string receive(client& c);
	static void send(client& c, std::string buf);
	static inline request get(std::string uri) 
	{
		return request().uri(uri);
	}
	
};

} // namespace nemok

