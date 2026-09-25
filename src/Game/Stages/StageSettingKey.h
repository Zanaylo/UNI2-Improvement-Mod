#pragma once

#include <initializer_list>
#include <string>

namespace StageSettingKey
{
	std::string For(const char* section, int libraryId, const std::string& legacy,
		std::initializer_list<const char*> suffixes);
}
