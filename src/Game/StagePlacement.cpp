#include "Game/StagePlacement.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/ExtraStages.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace {

constexpr const char* kSection = "StagePlacement";
constexpr uintptr_t kScaleField = 0x6c;
constexpr uintptr_t kPositionField = 0x78;
constexpr int kAxes = 3;

std::map<int, StagePlacement::Place> g_shipped;
std::map<int, StagePlacement::Place> g_edited;
std::set<int> g_unknown;

int g_stage = -1;
char g_status[160] = "no stage is loaded";

std::string Key(int stage)
{
	char text[32] = {};
	sprintf_s(text, "Stage%03d", stage);

	return text;
}

void Describe(int stage)
{
	if (g_edited.find(stage) != g_edited.end())
	{
		sprintf_s(g_status, "stage %d was moved by hand", stage);
		return;
	}

	sprintf_s(g_status, "stage %d is in its original place", stage);
}

bool Sane(const StagePlacement::Place& place)
{
	for (int i = 0; i < kAxes; ++i)
	{
		if (!std::isfinite(place.scale[i]) || !std::isfinite(place.position[i]))
			return false;
	}

	return true;
}

bool Read(uintptr_t record, StagePlacement::Place& out)
{
	if (record == 0)
		return false;

	if (!TryReadMemory(out.scale, reinterpret_cast<const void*>(record + kScaleField),
		sizeof(out.scale)))
	{
		return false;
	}

	return TryReadMemory(out.position, reinterpret_cast<const void*>(record + kPositionField),
		sizeof(out.position)) && Sane(out);
}

bool Write(uintptr_t record, const StagePlacement::Place& place)
{
	if (record == 0 || !Sane(place))
		return false;

	if (!TryWriteMemory(reinterpret_cast<void*>(record + kScaleField), place.scale,
		sizeof(place.scale)))
	{
		return false;
	}

	return TryWriteMemory(reinterpret_cast<void*>(record + kPositionField), place.position,
		sizeof(place.position));
}

void Store(int stage, const StagePlacement::Place& place)
{
	char value[128] = {};

	sprintf_s(value, "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f", place.scale[0], place.scale[1],
		place.scale[2], place.position[0], place.position[1], place.position[2]);

	Settings::SaveString(kSection, Key(stage).c_str(), value);
}

bool Learn(int stage)
{
	if (g_shipped.find(stage) != g_shipped.end())
		return true;

	if (stage < 0 || stage >= GameOffsets::kBgRecordCount
		|| g_unknown.find(stage) != g_unknown.end())
	{
		return false;
	}

	StagePlacement::Place shipped = {};

	if (!Read(ExtraStages::RecordAt(stage), shipped))
	{
		g_unknown.insert(stage);
		return false;
	}

	g_shipped[stage] = shipped;

	return true;
}

bool Recall(int stage, StagePlacement::Place& out)
{
	char stored[128] = {};

	GetPrivateProfileStringA(kSection, Key(stage).c_str(), "", stored, sizeof(stored),
		Settings::GetIniPath().c_str());

	if (stored[0] == 0)
		return false;

	const int read = sscanf_s(stored, "%f,%f,%f,%f,%f,%f", &out.scale[0], &out.scale[1],
		&out.scale[2], &out.position[0], &out.position[1], &out.position[2]);

	return read == 6 && Sane(out);
}

}

void StagePlacement::Update()
{
	const uintptr_t pending = RvaToAddress(GameOffsets::kBgPendingNumber);

	if (!IsAddressInGameModule(pending))
		return;

	const int stage = *reinterpret_cast<const int*>(pending);

	if (stage < 0 || stage >= GameOffsets::kBgRecordCount)
		return;

	const uintptr_t record = ExtraStages::RecordAt(stage);

	if (record == 0)
		return;

	if (g_shipped.find(stage) == g_shipped.end())
	{
		g_unknown.clear();

		if (!Learn(stage))
			return;

		Place stored = {};

		if (Recall(stage, stored))
			g_edited[stage] = stored;
	}

	if (stage != g_stage)
	{
		g_stage = stage;
		Describe(stage);
	}

	const std::map<int, Place>::const_iterator edit = g_edited.find(stage);

	if (edit != g_edited.end())
		Write(record, edit->second);
}

int StagePlacement::Current()
{
	return g_stage;
}

bool StagePlacement::Of(int stage, Place& out)
{
	const std::map<int, Place>::const_iterator edit = g_edited.find(stage);

	if (edit != g_edited.end())
	{
		out = edit->second;
		return true;
	}

	if (Shipped(stage, out))
		return true;

	if (!Learn(stage))
		return false;

	Place stored = {};

	if (Recall(stage, stored))
	{
		g_edited[stage] = stored;
		out = stored;
		return true;
	}

	return Shipped(stage, out);
}

bool StagePlacement::Shipped(int stage, Place& out)
{
	const std::map<int, Place>::const_iterator known = g_shipped.find(stage);

	if (known == g_shipped.end())
		return false;

	out = known->second;
	return true;
}

void StagePlacement::Set(int stage, const Place& place)
{
	if (!Sane(place))
		return;

	g_edited[stage] = place;
	Store(stage, place);

	if (stage == g_stage)
		Describe(stage);
}

void StagePlacement::Forget(int stage)
{
	g_edited.erase(stage);
	Settings::SaveString(kSection, Key(stage).c_str(), "");

	Place shipped = {};

	if (Shipped(stage, shipped))
		Write(ExtraStages::RecordAt(stage), shipped);

	if (stage == g_stage)
		Describe(stage);
}

bool StagePlacement::Edited(int stage)
{
	return g_edited.find(stage) != g_edited.end();
}

const char* StagePlacement::StatusText()
{
	return g_status;
}
