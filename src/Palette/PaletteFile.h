#pragma once

#include <cstdint>
#include <string>

namespace PaletteFile
{
	constexpr int kColors = 256;
	constexpr int kBytes = kColors * 4;

	constexpr int kNameLength = 32;
	constexpr int kCreatorLength = 32;
	constexpr int kDescriptionLength = 64;

	constexpr const char* kExtension = ".pal";

	struct Info
	{
		char name[kNameLength];
		char creator[kCreatorLength];
		char description[kDescriptionLength];
	};

	bool Load(const std::string& path, uint8_t* colors, Info& info, uint8_t* effectColors = nullptr,
		bool* outHasEffect = nullptr);
	bool Save(const std::string& path, const uint8_t* colors, const Info& info,
		const uint8_t* effectColors = nullptr);
}
