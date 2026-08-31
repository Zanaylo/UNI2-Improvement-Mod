#pragma once

#include <cstdint>
#include <string>

namespace Http
{
	class Progress
	{
	public:
		virtual ~Progress() = default;

		virtual bool OnProgress(uint64_t received, uint64_t total) = 0;
	};

	std::string HostOf(const std::string& url);

	bool GetText(const std::string& url, std::string& out, std::string& outError);

	bool Download(const std::string& url, const std::string& path, Progress* progress,
		std::string& outError);

	bool Sha256OfFile(const std::string& path, std::string& outHex, std::string& outError);
}
