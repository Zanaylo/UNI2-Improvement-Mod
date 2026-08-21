#pragma once

#include <cstdint>
#include <string>

namespace PngPalette
{
	bool Read(const std::string& path, uint8_t* outRgba, std::string& outError);
	bool Write(const std::string& path, const uint8_t* rgba, std::string& outError);
}
