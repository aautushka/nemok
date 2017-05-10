#include "http.h"

namespace nemok
{

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
	return base_type::when(starts_with(r.str()));
}

http& http::reply(response r)
{
	auto str = r.str();
	return base_type::exec([str](auto& c){c.write(str.c_str(), str.size());});
}

} // namespace nemok

