#pragma once

#include <string>

namespace StageSettings
{
	constexpr const char* kBaseFolder = "base";
	constexpr const char* kBaseFolderSlash = "base\\";

	std::string PathOf(int number);
	std::string NoteOf(int number);

	std::string Read(int number, const char* section, const char* key);
	bool Write(int number, const char* section, const char* key, const std::string& value);

	void Migrate();
}
