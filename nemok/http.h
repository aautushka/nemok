#pragma once
#include <sstream>
#include <experimental/optional>
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
	HTTP_BAD_VERSION,
	HTTP_10,
	HTTP_11
};

enum http_method
{
	HTTP_BAD_METHOD,
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

inline http_method http_method_from_str(const std::string str)
{
	if (str == "GET")
	{
		return HTTP_GET;
	}
	else if (str == "POST")
	{
		return HTTP_POST;
	}
	else if (str == "HEAD")
	{
		return HTTP_HEAD;
	}

	return HTTP_BAD_METHOD;
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

inline http_version http_version_from_str(const std::string& str)
{
	if (str == "HTTP/1.1")
	{
		return HTTP_11;
	}

	return HTTP_BAD_VERSION;
}

inline std::ostream& operator <<(std::ostream& stream, http_version ver)
{
	return stream << http_version_to_str(ver);
}

class http_request
{
public:
	using self_type = http_request;

	self_type& uri(std::string u) { uri_ = u; return *this; }
	self_type& method(http_method m) { method_ = m; return *this; }
	self_type& content(std::string c) { content_ = c; return *this; }

	self_type& header(std::string key, std::string val)
	{
		return header(std::make_pair(key, val));
	}

	self_type& header(std::pair<std::string, std::string> key_val)
	{
		header_ = std::move(key_val);
		return *this;
	}

	explicit http_request(http_method m) { method(m); }
	explicit http_request(http_version v) { ver_ = v; }
	explicit http_request(std::string uri) { uri = std::move(uri); }
	http_request() = default;

	std::string str() const
	{
		std::stringstream ss;

		ss << method() << " " << uri() << " " << version() << "\r\n";
		ss << "Content-Length: " << content().size() << "\r\n";
	
		if (header_)
		{
			ss << header_->first << ": " << header_->second << "\r\n";
		}

		ss << "\r\n\r\n";
		ss << content();

		return ss.str();
	}

	bool match(const http_request& rhs) const
	{
		const bool match_uri = match_opt(uri_, rhs.uri_); 
		const bool match_ver = match_opt(ver_, rhs.ver_);
		const bool match_method = match_opt(method_, rhs.method_);
		const bool match_content = match_opt(content_, rhs.content_);
		const bool match_header = match_opt(header_, rhs.header_);

		return match_uri && match_ver && match_method && match_content && match_header;
	}

private:
	template <typename T>
	static bool match_opt(const T& lhs, const T& rhs)
	{
		return !lhs || !rhs || *lhs == *rhs;
	}

	std::string uri() const
	{
		return uri_ ? *uri_ : "/";
	}

	http_method method() const
	{
		return method_ ? *method_ : HTTP_GET;
	}

	http_version version() const
	{
		return ver_ ? *ver_ : HTTP_11;
	}

	std::string content() const
	{
		return content_ ? *content_ : "";
	}

	template <typename T> 
	using optional = std::experimental::optional<T>;

	optional<std::string> uri_;
	optional<http_method> method_;
	optional<http_version> ver_;
	optional<std::string> content_;	
	optional<std::pair<std::string, std::string>> header_;
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


class http final : public basic_mock<http>
{
public:
	using base_type = basic_mock<http>;
	using response = http_response;
	using request = http_request;

	http() {}

	using base_type::when;

	http& when(request r);

	http& when_unexpected()
	{
		return when(unexpected());
	}

	http& reply(response r);
	http& reply(int status_code);

	static std::string receive(client& c);
	static void send(client& c, std::string buf);

	static inline request GET(std::string uri) 
	{
		return request(HTTP_GET).uri(uri);
	}

	static inline request GET()
	{
		return request(HTTP_GET);
	}

	static inline request POST(std::string uri)
	{
		return request().method(HTTP_POST).uri(uri);
	}

	static inline request POST()
	{
		return request().method(HTTP_POST);
	}

	static inline request HEAD(std::string uri)
	{
		return request().method(HTTP_HEAD).uri(uri);
	}

	static inline request PUT(std::string uri)
	{
		return request().method(HTTP_PUT).uri(uri);
	}

	static inline request DELETE(std::string uri)
	{
		return request().method(HTTP_DELETE).uri(uri);
	}

	static inline request TRACE(std::string uri)
	{
		return request().method(HTTP_TRACE).uri(uri);
	}

	static inline request OPTIONS(std::string uri)
	{
		return request().method(HTTP_OPTIONS).uri(uri);
	}

	static inline request CONNECT(std::string uri)
	{
		return request().method(HTTP_CONNECT).uri(uri);
	}

	static inline request PATCH(std::string uri)
	{
		return request().method(HTTP_PATCH).uri(uri);
	}

	static inline request unexpected()
	{
		return request();	
	}
};

} // namespace nemok

