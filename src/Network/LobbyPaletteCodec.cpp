#include "Network/LobbyPaletteCodec.h"

#include "Core/HexText.h"
#include "Palette/ColourSlots.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kVersion = 1;
constexpr int kChannels = 3;
constexpr uint8_t kOpaque = 0xff;
constexpr char kSeparator = ':';
constexpr int kSignatureDigits = PaletteSignature::kBytes * 2;

bool IsPick(int chara, int colour)
{
	return chara >= 0 && chara < ColourSlots::kCharacters && colour >= 0 && colour < ColourSlots::kGameColours;
}

void PackRgb(const uint8_t* rgba, uint8_t* rgb)
{
	for (int i = 0; i < LobbyPaletteCodec::kColours; ++i)
		memcpy(rgb + i * kChannels, rgba + i * 4, kChannels);
}

void UnpackRgb(const uint8_t* rgb, uint8_t* rgba)
{
	for (int i = 0; i < LobbyPaletteCodec::kColours; ++i)
	{
		memcpy(rgba + i * 4, rgb + i * kChannels, kChannels);
		rgba[i * 4 + 3] = kOpaque;
	}
}

bool DecodeSignature(const char* text, uint8_t* signature)
{
	if (strlen(text) <= static_cast<size_t>(kSignatureDigits) || text[kSignatureDigits] != kSeparator)
		return false;

	char digits[kSignatureDigits + 1] = {};
	memcpy(digits, text, kSignatureDigits);

	return HexText::Decode(digits, signature, PaletteSignature::kBytes);
}

}

bool LobbyPaletteCodec::Encode(int chara, int colour, const uint8_t* signature, const uint8_t* rgba, char* out,
	int size)
{
	if (signature == nullptr || rgba == nullptr || out == nullptr || size < kTextBytes || !IsPick(chara, colour))
		return false;

	const int header = sprintf_s(out, size, "%d:%d:%d:", kVersion, chara, colour);

	if (header <= 0)
		return false;

	char* at = out + header;

	HexText::Encode(signature, PaletteSignature::kBytes, at);
	at += kSignatureDigits;
	*at++ = kSeparator;

	uint8_t rgb[kRgbBytes] = {};
	PackRgb(rgba, rgb);
	HexText::Encode(rgb, kRgbBytes, at);

	return true;
}

bool LobbyPaletteCodec::Decode(const char* text, Entry& out)
{
	if (text == nullptr)
		return false;

	int version = 0;
	int chara = -1;
	int colour = -1;
	int consumed = 0;

	if (sscanf_s(text, "%d:%d:%d:%n", &version, &chara, &colour, &consumed) != 3 || consumed == 0)
		return false;

	if (version != kVersion || !IsPick(chara, colour))
		return false;

	const char* const signature = text + consumed;

	if (!DecodeSignature(signature, out.signature))
		return false;

	uint8_t rgb[kRgbBytes] = {};

	if (!HexText::Decode(signature + kSignatureDigits + 1, rgb, kRgbBytes))
		return false;

	UnpackRgb(rgb, out.rgba);
	out.chara = chara;
	out.colour = colour;
	return true;
}
