#pragma once

#include <string>

namespace StageFields
{
	bool Read(int id, const char* key, std::string& out);
	bool Write(int id, const char* key, const std::string& value);

	bool Edited(int id);
	bool Reset(int id);
}
