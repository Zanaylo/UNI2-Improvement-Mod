#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BbtagCrypt
{
	enum Key
	{
		Key_BBTAG,
		Key_P4U2,
	};

	std::string Md5(const std::string& text);

	std::string NameOf(const std::string& relative);

	void Decrypt(Key key, const std::string& name, std::vector<uint8_t>& data);
}
