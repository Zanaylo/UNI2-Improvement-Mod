#include "Game/StageReplacements.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/StageArchive.h"
#include "Game/StageLibrary.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

using Replacement = StageReplacements::Replacement;

constexpr const char* kSection = "StageReplacements";
constexpr int kFirstNumber = 1;

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<Replacement> g_replaced;
std::vector<int> g_stale;

std::string Key(int number)
{
	char key[16] = {};
	sprintf_s(key, "Stage%d", number);

	return key;
}

bool Remembered(int number)
{
	return GetPrivateProfileIntA(kSection, Key(number).c_str(), 0, Settings::GetIniPath().c_str()) != 0;
}

bool Read(int number, Replacement& out)
{
	std::vector<uint8_t> blob;

	if (!ReadWholeFile(StageLibrary::NoteOf(number), blob) || blob.empty())
		return false;

	out.number = number;
	out.note.assign(blob.begin(), blob.end());

	std::string name;
	out.name = StageArchive::Field(out.note, "Name", name) ? StageArchive::Unquoted(name) : std::string();

	return true;
}

}

void StageReplacements::Load()
{
	std::vector<Replacement> replaced;
	std::vector<int> stale;

	for (int number = kFirstNumber; number <= StageLibrary::kDebugStage; ++number)
	{
		if (!StageLibrary::GameOwns(number))
			continue;

		Replacement replacement = {};
		const bool remembered = Remembered(number);

		if (!Read(number, replacement))
		{
			if (remembered)
				stale.push_back(number);

			continue;
		}

		if (!remembered)
			Settings::SaveString(kSection, Key(number).c_str(), "1");

		replaced.push_back(replacement);
	}

	AcquireSRWLockExclusive(&g_lock);
	g_replaced.swap(replaced);
	g_stale.swap(stale);
	ReleaseSRWLockExclusive(&g_lock);

	LOG("StageReplacements: %d of the game's stages carry a stage.txt, %d lost theirs",
		static_cast<int>(g_replaced.size()), static_cast<int>(g_stale.size()));
}

void StageReplacements::Snapshot(std::vector<Replacement>& out)
{
	AcquireSRWLockShared(&g_lock);
	out = g_replaced;
	ReleaseSRWLockShared(&g_lock);
}

void StageReplacements::Stale(std::vector<int>& out)
{
	AcquireSRWLockShared(&g_lock);
	out = g_stale;
	ReleaseSRWLockShared(&g_lock);
}

bool StageReplacements::Replaced(int number, std::string& name)
{
	AcquireSRWLockShared(&g_lock);

	const auto found = std::find_if(g_replaced.begin(), g_replaced.end(),
		[number](const Replacement& replacement) { return replacement.number == number; });
	const bool replaced = found != g_replaced.end();

	if (replaced)
		name = found->name;

	ReleaseSRWLockShared(&g_lock);

	return replaced;
}

void StageReplacements::Forget(int number)
{
	Settings::SaveString(kSection, Key(number).c_str(), nullptr);

	AcquireSRWLockExclusive(&g_lock);

	g_replaced.erase(std::remove_if(g_replaced.begin(), g_replaced.end(),
		[number](const Replacement& replacement) { return replacement.number == number; }), g_replaced.end());
	g_stale.erase(std::remove(g_stale.begin(), g_stale.end(), number), g_stale.end());

	ReleaseSRWLockExclusive(&g_lock);
}
