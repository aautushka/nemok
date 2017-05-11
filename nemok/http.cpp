#include "http.h"

namespace nemok
{

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
		return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
	}

	const char* end()
	{
		return str_ + len_;
	}

	const char* cursor_ = nullptr;
	const char* str_ = nullptr;
	size_t len_ = 0;

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

class matches_request
{
public:
	explicit matches_request(http::request request): request_(std::move(request_)) {}

	bool operator ()(buffer_type& input)
	{
		const std::string HEAD_END("\r\n\r\n");
		const void* head_end = memmem(&input[0], input.size(), &HEAD_END[0], HEAD_END.size());

		if (head_end)
		{
			const size_t head_size = static_cast<const uint8_t*>(head_end) - &input[0];
			
			auto tokens = tokenize(reinterpret_cast<const char*>(&input[0]), input.size());
			if (tokens.size() >= 3)
			{
				auto method = http_method_from_str(tokens[0]);
				auto uri = tokens[1];
				auto ver = http_version_from_str(tokens[2]);

				if (method != HTTP_BAD_METHOD && ver != HTTP_BAD_VERSION)
				{
					http::request actual_request = http::request(ver).method(method).uri(uri);;

					if (request_.match(actual_request))
					{
						input.erase(input.begin(), input.begin() + head_size + HEAD_END.size());
						return true;
					}
				} 
			}

			return false;
		}
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

} // namespace nemok

