#pragma once

#include "Palette/PaletteSignature.h"

#include <cstdint>

namespace LobbyPaletteCodec
{
	constexpr int kColours = 256;
	constexpr int kRgbaBytes = kColours * 4;
	constexpr int kRgbBytes = kColours * 3;
	constexpr int kHeaderBytes = 16;
	constexpr int kTextBytes = kHeaderBytes + (PaletteSignature::kBytes + 1 + kRgbBytes) * 2 + 1;
	constexpr int kEffectEntryBytes = 4;
	constexpr int kEffectTextBytes = kHeaderBytes + kColours * kEffectEntryBytes * 2 + 1;

	struct Entry
	{
		int chara;
		int colour;
		uint8_t signature[PaletteSignature::kBytes];
		uint8_t rgba[kRgbaBytes];
	};

	bool Encode(int chara, int colour, const uint8_t* signature, const uint8_t* rgba, char* out, int size);
	bool Decode(const char* text, Entry& out);

	bool EncodeEffects(const uint8_t* block, char* out, int size);
	bool DecodeEffects(const char* text, uint8_t* block);
}
