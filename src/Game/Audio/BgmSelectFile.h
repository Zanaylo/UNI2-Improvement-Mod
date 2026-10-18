#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BgmSelectFile
{
	struct Record
	{
		int number;
		std::string body;
	};

	struct Contents
	{
		std::string prefix;
		std::vector<Record> records;
	};

	std::vector<uint8_t> ReadBase();

	void Split(const std::vector<uint8_t>& blob, Contents& out);

	std::string Compose(const Contents& contents);

	std::string Body(int number, const std::string& name);
}
