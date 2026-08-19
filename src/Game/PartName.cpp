#include "Game/PartName.h"

#include "Core/Settings.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr const char* kSection = "PartNames";

bool KeyFor(int chara, int entry, char* out, int size)
{
	if (chara < 0 || entry < 0 || entry > 255)
		return false;

	sprintf_s(out, size, "chr%03d.%d", chara, entry);
	return true;
}

}

void PartName::Get(int chara, int entry, char* out, int size)
{
	if (out == nullptr || size <= 0)
		return;

	out[0] = '\0';

	char key[32] = {};
	if (!KeyFor(chara, entry, key, sizeof(key)))
		return;

	GetPrivateProfileStringA(kSection, key, "", out, size, Settings::GetIniPath().c_str());
}

void PartName::Set(int chara, int entry, const char* name)
{
	char key[32] = {};
	if (!KeyFor(chara, entry, key, sizeof(key)))
		return;

	Settings::SaveString(kSection, key, name != nullptr ? name : "");
}
