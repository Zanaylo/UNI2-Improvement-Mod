#pragma once

#include <string>

namespace StageSettings
{
	std::string PathOf(int number);

	std::string Read(int number, const char* section, const char* key);
	bool Write(int number, const char* section, const char* key, const std::string& value);

	void Migrate();
}
