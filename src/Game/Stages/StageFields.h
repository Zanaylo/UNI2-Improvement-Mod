#pragma once

#include <string>
#include <vector>

namespace StageFields
{
	bool Read(int id, const char* key, std::string& out);
	bool Write(int id, const char* key, const std::string& value);

	std::vector<std::string> Edited(int id);
	bool Reset(int id, const std::vector<std::string>& keys);
}
