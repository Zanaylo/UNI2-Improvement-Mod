#pragma once

#include "Palette/PaletteSignature.h"

#include <cstdint>
#include <string>

namespace NetworkPick
{
	constexpr int kFileLength = 128;

	struct Pick
	{
		int chara;
		int colour;
		char file[kFileLength];
		bool hasSignature;
		uint8_t signature[PaletteSignature::kBytes];
	};

	void Remember(int chara, int colour, const char* file, const uint8_t* signature = nullptr);
	bool Recall(Pick& out);

	std::string FileFor(int chara, int colour);

	void Reload();
}
