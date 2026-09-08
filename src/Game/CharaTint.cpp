#include "Game/CharaTint.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/ExtraStages.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"
#include "Game/StageLibrary.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kPlayers = 2;
constexpr uint16_t kHold = 9999;
constexpr const char* kKey = "CharaTint";

int g_stage = -2;
uint32_t g_colour = 0;
bool g_wanted = false;

char g_status[160] = "no stage asks for one";

bool ReadTint(int stage, uint32_t& out)
{
	const int id = StageLibrary::IdForSlot(stage);
	std::vector<uint8_t> blob;

	if (id < 0 || !ReadWholeFile(StageLibrary::NoteOf(id), blob) || blob.empty())
		return false;

	const std::string text(reinterpret_cast<const char*>(blob.data()), blob.size());
	const size_t at = text.find(kKey);

	if (at == std::string::npos)
		return false;

	const size_t equals = text.find('=', at);

	if (equals == std::string::npos)
		return false;

	return sscanf_s(text.c_str() + equals + 1, " 0x%x", &out) == 1;
}

void Follow(int stage)
{
	g_stage = stage;
	g_wanted = stage >= 0 && ReadTint(stage, g_colour);

	if (!g_wanted)
	{
		strncpy_s(g_status, "no stage asks for one", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "stage %d asks for characters at 0x%06X", stage, g_colour & 0xffffff);
	LOG("CharaTint: %s", g_status);
}

void Paint(void* playerData)
{
	if (playerData == nullptr)
		return;

	uint8_t* const base = static_cast<uint8_t*>(playerData);
	const uint16_t hold = kHold;
	const uint8_t type = GameOffsets::kCharaTintHold;

	TryWriteMemory(base + GameOffsets::kPlayerDataTintColour, &g_colour, sizeof(g_colour));
	TryWriteMemory(base + GameOffsets::kPlayerDataTintTime, &hold, sizeof(hold));
	TryWriteMemory(base + GameOffsets::kPlayerDataTintHold, &hold, sizeof(hold));
	TryWriteMemory(base + GameOffsets::kPlayerDataTintIn, &hold, sizeof(hold));
	TryWriteMemory(base + GameOffsets::kPlayerDataTintType, &type, sizeof(type));
}

}

void CharaTint::Update()
{
	const int stage = ExtraStages::LoadedStage();

	if (stage != g_stage)
		Follow(stage);

	if (!g_wanted || OnlineState::IsOnline() || !GameState::IsInMatch())
		return;

	for (int player = 0; player < kPlayers; ++player)
		Paint(MemoryMap::GetCharaSlot(player));
}

bool CharaTint::Applies()
{
	return g_wanted;
}

const char* CharaTint::StatusText()
{
	return g_status;
}
