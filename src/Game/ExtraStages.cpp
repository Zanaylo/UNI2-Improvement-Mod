#include "Game/ExtraStages.h"

#include "Core/Settings.h"
#include "Core/TextEncoding.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <Windows.h>

namespace {

constexpr int kSettleFrames = 60;
constexpr int kFirstStage = 1;
constexpr int kFlagCount = 3;

constexpr uintptr_t kFlagFields[kFlagCount] = {
	GameOffsets::kBgRecordSelectDisable,
	GameOffsets::kBgRecordRandomDisable,
	GameOffsets::kBgRecordVsDisable,
};

struct Held
{
	int number;
	uint32_t original[kFlagCount];
};

std::vector<ExtraStages::Stage> g_stages;
std::vector<Held> g_held;

bool g_ready = false;
int g_countdown = 0;
char g_status[160] = "not looked yet";

uintptr_t RecordAt(int number)
{
	if (number < 0 || number >= GameOffsets::kBgRecordCount)
		return 0;

	const uintptr_t table = RvaToAddress(GameOffsets::kBgRecordTable);
	uint32_t record = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(table + number * sizeof(uint32_t)), record))
		return 0;

	return record;
}

bool ReadFlags(uintptr_t record, uint32_t* out)
{
	for (int i = 0; i < kFlagCount; ++i)
	{
		if (!TryReadDword(reinterpret_cast<const void*>(record + kFlagFields[i]), out[i]))
			return false;

		if (out[i] > 1)
			return false;
	}

	return true;
}

std::string ReadText(uintptr_t record, uintptr_t field, size_t maxBytes)
{
	std::vector<char> raw(maxBytes + 1, 0);

	if (!TryReadMemory(raw.data(), reinterpret_cast<const void*>(record + field), maxBytes))
		return std::string();

	std::string out;
	TextEncoding::ShiftJisToUtf8(raw.data(), maxBytes, out);

	return out;
}

const Held* HeldFor(int number)
{
	const auto found = std::find_if(g_held.begin(), g_held.end(),
		[number](const Held& held) { return held.number == number; });

	return found == g_held.end() ? nullptr : &*found;
}

void Write(int number, bool unlocked)
{
	const uintptr_t record = RecordAt(number);
	const Held* const held = HeldFor(number);

	if (record == 0 || held == nullptr)
		return;

	for (int i = 0; i < kFlagCount; ++i)
		TryWriteDword(reinterpret_cast<void*>(record + kFlagFields[i]),
			unlocked ? 0u : held->original[i]);
}

bool Lists(const char* list, int number)
{
	for (const char* at = list; *at != '\0';)
	{
		if (atoi(at) == number)
			return true;

		while (*at != '\0' && *at != ',')
			++at;

		while (*at == ',')
			++at;
	}

	return false;
}

void Save()
{
	std::string out;

	for (const ExtraStages::Stage& stage : g_stages)
	{
		if (!stage.unlocked)
			continue;

		if (!out.empty())
			out.push_back(',');

		char text[16] = {};
		sprintf_s(text, "%d", stage.number);
		out += text;
	}

	Settings::SaveString("Extras", "UnlockedStages", out.c_str());
}

void Discover()
{
	char saved[256] = {};

	GetPrivateProfileStringA("Extras", "UnlockedStages", "", saved, sizeof(saved),
		Settings::GetIniPath().c_str());

	for (int number = 0; number < GameOffsets::kBgRecordCount; ++number)
	{
		const uintptr_t record = RecordAt(number);

		if (record == 0)
			continue;

		Held held = {};
		held.number = number;

		if (!ReadFlags(record, held.original) || held.original[0] == 0)
			continue;

		ExtraStages::Stage stage = {};
		stage.number = number;
		stage.folder = ReadText(record, 0, GameOffsets::kBgRecordNameField);
		stage.name = ReadText(record, GameOffsets::kBgRecordNameField,
			GameOffsets::kBgRecordSelectDisable - GameOffsets::kBgRecordNameField);
		stage.unlocked = Lists(saved, number);

		g_held.push_back(held);
		g_stages.push_back(stage);
	}
}

void Settle()
{
	if (RecordAt(kFirstStage) == 0)
		return;

	Discover();
	g_ready = true;

	sprintf_s(g_status, "%d stage(s) the game builds and hides",
		static_cast<int>(g_stages.size()));

	LOG("ExtraStages: %s", g_status);

	for (const ExtraStages::Stage& stage : g_stages)
	{
		LOG("ExtraStages: stage %d '%s' in bg\\%s, %s", stage.number, stage.name.c_str(),
			stage.folder.c_str(), stage.unlocked ? "unlocked" : "left hidden");
	}
}

}

void ExtraStages::OnFrame()
{
	if (g_countdown > 0)
	{
		--g_countdown;
		return;
	}

	g_countdown = kSettleFrames;

	if (!g_ready)
	{
		Settle();
		return;
	}

	for (const Stage& stage : g_stages)
	{
		if (stage.unlocked)
			Write(stage.number, true);
	}
}

int ExtraStages::Count()
{
	return static_cast<int>(g_stages.size());
}

const ExtraStages::Stage* ExtraStages::Get(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	return &g_stages[index];
}

void ExtraStages::SetUnlocked(int number, bool unlocked)
{
	const auto found = std::find_if(g_stages.begin(), g_stages.end(),
		[number](const Stage& stage) { return stage.number == number; });

	if (found == g_stages.end() || found->unlocked == unlocked)
		return;

	found->unlocked = unlocked;
	Write(number, unlocked);
	Save();

	LOG("ExtraStages: stage %d '%s' %s", found->number, found->name.c_str(),
		unlocked ? "unlocked" : "hidden again");
}

bool ExtraStages::Ready()
{
	return g_ready;
}

const char* ExtraStages::StatusText()
{
	return g_status;
}
