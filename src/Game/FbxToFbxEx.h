#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FbxToFbxEx
{
	bool Convert(const uint8_t* fbx, size_t size, std::vector<uint8_t>& out, std::string& error);
}
