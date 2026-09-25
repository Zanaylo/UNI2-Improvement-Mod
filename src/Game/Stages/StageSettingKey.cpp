#include "Game/Stages/StageSettingKey.h"

#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Game/Stages/StageLibrary.h"

#include <Windows.h>

#include <set>

namespace {

constexpr const char* kPrefix = "Stage_";

std::set<std::string> g_carried;

bool Stored(const char* section, const std::string& key, char* out, DWORD size)
{
	GetPrivateProfileStringA(section, key.c_str(), "", out, size, Settings::GetIniPath().c_str());

	return out[0] != 0;
}

void Carry(const char* section, const std::string& from, const std::string& to)
{
	char value[256] = {};

	if (!Stored(section, from, value, sizeof(value)))
		return;

	char existing[256] = {};

	if (!Stored(section, to, existing, sizeof(existing)))
		Settings::SaveString(section, to.c_str(), value);

	Settings::SaveString(section, from.c_str(), nullptr);

	LOG("StageSettingKey: [%s] %s moved to %s", section, from.c_str(), to.c_str());
}

}

std::string StageSettingKey::For(const char* section, int libraryId, const std::string& legacy,
	std::initializer_list<const char*> suffixes)
{
	const std::string hash = StageLibrary::KeyOf(libraryId);

	if (hash.empty())
		return legacy;

	const std::string modern = kPrefix + hash;

	if (!g_carried.insert(std::string(section) + "|" + legacy + "|" + modern).second)
		return modern;

	for (const char* suffix : suffixes)
		Carry(section, legacy + suffix, modern + suffix);

	return modern;
}
