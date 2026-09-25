#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DataArchive
{
	bool Read(const char* folder, const char* file, std::vector<uint8_t>& out);

	bool List(const char* folder, std::vector<std::string>& out);

	void Folders(std::vector<std::string>& out);

	bool IsAvailable();
}
