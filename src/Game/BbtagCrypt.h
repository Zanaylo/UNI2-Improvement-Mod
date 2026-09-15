#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagCrypt
{
	std::string Md5(const std::string& text);

	std::string NameOf(const std::string& relative);

	void Decrypt(const std::string& name, std::vector<uint8_t>& data);
}
