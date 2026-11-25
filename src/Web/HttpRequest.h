#pragma once

#include <string>
#include <vector>

namespace HttpRequest
{
	struct Header
	{
		std::string name;
		std::string value;
	};

	struct Request
	{
		std::string method;
		std::string url;
		std::vector<Header> headers;
		std::string body;
	};

	struct Response
	{
		int status = 0;
		std::string body;
	};

	bool Send(const Request& request, Response& out, std::string& outError);
}
