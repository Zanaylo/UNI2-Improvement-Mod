#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace PngPalette
{
	struct Sheet
	{
		const uint8_t* data;
		size_t size;
	};

	bool Read(const std::string& path, const std::vector<Sheet>& sheets, uint8_t* outRgba,
		std::string& outError);
	bool Write(const std::string& path, const uint8_t* rgba, std::string& outError);

	bool Recolour(const std::string& path, const uint8_t* basePng, size_t baseSize,
		const uint8_t* rgba, std::string& outError);
}
