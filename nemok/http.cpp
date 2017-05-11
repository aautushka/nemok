#include "http.h"

namespace nemok
{
template <typename T, typename U>
T lexical_cast(U u)
{
	std::stringstream ss;
	ss << u;

	T t;
	ss >> t;
	return std::move(t);
}

class tokenizer
{
public:
	void tokenize(const char* str, size_t len)
	{
		str_ = str;
		len_ = len;
		cursor_ = str;
	}

	std::pair<const char*, size_t> next()
	{
		skip_blanks();	

		if (!finished())
		{
			const char* token = cursor_;
			skip_token();
			const char* token_end = finished() ? end() : cursor_;
			return std::make_pair(token, token_end - token);
			
		}

		return std::make_pair<const char*, size_t>(nullptr, 0);
	}

	bool finished()
	{
		return cursor_ >= str_ + len_;
	}

	void split_on(std::string blanks)
	{
		blanks_ = std::move(blanks);
	}

private:

	void skip_blanks()
	{
		for (;!finished() && blank_under_cursor(); ++cursor_);
	}

	void skip_token()
	{
		for (;!finished() && !blank_under_cursor(); ++cursor_);
	}

	bool blank_under_cursor()
	{
		const char ch = *cursor_;
		return std::string::npos != blanks_.find(ch);
	}

	const char* end()
	{
		return str_ + len_;
	}

	const char* cursor_ = nullptr;
	const char* str_ = nullptr;
	size_t len_ = 0;
	std::string blanks_ = " \t\r\n";
};

std::vector<std::string> tokenize(const char* str, size_t len)
{
	std::vector<std::string> ret;
	tokenizer tok;
	tok.tokenize(str, len);

	do
	{
		auto token = tok.next();
		if (token.first)
		{
			ret.push_back(std::string(token.first, token.second));
		}
	}
	while (!tok.finished());

	return std::move(ret);
}

std::vector<std::string> tokenize(const std::string& str)
{
	return tokenize(str.c_str(), str.size());
}

std::pair<std::string, std::string> split_pair(const std::string& input, char sep)
{
	auto pos = input.find(sep);
	if (pos != std::string::npos)
	{
		return std::make_pair(input.substr(0, pos), input.substr(pos + 1));
	}

	return std::make_pair(std::move(input), std::string());
}

void trim_left(std::string& s)
{
	size_t trim_count = 0;
	for (char ch : s)
	{
		if (ch == ' ')
		{
			++trim_count;
		}
		else
		{
			break;
		}
	}

	s.erase(0, trim_count);
}

void trim_right(std::string& s)
{
	size_t trim_count = 0;
	for (auto i = s.rbegin(); i != s.rend(); ++i)
	{
		if (*i == ' ')
		{
			++trim_count;
		}
		else
		{
			break;
		}
	}

	s.erase(s.size() - trim_count);
}

void trim(std::string& s)
{
	trim_left(s);
	trim_right(s);
}

namespace wire
{

class headers
{
public:
	bool has(const std::string& name) const
	{
		auto i = pairs_.find(name);
		return i != pairs_.end();
	}

	template <typename T>
	T get(const std::string& name) const
	{
		auto i = pairs_.find(name);
		return lexical_cast<T>(i->second);
	}

	template <typename T>
	T get_opt(const std::string& name, T default_val = T()) const
	{
		if (has(name))
		{
			return get<T>(name);
		}

		return default_val;
	}

	void add(const std::string& name, const std::string& value)
	{
		pairs_[name] = value;
		ordered_.emplace_back(std::make_pair(name, value));
	}

	void parse(const std::string& input)
	{
		tokenizer tok;
		tok.split_on("\r\n");
		tok.tokenize(input.c_str(), input.size());
		
		auto token = tok.next();
		while (token.first != nullptr)
		{
			auto pair = split_pair(std::string(token.first, token.second), ':');
			trim(pair.first);
			trim(pair.second);
			add(pair.first, pair.second);
			token = tok.next();
		}
	}

	size_t count() const
	{
		return pairs_.size();
	}

	std::pair<std::string, std::string> operator [](int i) const
	{
		return ordered_[i];
	}

private:
	using map_type = std::map<std::string, std::string>;
	using value_type = map_type::value_type;
	map_type pairs_; 
	std::vector<value_type> ordered_; 
};

class request
{
public:
	bool parse(const std::string& input)
	{
		const std::string HEAD_END("\r\n\r\n");
		const char* const head_ptr = &input[0];
		const void* const found = memmem(head_ptr, input.size(), &HEAD_END[0], HEAD_END.size());
		const char* const head_end = static_cast<const char*>(found);

		if (!head_end)
		{
			return false;
		}

		const size_t head_size = head_end - input.c_str();
		headers_.parse(input.substr(0, head_size));

		auto content_len = headers_.get_opt<size_t>("Content-Length");
		auto content_ptr = head_end + HEAD_END.size();

		// check if out of bounds
		auto packet_size = content_ptr + content_len - &input[0];
		if (packet_size > input.size())
		{
			return false;
		}

		content_ = input.substr(content_ptr - head_ptr, content_len);
		
		if (headers_.count() > 0)
		{
			auto first_header = headers_[0];

			auto tokens = tokenize(first_header.first);
			if (tokens.size() >= 3)
			{
				method_ = http_method_from_str(tokens[0]);
				uri_ = tokens[1];
				version_ = http_version_from_str(tokens[2]);

				if (method_ == HTTP_BAD_METHOD || version_ == HTTP_BAD_VERSION)
				{
					return false;
				}
			}
		}

		size_ = packet_size;
		return true;
	}

	http_method method() const
	{
		return method_;
	}

	http_version version() const
	{
		return version_;
	}

	const std::string& content() const
	{
		return content_;
	}

	const std::string& uri() const
	{
		return uri_;
	}

	size_t size() const
	{
		return size_;
	}

	std::pair<std::string, std::string> get_header(int i) const
	{
		return headers_[i];
	}

	size_t count_headers() const
	{
		return headers_.count();
	}

private:
	headers headers_;
	int status_ = 0;
	http_method method_ = HTTP_BAD_METHOD;
	http_version version_ = HTTP_BAD_VERSION;
	std::string uri_;
	std::string content_;
	size_t size_ = 0;
};

} // namespace wire


class matches_request
{
public:
	explicit matches_request(http::request request): request_(std::move(request)) {}

	bool operator ()(buffer_type& input)
	{
		wire::request wire_request;
		if (wire_request.parse(std::string(input.begin(), input.end())))
		{
			http::request actual_request = http::request(wire_request.version())
				.method(wire_request.method())
				.uri(wire_request.uri())
				.content(wire_request.content());

			if (wire_request.count_headers() > 1)
			{
				actual_request.header(wire_request.get_header(1));
			}

			if (request_.match(actual_request))
			{
				input.erase(input.begin(), input.begin() + wire_request.size());
				return true;
			}
		}
		return false;
	}

private:
	http::request request_;
};

bool ends_with(const std::string& input, const std::string& test)
{
	return input.size() >= test.size() &&
		0 == input.compare(input.size() - test.size(), test.size(), test);
}


std::string http::receive(client& c)
{
	char ch;
	std::string ret;
	while (!ends_with(ret, "\r\n\r\n"))
	{
		c.read(&ch, 1);
		ret.append(1, ch);
	}

	wire::headers headers;
	headers.parse(ret);

	if (headers.has("Content-Length"))
	{
		auto content_len = headers.get<size_t>("Content-Length");
		auto head_len = ret.size();
		ret.resize(head_len + content_len);
		c.read(&ret[head_len], content_len);
	}

	return std::move(ret);
}

void http::send(client& c, std::string buf)
{
	c.write_all(buf.c_str(), buf.size());
}

http& http::when(request r)
{
	return base_type::when(matches_request(r));
}

http& http::reply(response r)
{
	auto str = r.str();
	return base_type::exec([str](auto& c){c.write(str.c_str(), str.size());});
}

http& http::reply(int status_code)
{
	return reply(response(status_code));
}

} // namespace nemok

