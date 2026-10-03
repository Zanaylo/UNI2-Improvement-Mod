#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace PacWriter
{
	struct Entry
	{
		std::string name;
		std::vector<uint8_t> data;
	};

	void Build(const std::vector<Entry>& entries, std::vector<uint8_t>& out);

	bool Packed(const std::vector<uint8_t>& archive, std::vector<uint8_t>& out);
}
