#include "Game/Stages/StagePlacement.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/BgCeiling.h"
#include "Game/Stages/ExtraStages.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StageSettings.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace {

constexpr const char* kSection = "StagePlacement";
constexpr const char* kPlaceKey = "Place";
constexpr uintptr_t kPlaceField = 0x6c;
constexpr int kFloats = sizeof(StagePlacement::Place) / sizeof(float);
constexpr int kPlacementFloats = 6;

static_assert(sizeof(StagePlacement::Place) == 0x94 - 0x6c, "Place mirrors record +0x6c..+0x94");

std::map<int, StagePlacement::Place> g_shipped;
std::map<int, StagePlacement::Place> g_edited;
std::set<int> g_unknown;

int g_stage = -1;
long g_revision = -1;
char g_status[160] = "no stage is loaded";

int FolderNumber(int stage)
{
	const int id = StageLibrary::IdForSlot(stage);

	return id >= 0 ? id : stage;
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

const float* Floats(const StagePlacement::Place& place)
{
	return reinterpret_cast<const float*>(&place);
}

float* Floats(StagePlacement::Place& place)
{
	return reinterpret_cast<float*>(&place);
}

bool Sane(const StagePlacement::Place& place)
{
	for (int i = 0; i < kFloats; ++i)
	{
		if (!std::isfinite(Floats(place)[i]))
			return false;
	}

	return true;
}

bool Read(uintptr_t record, StagePlacement::Place& out)
{
	if (record == 0)
		return false;

	return TryReadMemory(&out, reinterpret_cast<const void*>(record + kPlaceField), sizeof(out))
		&& Sane(out);
}

bool Write(uintptr_t record, const StagePlacement::Place& place)
{
	if (record == 0 || !Sane(place))
		return false;

	return TryWriteMemory(reinterpret_cast<void*>(record + kPlaceField), &place, sizeof(place));
}

void Store(int stage, const StagePlacement::Place& place)
{
	std::string value;

	for (int i = 0; i < kFloats; ++i)
	{
		char number[32] = {};
		sprintf_s(number, "%s%.4f", i == 0 ? "" : ",", Floats(place)[i]);
		value += number;
	}

	StageSettings::Write(FolderNumber(stage), kSection, kPlaceKey, value);
}

bool Learn(int stage)
{
	if (g_shipped.find(stage) != g_shipped.end())
		return true;

	if (stage < 0 || stage >= BgCeiling::Numbers()
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

void ForgetMovedSlots()
{
	const long revision = StageLibrary::Revision();

	if (revision == g_revision)
		return;

	g_revision = revision;
	g_shipped.clear();
	g_edited.clear();
	g_unknown.clear();
	g_stage = -1;
}

bool Recall(int stage, StagePlacement::Place& out)
{
	const std::string stored = StageSettings::Read(FolderNumber(stage), kSection, kPlaceKey);

	if (stored.empty() || !StagePlacement::Shipped(stage, out))
		return false;

	int read = 0;
	const char* at = stored.c_str();

	while (read < kFloats && at != nullptr && sscanf_s(at, "%f", &Floats(out)[read]) == 1)
	{
		++read;
		at = strchr(at, ',');
		at = at == nullptr ? nullptr : at + 1;
	}

	return (read == kPlacementFloats || read == kFloats) && Sane(out);
}

bool Apply(int stage)
{
	if (stage < 0)
		return false;

	const uintptr_t record = ExtraStages::RecordAt(stage);

	if (record == 0)
		return false;

	if (g_shipped.find(stage) == g_shipped.end())
	{
		g_unknown.clear();

		if (!Learn(stage))
			return false;

		StagePlacement::Place stored = {};

		if (Recall(stage, stored))
			g_edited[stage] = stored;
	}

	const std::map<int, StagePlacement::Place>::const_iterator edit = g_edited.find(stage);

	if (edit != g_edited.end())
		Write(record, edit->second);

	return true;
}

}

void StagePlacement::Update()
{
	ForgetMovedSlots();

	const int pending = ExtraStages::PendingStage();
	const int drawn = ExtraStages::DrawnStage();

	if (pending != drawn)
		Apply(pending);

	if (!Apply(drawn) || drawn == g_stage)
		return;

	g_stage = drawn;
	Describe(drawn);
}

int StagePlacement::Current()
{
	return g_stage;
}

bool StagePlacement::Of(int stage, Place& out)
{
	ForgetMovedSlots();

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
	StageSettings::Write(FolderNumber(stage), kSection, kPlaceKey, std::string());

	Place shipped = {};

	if (Shipped(stage, shipped))
		Write(ExtraStages::RecordAt(stage), shipped);

	if (stage == g_stage)
		Describe(stage);
}

bool StagePlacement::Edited(int stage)
{
	ForgetMovedSlots();

	return g_edited.find(stage) != g_edited.end();
}

const char* StagePlacement::StatusText()
{
	return g_status;
}
