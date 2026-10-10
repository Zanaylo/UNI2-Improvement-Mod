#include "Palette/NetworkPick.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/HexText.h"
#include "Palette/ColourSlots.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* kSection = "PaletteSlots";
constexpr const char* kCharaKey = "NetworkCharacter";
constexpr const char* kColourKey = "NetworkColour";
constexpr const char* kFileKey = "NetworkFile";
constexpr const char* kSignatureKey = "NetworkSignature";
constexpr int kSignatureDigits = PaletteSignature::kBytes * 2;
constexpr int kNone = -1;

NetworkPick::Pick g_pick = { kNone, kNone, "", false, {} };
bool g_loaded = false;

void SaveNumber(const char* key, int value)
{
	char text[16] = {};
	sprintf_s(text, "%d", value);

	Settings::SaveString(kSection, key, text);
}

void Load()
{
	if (g_loaded)
		return;

	const std::string path = Settings::GetIniPath();

	g_pick.chara = static_cast<int>(Ini::GetInt(kSection, kCharaKey, kNone, path.c_str()));
	g_pick.colour = static_cast<int>(Ini::GetInt(kSection, kColourKey, kNone, path.c_str()));
	Ini::GetString(kSection, kFileKey, "", g_pick.file, sizeof(g_pick.file), path.c_str());

	char digits[kSignatureDigits + 1] = {};
	Ini::GetString(kSection, kSignatureKey, "", digits, sizeof(digits), path.c_str());
	g_pick.hasSignature = HexText::Decode(digits, g_pick.signature, PaletteSignature::kBytes);

	g_loaded = true;
}

bool IsCharacter(int chara)
{
	return chara >= 0 && chara < ColourSlots::kCharacters;
}

}

void NetworkPick::Remember(int chara, int colour, const char* file, const uint8_t* signature)
{
	g_pick.chara = chara;
	g_pick.colour = colour;
	strncpy_s(g_pick.file, file != nullptr ? file : "", _TRUNCATE);
	g_pick.hasSignature = signature != nullptr;
	g_loaded = true;

	char digits[kSignatureDigits + 1] = {};

	if (g_pick.hasSignature)
	{
		memcpy(g_pick.signature, signature, PaletteSignature::kBytes);
		HexText::Encode(signature, PaletteSignature::kBytes, digits);
	}

	SaveNumber(kCharaKey, chara);
	SaveNumber(kColourKey, colour);
	Settings::SaveString(kSection, kFileKey, g_pick.file);
	Settings::SaveString(kSection, kSignatureKey, digits);
}

bool NetworkPick::Recall(Pick& out)
{
	Load();

	if (!IsCharacter(g_pick.chara))
		return false;

	out = g_pick;
	return true;
}

std::string NetworkPick::FileFor(int chara, int colour)
{
	Pick pick = {};

	if (Recall(pick) && pick.chara == chara && pick.colour == colour && pick.file[0] != '\0')
		return pick.file;

	return ColourSlots::BoundFile(chara, colour);
}

void NetworkPick::Reload()
{
	g_loaded = false;
}
