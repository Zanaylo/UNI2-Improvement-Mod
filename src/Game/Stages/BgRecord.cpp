#include "Game/Stages/BgRecord.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/ExtraStages.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Stages/StageArchive.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr size_t kFolderBytes = GameOffsets::kBgRecordNameField;
constexpr size_t kNameBytes = GameOffsets::kBgRecordSelectDisable
	- GameOffsets::kBgRecordNameField;
constexpr int kValues = 4;

struct Field
{
	const char* key;
	uint16_t at;
	uint8_t count;
	bool real;
	const char* fallback;
};

const Field kFields[] = {
	{ "Scale", 0x6c, 3, true, "[ 1.0, 1.0, 1.0 ]" },
	{ "Position", 0x78, 3, true, "[ 0.0, 0.0, 0.0 ]" },
	{ "FOV", 0x84, 1, true, "45.0" },
	{ "VanishingPoint", 0x88, 1, true, "0.0" },
	{ "ViewRotationX", 0x8c, 1, true, "0.0" },
	{ "ViewRotationY", 0x90, 1, true, "0.0" },
	{ "IsFog", 0x98, 1, false, "0" },
	{ "FogStart", 0x9c, 1, true, "0.0" },
	{ "FogEnd", 0xa0, 1, true, "0.0" },
	{ "FogColor", 0xa4, 4, true, "[ 0.0, 0.0, 0.0, 0.0 ]" },
	{ "IsBloom", 0xb4, 1, false, "0" },
	{ "BGBloomEnable", 0xb8, 1, false, "0" },
	{ "BGBloomBlightness", 0xbc, 1, true, "0.0" },
	{ "BGBloomPower", 0xc0, 1, true, "0.0" },
	{ "BGBloomBiassR", 0xc4, 1, true, "1.0" },
	{ "BGBloomBiassG", 0xc8, 1, true, "1.0" },
	{ "BGBloomBiassB", 0xcc, 1, true, "1.0" },
	{ "BGBloomBlurRadius", 0xd0, 1, true, "0.0" },
	{ "BGBloomTextureSize", 0xd4, 1, false, "256" },
	{ "BGBloomAlpha", 0xd8, 1, true, "0.5" },
	{ "BGTinyFXAAEnable", 0xdc, 1, false, "0" },
	{ "BGTinyFXAAThreshold", 0xe0, 1, true, "0.2" },
	{ "BGTinyFXAALerpT", 0xe4, 1, true, "0.5" },
	{ "ViewGrid", 0xec, 1, false, "0" },
	{ "MSAA", 0xf0, 1, false, "0" },
	{ "TargetW", 0xf4, 1, false, "1280" },
	{ "TargetH", 0xf8, 1, false, "720" },
	{ "ShadowLightType", 0xfc, 1, false, "0" },
	{ "ShadowReflexColor", 0x1e4, 4, true, "[ 0.0, 0.0, 0.0, 0.0 ]" },
	{ "LightType", 0x1f4, 1, false, "0" },
	{ "LightColor", 0x1f8, 4, true, "[ 0.0, 0.0, 0.0, 0.0 ]" },
	{ "StageW", 0x208, 1, false, "2560" },
	{ "BlanchStage", 0x20c, 1, false, "-1" },
	{ "BlanchChara", 0x210, 1, false, "0" },
	{ "CharaColor", 0x214, 4, true, "[ 0.0, 0.0, 0.0, 0.0 ]" },
	{ "ShadowScale", 0x224, 1, true, "0.6" },
	{ "ShadowAlpha", 0x228, 1, true, "0.7" },
	{ "InterpolationType", 0x22c, 1, false, "0" },
	{ "InterpolationNum", 0x230, 1, false, "0" },
};

constexpr uintptr_t kThumbnail = 0x234;

int Numbers(const std::string& text, double* out, int wanted)
{
	const char* at = text.c_str();
	int found = 0;

	while (*at != 0 && found < wanted)
	{
		while (*at != 0 && strchr("[], \t\r\n", *at) != nullptr)
			++at;

		if (*at == 0)
			break;

		char* end = nullptr;
		const double value = strtod(at, &end);

		if (end == at)
			break;

		out[found++] = value;
		at = end;
	}

	return found;
}

uint32_t Raw(double value, bool real)
{
	if (!real)
		return static_cast<uint32_t>(static_cast<int32_t>(value));

	const float narrow = static_cast<float>(value);
	uint32_t bits = 0;
	memcpy(&bits, &narrow, sizeof(bits));

	return bits;
}

bool Put(uintptr_t record, const Field& field, const std::string& text)
{
	double values[kValues] = {};
	const int found = Numbers(text, values, field.count);

	if (found != field.count)
		return false;

	for (int i = 0; i < found; ++i)
	{
		void* const at = reinterpret_cast<void*>(record + field.at + i * sizeof(uint32_t));

		if (!TryWriteDword(at, Raw(values[i], field.real)))
			return false;
	}

	return true;
}

bool PutText(uintptr_t record, uintptr_t at, const std::string& text, size_t bytes)
{
	std::vector<char> field(bytes, 0);
	strncpy_s(field.data(), field.size(), text.c_str(), _TRUNCATE);

	return TryWriteMemory(reinterpret_cast<void*>(record + at), field.data(), field.size());
}

}

bool BgRecord::Reachable()
{
	return ExtraStages::RecordAt(1) != 0;
}

bool BgRecord::FolderOf(int slot, std::string& out)
{
	const uintptr_t record = ExtraStages::RecordAt(slot);

	if (record == 0)
		return false;

	char folder[kFolderBytes + 1] = {};

	if (!TryReadMemory(folder, reinterpret_cast<const void*>(record), kFolderBytes))
		return false;

	out = folder;
	return true;
}

bool BgRecord::Apply(int slot, int id, const std::string& block, const std::string& shiftJisName,
	int thumbnail)
{
	const uintptr_t record = ExtraStages::RecordAt(slot);

	if (record == 0)
		return false;

	char folder[16] = {};
	sprintf_s(folder, "bg%03d", id);

	if (!PutText(record, 0, folder, kFolderBytes))
		return false;

	int carried = 0;

	for (const Field& field : kFields)
	{
		std::string text;

		if (!StageArchive::Field(block, field.key, text))
		{
			Put(record, field, field.fallback);
			continue;
		}

		if (Put(record, field, text))
			++carried;
	}

	if (!shiftJisName.empty())
		PutText(record, GameOffsets::kBgRecordNameField, shiftJisName, kNameBytes);

	TryWriteDword(reinterpret_cast<void*>(record + kThumbnail),
		static_cast<uint32_t>(static_cast<int32_t>(thumbnail)));

	LOG("BgRecord: stage %d points at %s now, %d of %d field(s) carried", slot, folder, carried,
		static_cast<int>(sizeof(kFields) / sizeof(kFields[0])));

	return true;
}
