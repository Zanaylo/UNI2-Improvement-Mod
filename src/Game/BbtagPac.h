#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BbtagPac
{
	typedef std::map<std::string, std::vector<uint8_t> > Files;

	bool IsArchive(const std::vector<uint8_t>& blob);

	bool Unpacked(const std::vector<uint8_t>& blob, std::vector<uint8_t>& out);

	bool Walk(const std::vector<uint8_t>& blob, Files& out);
}
