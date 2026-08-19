#include "Palette/PaletteDrawProbe.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTracker.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"
#include "Hooks/HookManager.h"
#include "Palette/PaletteTexture.h"

#include "MinHook.h"

#include <atomic>
#include <cmath>
#include <cstring>

namespace {

using SetFloatParam_t = void(__fastcall*)(void*, void*, const char*, const float*, int);
SetFloatParam_t oSetFloatParam = nullptr;

std::atomic<bool> g_capturing{ false };
bool g_installed = false;

constexpr int kMaxNames = 128;
constexpr int kMaxNameValues = 4;

struct NameSeen
{
	char name[64];
	int calls;
	int count;
	float values[kMaxNameValues];
};

std::atomic<bool> g_captureNames{ false };
NameSeen g_names[kMaxNames] = {};
int g_nameCount = 0;

std::atomic<bool> g_forcing{ false };
char g_forcedName[64] = "g_fPtLastColor";
float g_forcedRgb[3] = { 1.0f, 0.0f, 0.0f };
std::atomic<uintptr_t> g_forcedTexture{ 0 };
std::atomic<int> g_forcedIndex{ -1 };

constexpr int kMaxStages = 16;
void* g_stageTexture[kMaxStages] = {};
bool g_stagePalette[kMaxStages] = {};

constexpr int kEffectPaletteBytes = 256 * 4;

constexpr int kMaxTints = 96;

std::atomic<bool> g_captureTints{ false };
PaletteDrawProbe::Tint g_tints[kMaxTints] = {};
int g_tintCount = 0;
uintptr_t g_pendingCaller = 0;

bool SameColour(const float* a, const float* b)
{
	for (int i = 0; i < 3; ++i)
	{
		const float d = a[i] - b[i];
		if (d > 0.004f || d < -0.004f)
			return false;
	}

	return true;
}

constexpr uintptr_t kObjectScanBytes = 0x200;

uint8_t g_livePalette[2][kEffectPaletteBytes] = {};
std::atomic<bool> g_hasLivePalette[2] = {};

int g_frameSerial = 0;

int OwnerFromColour(int entry, const float* rgb)
{
	if (entry <= 0 || entry >= 256)
		return -1;

	int wanted[3] = {};
	for (int i = 0; i < 3; ++i)
		wanted[i] = static_cast<int>(rgb[i] * 255.0f + 0.5f);

	int found = -1;

	for (int player = 0; player < 2; ++player)
	{
		if (!g_hasLivePalette[player].load(std::memory_order_relaxed))
			continue;

		const uint8_t* const at = &g_livePalette[player][entry * 4];

		bool same = true;
		for (int i = 0; i < 3 && same; ++i)
		{
			const int delta = static_cast<int>(at[i]) - wanted[i];
			same = delta <= 1 && delta >= -1;
		}

		if (!same)
			continue;

		if (found >= 0)
			return -1;

		found = player;
	}

	if (found >= 0)
		return found;

	const bool readable[2] = {
		g_hasLivePalette[0].load(std::memory_order_relaxed),
		g_hasLivePalette[1].load(std::memory_order_relaxed),
	};

	if (readable[0] != readable[1])
		return readable[0] ? 1 : 0;

	return -1;
}

constexpr int kLiveRefreshFrames = 15;

int g_liveTexture[2] = { -1, -1 };
int g_liveRow[2] = { -1, -1 };
int g_liveGeneration[2] = { -1, -1 };
int g_liveRefreshIn[2] = {};

bool LiveReadIsDue(int player, int texture, int row, bool pristine)
{
	if (pristine || !g_hasLivePalette[player])
		return true;

	const int generation = PaletteTexture::GetGeneration();

	if (texture != g_liveTexture[player] || row != g_liveRow[player] ||
		generation != g_liveGeneration[player])
	{
		return true;
	}

	if (--g_liveRefreshIn[player] > 0)
		return false;

	g_liveRefreshIn[player] = kLiveRefreshFrames;
	return true;
}

void RefreshLivePalettes()
{
	for (int player = 0; player < 2; ++player)
	{
		const int texture = PaletteTexture::FindForPlayer(player);
		const int row = PaletteTexture::GetRowForPlayer(player);

		if (texture < 0 || row < 0)
		{
			g_hasLivePalette[player] = false;
			continue;
		}

		const bool pristine = PaletteTexture::HasPristineRow(texture, static_cast<unsigned>(row));

		if (!LiveReadIsDue(player, texture, row, pristine))
			continue;

		g_liveTexture[player] = texture;
		g_liveRow[player] = row;
		g_liveGeneration[player] = PaletteTexture::GetGeneration();

		uint8_t palette[kEffectPaletteBytes] = {};

		if (!PaletteTexture::ReadPristineRowAsRgba(texture, static_cast<unsigned>(row), palette) &&
			!PaletteTexture::ReadRowAsRgba(texture, static_cast<unsigned>(row), palette))
		{
			g_hasLivePalette[player] = false;
			continue;
		}

		memcpy(g_livePalette[player], palette, sizeof(palette));
		g_hasLivePalette[player] = true;
	}
}

std::atomic<int> g_objectInEffectPool{ 0 };
std::atomic<int> g_objectInCharaPool{ 0 };
std::atomic<int> g_objectElsewhere{ 0 };
std::atomic<uintptr_t> g_objectSample{ 0 };
std::atomic<bool> g_objectScanned{ false };

constexpr uintptr_t kTintCallerRva = 0x1535d;
constexpr uintptr_t kTintValuesToEbp = 0x5c;
constexpr uintptr_t kTintObjectSlot = 0x50;
constexpr uintptr_t kTintIndexOffset = 0x40;

struct DrawnEntry
{
	uint8_t drawn[3];
	uint8_t first[3];
	bool seen;
};

DrawnEntry g_drawn[2][256] = {};

uint8_t ToByte(float value)
{
	const float scaled = value * 255.0f;

	if (scaled <= 0.0f)
		return 0;
	if (scaled >= 255.0f)
		return 255;

	return static_cast<uint8_t>(scaled + 0.5f);
}

void NoteDrawn(int player, int entry, const float* values)
{
	if (player < 0 || player > 1 || entry <= 0 || entry >= 256)
		return;

	DrawnEntry& seen = g_drawn[player][entry];

	const uint8_t passed[3] = { ToByte(values[0]), ToByte(values[1]), ToByte(values[2]) };

	if (!seen.seen)
	{
		memcpy(seen.first, passed, sizeof(passed));
		seen.seen = true;
	}

	memcpy(seen.drawn, passed, sizeof(passed));
}

uint8_t g_entryColour[2][256][3] = {};
bool g_entryEdited[2][256] = {};
std::atomic<int> g_entryEditedCount[2] = {};

int g_entryChara[2] = { -1, -1 };
int g_entryPalette[2] = { -1, -1 };

std::atomic<bool> g_mirror{ false };

int EditedOwner(int player, int entry)
{
	if (entry <= 0 || entry >= 256)
		return -1;

	if (player >= 0 && player <= 1)
		return g_entryEdited[player][entry] ? player : -1;

	if (g_mirror.load(std::memory_order_relaxed))
		return -1;

	if (g_entryEdited[0][entry] && !g_entryEdited[1][entry])
		return 0;

	if (g_entryEdited[1][entry] && !g_entryEdited[0][entry])
		return 1;

	return -1;
}

int ReadTintSource(const float* values, uintptr_t& outObject)
{
	outObject = 0;

	if (g_pendingCaller != kTintCallerRva)
		return -1;

	const uintptr_t ebp = reinterpret_cast<uintptr_t>(values) + kTintValuesToEbp;

	uint32_t object = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(ebp - kTintObjectSlot), object) || object == 0)
		return -1;

	uint8_t index = 0;
	if (!TryReadMemory(&index, reinterpret_cast<const void*>(object + kTintIndexOffset),
		sizeof(index)))
	{
		return -1;
	}

	outObject = object;
	return index;
}

int ReadTintSource(const float* values)
{
	uintptr_t ignored = 0;
	return ReadTintSource(values, ignored);
}

void NoteObjectPool(uintptr_t object)
{
	void* const at = reinterpret_cast<void*>(object);

	if (MemoryMap::GetEffectSlotIndex(at) >= 0)
	{
		++g_objectInEffectPool;
		return;
	}

	if (MemoryMap::GetCharaSlotIndex(at) >= 0)
	{
		++g_objectInCharaPool;
		return;
	}

	++g_objectElsewhere;

	if (g_objectSample.load(std::memory_order_relaxed) == 0)
		g_objectSample = object;
}

void ScanObjectOnce()
{
	if (g_objectScanned.load(std::memory_order_relaxed))
		return;

	const uintptr_t object = g_objectSample.load(std::memory_order_relaxed);
	if (object == 0)
		return;

	g_objectScanned = true;

	LOG("[effect] object 0x%08x is in no pool - scanning it for a link to an entity",
		static_cast<unsigned>(object));

	int found = 0;

	for (uintptr_t offset = 0; offset < kObjectScanBytes; offset += 4)
	{
		uint32_t value = 0;
		if (!TryReadDword(reinterpret_cast<const void*>(object + offset), value) || value < 0x10000)
			continue;

		void* const at = reinterpret_cast<void*>(value);
		const int effect = MemoryMap::GetEffectSlotIndex(at);
		const int chara = MemoryMap::GetCharaSlotIndex(at);

		if (effect >= 0)
		{
			LOG("[effect]    +0x%03x -> effect slot %d", static_cast<unsigned>(offset), effect);
			++found;
		}
		else if (chara >= 0)
		{
			LOG("[effect]    +0x%03x -> chara slot %d", static_cast<unsigned>(offset), chara);
			++found;
		}
	}

	LOG("[effect] scan done, %d link(s)", found);
}

void NoteTint(uintptr_t texture, const float* rgb, uintptr_t valuesPointer, uintptr_t callerRva,
	int paletteIndex)
{
	for (int i = 0; i < g_tintCount; ++i)
	{
		if (g_tints[i].texture != texture || !SameColour(g_tints[i].rgb, rgb) ||
			g_tints[i].callerRva != callerRva || g_tints[i].paletteIndex != paletteIndex)
		{
			continue;
		}

		++g_tints[i].draws;
		g_tints[i].valuesPointer = valuesPointer;
		return;
	}

	if (g_tintCount >= kMaxTints)
		return;

	PaletteDrawProbe::Tint& tint = g_tints[g_tintCount++];
	tint.texture = texture;
	tint.rgb[0] = rgb[0];
	tint.rgb[1] = rgb[1];
	tint.rgb[2] = rgb[2];
	tint.draws = 1;
	tint.valuesPointer = valuesPointer;
	tint.callerRva = callerRva;
	tint.paletteIndex = paletteIndex;
	tint.stackCount = 0;

	void* frames[16] = {};
	const USHORT captured = RtlCaptureStackBackTrace(0, 16, frames, nullptr);
	const uintptr_t base = GetGameBaseAddress();

	for (USHORT f = 0; f < captured && tint.stackCount < 8; ++f)
	{
		const uintptr_t address = reinterpret_cast<uintptr_t>(frames[f]);
		if (base == 0 || address <= base || address - base > 0x4000000)
			continue;

		tint.stack[tint.stackCount++] = address - base;
	}
}

constexpr int kMaxVoted = 16;

struct SeatVote
{
	uintptr_t texture;
	unsigned votes[2];
};

SeatVote g_seatVotes[kMaxVoted] = {};
int g_seatVoteCount = 0;

int g_currentRow = -1;

void NoteSeat(void* texture, int seat)
{
	const uintptr_t value = reinterpret_cast<uintptr_t>(texture);

	for (int i = 0; i < g_seatVoteCount; ++i)
	{
		if (g_seatVotes[i].texture != value)
			continue;

		++g_seatVotes[i].votes[seat];
		return;
	}

	if (g_seatVoteCount >= kMaxVoted)
		return;

	SeatVote& vote = g_seatVotes[g_seatVoteCount++];
	vote.texture = value;
	vote.votes[0] = 0;
	vote.votes[1] = 0;
	vote.votes[seat] = 1;
}

float g_pendingRow = -1.0f;
bool g_hasPendingRow = false;
bool g_pendingMainPath = false;
uintptr_t g_pendingChara = 0;

int g_charaHits = 0;
int g_rowSets = 0;

int g_callsThisFrame = 0;
int g_drawsThisFrame = 0;
int g_tuplesThisFrame = 0;
int g_callsLastFrame = 0;
int g_drawsLastFrame = 0;
int g_tuplesLastFrame = 0;

constexpr int kMaxTuples = 48;
PaletteDrawProbe::Tuple g_tuples[kMaxTuples] = {};
int g_lastSeenFrame[kMaxTuples] = {};
int g_tupleCount = 0;

void NoteTuple(void* texture, int stage, int row, bool mainPath, uintptr_t chara)
{
	++g_tuplesThisFrame;

	for (int i = 0; i < g_tupleCount; ++i)
	{
		PaletteDrawProbe::Tuple& tuple = g_tuples[i];
		if (tuple.texture == reinterpret_cast<uintptr_t>(texture) && tuple.stage == stage &&
			tuple.row == row && tuple.mainPath == mainPath)
		{
			++tuple.count;

			if (g_lastSeenFrame[i] != g_frameSerial)
				++tuple.frames;

			if (tuple.chara == 0)
				tuple.chara = chara;

			g_lastSeenFrame[i] = g_frameSerial;
			return;
		}
	}

	int slot = g_tupleCount;

	if (slot >= kMaxTuples)
	{

		slot = 0;
		for (int i = 1; i < kMaxTuples; ++i)
		{
			if (g_lastSeenFrame[i] < g_lastSeenFrame[slot])
				slot = i;
		}
	}
	else
	{
		++g_tupleCount;
	}

	PaletteDrawProbe::Tuple& tuple = g_tuples[slot];
	tuple.texture = reinterpret_cast<uintptr_t>(texture);
	tuple.stage = stage;
	tuple.row = row;
	tuple.mainPath = mainPath;
	tuple.count = 1;
	tuple.frames = 1;
	tuple.chara = chara;
	g_lastSeenFrame[slot] = g_frameSerial;

	LOG("drawprobe: texture 0x%p on stage %d draws row %d (%s path, chara 0x%08x)", texture, stage,
		row, mainPath ? "main" : "ex", static_cast<unsigned>(chara));
}

void NoteName(const char* name, const float* values, int count)
{
	for (int i = 0; i < g_nameCount; ++i)
	{
		if (strcmp(g_names[i].name, name) != 0)
			continue;

		++g_names[i].calls;
		g_names[i].count = count;

		for (int v = 0; v < count && v < kMaxNameValues; ++v)
			g_names[i].values[v] = values != nullptr ? values[v] : 0.0f;

		return;
	}

	if (g_nameCount >= kMaxNames)
		return;

	NameSeen& seen = g_names[g_nameCount++];
	strncpy_s(seen.name, name, _TRUNCATE);
	seen.calls = 1;
	seen.count = count;

	for (int v = 0; v < count && v < kMaxNameValues; ++v)
		seen.values[v] = values != nullptr ? values[v] : 0.0f;
}

bool IsLastColorName(const char* name)
{
	return name[0] == 'g' && name[1] == '_' && name[2] == 'f' && name[3] == 'P' &&
		strcmp(name, "g_fPtLastColor") == 0;
}

void NoteParam(const char* name, const float* values, int count)
{
	++g_callsThisFrame;

	if (name == nullptr)
		return;

	if (g_captureNames)
		NoteName(name, values, count);

	if (g_captureTints.load(std::memory_order_relaxed) && values != nullptr && count >= 3 &&
		IsLastColorName(name))
	{
		NoteTint(reinterpret_cast<uintptr_t>(g_stageTexture[0]), values,
			reinterpret_cast<uintptr_t>(values), g_pendingCaller, ReadTintSource(values));
	}

	if (name[0] == 'g')
	{
		if (name[1] == '_' && name[2] == 'f' && name[3] == 'Z' &&
			strcmp(name, "g_fZeroPosV") == 0)
		{
			g_pendingMainPath = true;
		}

		return;
	}

	if (name[0] != 'f' || name[1] != 'P')
		return;

	if (values == nullptr || count < 1 || strcmp(name, "fPalPlusV") != 0)
		return;

	g_pendingRow = values[0];
	g_hasPendingRow = true;
	g_currentRow = static_cast<int>(std::lround(values[0] * 64.0f));
	++g_rowSets;

	void* object = nullptr;
	void* chara = nullptr;
	g_pendingChara = CharaTracker::ReadCurrentCharacter(object, chara)
		? reinterpret_cast<uintptr_t>(chara) : 0;

	if (g_pendingChara != 0)
		++g_charaHits;
}

void __fastcall HookedSetFloatParam(void* self, void* edx, const char* name, const float* values,
	int count)
{

	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
	const uintptr_t base = GetGameBaseAddress();
	g_pendingCaller = base != 0 && caller > base ? caller - base : 0;

	if (g_capturing.load(std::memory_order_relaxed))
		NoteParam(name, values, count);

	if (g_forcing.load(std::memory_order_relaxed) && name != nullptr && values != nullptr &&
		count >= 3 && strcmp(name, g_forcedName) == 0)
	{
		const uintptr_t only = g_forcedTexture.load(std::memory_order_relaxed);
		const uintptr_t bound = reinterpret_cast<uintptr_t>(g_stageTexture[0]);
		const int wantIndex = g_forcedIndex.load(std::memory_order_relaxed);
		const bool indexOk = wantIndex < 0 || wantIndex == ReadTintSource(values);

		if ((only == 0 || only == bound) && indexOk)
		{

			float forced[4] = { g_forcedRgb[0], g_forcedRgb[1], g_forcedRgb[2],
				count > 3 ? values[3] : 0.0f };

			oSetFloatParam(self, edx, name, forced, count);
			return;
		}
	}

	if (name != nullptr && values != nullptr && count >= 3 && g_pendingCaller == kTintCallerRva &&
		IsLastColorName(name))
	{
		uintptr_t object = 0;
		const int index = ReadTintSource(values, object);

		if (object != 0)
			NoteObjectPool(object);

		if (index > 0 && index < 256)
		{
			const int player = OwnerFromColour(index, values);

			NoteDrawn(player, index, values);

			const int owner = EditedOwner(player, index);

			if (owner >= 0)
			{
				const uint8_t* const rgb = g_entryColour[owner][index];

				const float edited[4] = { rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f,
					count > 3 ? values[3] : 0.0f };

				oSetFloatParam(self, edx, name, edited, count);
				return;
			}
		}
	}

	oSetFloatParam(self, edx, name, values, count);
}

}

bool PaletteDrawProbe::Install()
{
	if (g_installed)
		return true;

	void* target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnSetEffectFloatParam));

	if (!HookManager::CreateAndEnableHook(target, &HookedSetFloatParam,
		reinterpret_cast<void**>(&oSetFloatParam), "SetEffectFloatParam"))
	{
		return false;
	}

	g_installed = true;
	LOG("PaletteDrawProbe installed on the float setter at 0x%p", target);
	return true;
}

void PaletteDrawProbe::ResetEffectEntries()
{
	memset(g_drawn, 0, sizeof(g_drawn));
}

bool PaletteDrawProbe::GetEffectEntryColour(int player, int entry, unsigned char* outRgb,
	unsigned char* outFirst)
{
	if (player < 0 || player > 1 || entry <= 0 || entry >= 256 || outRgb == nullptr)
		return false;

	const DrawnEntry& seen = g_drawn[player][entry];
	if (!seen.seen)
		return false;

	memcpy(outRgb, seen.drawn, 3);

	if (outFirst != nullptr)
		memcpy(outFirst, seen.first, 3);

	return true;
}

void PaletteDrawProbe::SetEffectEntryColour(int player, int entry, const unsigned char* rgb)
{
	if (player < 0 || player > 1 || entry <= 0 || entry >= 256)
		return;

	if (rgb == nullptr)
	{
		if (!g_entryEdited[player][entry])
			return;

		g_entryEdited[player][entry] = false;
		--g_entryEditedCount[player];
		return;
	}

	if (!g_entryEdited[player][entry])
	{
		g_entryEdited[player][entry] = true;
		++g_entryEditedCount[player];
	}

	memcpy(g_entryColour[player][entry], rgb, 3);
}

bool PaletteDrawProbe::IsEffectEntryEdited(int player, int entry)
{
	if (player < 0 || player > 1 || entry <= 0 || entry >= 256)
		return false;

	return g_entryEdited[player][entry];
}

bool PaletteDrawProbe::GetEffectEntryEdit(int player, int entry, unsigned char* outRgb)
{
	if (!IsEffectEntryEdited(player, entry) || outRgb == nullptr)
		return false;

	memcpy(outRgb, g_entryColour[player][entry], 3);
	return true;
}

void PaletteDrawProbe::ClearEffectEntryColours(int player)
{
	if (player < 0 || player > 1)
		return;

	for (int entry = 1; entry < 256; ++entry)
		SetEffectEntryColour(player, entry, nullptr);
}

void PaletteDrawProbe::SyncEffectSides(int chara0, int palette0, int chara1, int palette1)
{
	const int chara[2] = { chara0, chara1 };
	const int palette[2] = { palette0, palette1 };

	g_mirror = chara0 >= 0 && chara0 == chara1;

	const bool swapped =
		chara[0] == g_entryChara[1] && palette[0] == g_entryPalette[1] &&
		chara[1] == g_entryChara[0] && palette[1] == g_entryPalette[0] &&
		g_entryChara[0] != g_entryChara[1];

	if (swapped)
	{
		for (int entry = 1; entry < 256; ++entry)
		{
			std::swap(g_entryEdited[0][entry], g_entryEdited[1][entry]);

			uint8_t keep[3] = {};
			memcpy(keep, g_entryColour[0][entry], sizeof(keep));
			memcpy(g_entryColour[0][entry], g_entryColour[1][entry], sizeof(keep));
			memcpy(g_entryColour[1][entry], keep, sizeof(keep));
		}

		const int count = g_entryEditedCount[0].load(std::memory_order_relaxed);
		g_entryEditedCount[0] = g_entryEditedCount[1].load(std::memory_order_relaxed);
		g_entryEditedCount[1] = count;

		std::swap(g_entryChara[0], g_entryChara[1]);
		std::swap(g_entryPalette[0], g_entryPalette[1]);

		LOG("[effect] sides swapped, the edits went with their characters");
		return;
	}

	for (int player = 0; player < 2; ++player)
	{
		if (chara[player] == g_entryChara[player] && palette[player] == g_entryPalette[player])
			continue;

		if (g_entryEditedCount[player].load(std::memory_order_relaxed) > 0)
			ClearEffectEntryColours(player);

		g_entryChara[player] = chara[player];
		g_entryPalette[player] = palette[player];
	}
}

void PaletteDrawProbe::ResetMatchState()
{
	for (int player = 0; player < 2; ++player)
		ClearEffectEntryColours(player);

	g_entryChara[0] = -1;
	g_entryChara[1] = -1;
	g_entryPalette[0] = -1;
	g_entryPalette[1] = -1;
	g_mirror = false;

	ResetEffectEntries();
}

int PaletteDrawProbe::GetEffectEntryEditedCount(int player)
{
	if (player < 0 || player > 1)
		return 0;

	return g_entryEditedCount[player].load(std::memory_order_relaxed);
}

void PaletteDrawProbe::GetObjectPoolCounts(int& outEffect, int& outChara, int& outElsewhere)
{
	outEffect = g_objectInEffectPool.load(std::memory_order_relaxed);
	outChara = g_objectInCharaPool.load(std::memory_order_relaxed);
	outElsewhere = g_objectElsewhere.load(std::memory_order_relaxed);
}

bool PaletteDrawProbe::IsInstalled()
{
	return g_installed;
}

void PaletteDrawProbe::SetCapturing(bool on)
{
	if (on == g_capturing.load(std::memory_order_relaxed))
		return;

	if (on)
	{
		memset(g_stageTexture, 0, sizeof(g_stageTexture));
		memset(g_stagePalette, 0, sizeof(g_stagePalette));
		g_hasPendingRow = false;
		g_pendingMainPath = false;
	}

	g_capturing.store(on, std::memory_order_relaxed);
	LOG("drawprobe: %s", on ? "capturing" : "off");
}

void PaletteDrawProbe::SetNameCapture(bool enabled)
{
	if (enabled)
		g_nameCount = 0;

	g_captureNames = enabled;
	LOG("[params] name capture %s", enabled ? "on" : "off");
}

bool PaletteDrawProbe::IsNameCapturing()
{
	return g_captureNames;
}

void PaletteDrawProbe::DumpNames()
{
	LOG("[params] %d distinct shader parameter name(s)", g_nameCount);

	for (int i = 0; i < g_nameCount; ++i)
	{
		const NameSeen& seen = g_names[i];

		LOG("[params] %-28s floats %d calls %-7d last %.3f %.3f %.3f %.3f", seen.name, seen.count,
			seen.calls, seen.values[0], seen.values[1], seen.values[2], seen.values[3]);
	}

	LOG("[params] end");
}

void PaletteDrawProbe::SetTintCapture(bool enabled)
{
	if (enabled)
		g_tintCount = 0;

	g_captureTints = enabled;
	LOG("[tint] capture %s", enabled ? "on" : "off");
}

bool PaletteDrawProbe::IsTintCapturing()
{
	return g_captureTints;
}

int PaletteDrawProbe::GetTintCount()
{
	return g_tintCount;
}

bool PaletteDrawProbe::GetTint(int index, Tint& out)
{
	if (index < 0 || index >= g_tintCount)
		return false;

	out = g_tints[index];
	return true;
}

void PaletteDrawProbe::DumpTints()
{
	LOG("[tint] %d distinct sprite-and-colour pair(s)", g_tintCount);

	for (int i = 0; i < g_tintCount; ++i)
	{
		const Tint& tint = g_tints[i];

		LOG("[tint] %2d sprite 0x%08x rgb %.3f %.3f %.3f draws %-6d from 0x%08x set by rva 0x%06x",
			i, static_cast<unsigned>(tint.texture), tint.rgb[0], tint.rgb[1], tint.rgb[2],
			tint.draws, static_cast<unsigned>(tint.valuesPointer),
			static_cast<unsigned>(tint.callerRva));

		LOG("[tint]        palette entry %d", tint.paletteIndex);

		for (int s = 0; s < tint.stackCount; ++s)
			LOG("[tint]        stack[%d] rva 0x%06x", s, static_cast<unsigned>(tint.stack[s]));
	}

	LOG("[tint] distinct colours:");

	for (int i = 0; i < g_tintCount; ++i)
	{
		bool earlier = false;
		for (int j = 0; j < i && !earlier; ++j)
			earlier = SameColour(g_tints[j].rgb, g_tints[i].rgb);

		if (earlier)
			continue;

		int draws = 0;
		for (int j = 0; j < g_tintCount; ++j)
		{
			if (SameColour(g_tints[j].rgb, g_tints[i].rgb))
				draws += g_tints[j].draws;
		}

		LOG("[tint]     rgb %.4f %.4f %.4f  (bytes %d %d %d)  draws %d", g_tints[i].rgb[0],
			g_tints[i].rgb[1], g_tints[i].rgb[2],
			static_cast<int>(g_tints[i].rgb[0] * 255.0f + 0.5f),
			static_cast<int>(g_tints[i].rgb[1] * 255.0f + 0.5f),
			static_cast<int>(g_tints[i].rgb[2] * 255.0f + 0.5f), draws);
	}

	LOG("[tint] end");
}

void PaletteDrawProbe::SetForcedColor(bool enabled, const char* name, const float* rgb,
	uintptr_t onlyTexture, int onlyIndex)
{
	g_forcedTexture = onlyTexture;
	g_forcedIndex = onlyIndex;

	if (name != nullptr && name[0] != '\0')
		strncpy_s(g_forcedName, name, _TRUNCATE);

	if (rgb != nullptr)
	{
		g_forcedRgb[0] = rgb[0];
		g_forcedRgb[1] = rgb[1];
		g_forcedRgb[2] = rgb[2];
	}

	g_forcing = enabled;

	LOG("[params] force %s on '%s' = %.3f %.3f %.3f, sprite 0x%08x, entry %d", enabled ? "on" : "off",
		g_forcedName, g_forcedRgb[0], g_forcedRgb[1], g_forcedRgb[2],
		static_cast<unsigned>(onlyTexture), onlyIndex);
}

bool PaletteDrawProbe::IsForcingColor()
{
	return g_forcing;
}

const char* PaletteDrawProbe::GetForcedName()
{
	return g_forcedName;
}

bool PaletteDrawProbe::IsCapturing()
{
	return g_capturing.load(std::memory_order_relaxed);
}

void PaletteDrawProbe::OnSetTexture(unsigned stage, void* texture, bool paletteShaped)
{
	if (stage >= kMaxStages)
		return;

	g_stageTexture[stage] = texture;
	g_stagePalette[stage] = texture != nullptr && paletteShaped;
}

void PaletteDrawProbe::OnDraw()
{
	if (!g_capturing.load(std::memory_order_relaxed))
		return;

	++g_drawsThisFrame;

	if (g_currentRow == 0 || g_currentRow == 1)
	{
		const int seat = PaletteTexture::GetRowsSwapped() ? 1 - g_currentRow : g_currentRow;

		for (int stage = 0; stage < kMaxStages; ++stage)
		{
			if (g_stagePalette[stage])
				NoteSeat(g_stageTexture[stage], seat);
		}
	}

	if (!g_hasPendingRow)
	{

		g_pendingMainPath = false;
		return;
	}

	const int row = static_cast<int>(std::lround(g_pendingRow * 64.0f));

	for (int stage = 0; stage < kMaxStages; ++stage)
	{
		if (g_stagePalette[stage])
			NoteTuple(g_stageTexture[stage], stage, row, g_pendingMainPath, g_pendingChara);
	}

	g_hasPendingRow = false;
	g_pendingMainPath = false;
	g_pendingChara = 0;
}

void PaletteDrawProbe::OnFrame()
{
	++g_frameSerial;

	RefreshLivePalettes();
	ScanObjectOnce();

	g_callsLastFrame = g_callsThisFrame;
	g_drawsLastFrame = g_drawsThisFrame;
	g_tuplesLastFrame = g_tuplesThisFrame;
	g_callsThisFrame = 0;
	g_drawsThisFrame = 0;
	g_tuplesThisFrame = 0;
}

int PaletteDrawProbe::GetTupleCount()
{
	return g_tupleCount;
}

bool PaletteDrawProbe::GetTuple(int index, Tuple& out)
{
	if (index < 0 || index >= g_tupleCount)
		return false;

	out = g_tuples[index];
	out.lastAgeFrames = g_frameSerial - g_lastSeenFrame[index];
	return true;
}

int PaletteDrawProbe::GetCallsLastFrame()
{
	return g_callsLastFrame;
}

int PaletteDrawProbe::GetDrawsLastFrame()
{
	return g_drawsLastFrame;
}

int PaletteDrawProbe::GetTuplesLastFrame()
{
	return g_tuplesLastFrame;
}

int PaletteDrawProbe::GetCharaHits()
{
	return g_charaHits;
}

int PaletteDrawProbe::GetRowSets()
{
	return g_rowSets;
}

void PaletteDrawProbe::ResetTuples()
{
	g_tupleCount = 0;
	memset(g_tuples, 0, sizeof(g_tuples));
	memset(g_lastSeenFrame, 0, sizeof(g_lastSeenFrame));

	g_seatVoteCount = 0;
	g_currentRow = -1;
	memset(g_seatVotes, 0, sizeof(g_seatVotes));
}

bool PaletteDrawProbe::GetSeatVotes(uintptr_t texture, unsigned& outFirst, unsigned& outSecond)
{
	outFirst = 0;
	outSecond = 0;

	for (int i = 0; i < g_seatVoteCount; ++i)
	{
		if (g_seatVotes[i].texture != texture)
			continue;

		outFirst = g_seatVotes[i].votes[0];
		outSecond = g_seatVotes[i].votes[1];
		return true;
	}

	return false;
}

void PaletteDrawProbe::Dump()
{
	LOG_SECTION("draw probe");

	LOG_RAW("capturing %d, %d setter calls / %d draws / %d tuples last frame",
		g_capturing.load(std::memory_order_relaxed) ? 1 : 0, g_callsLastFrame, g_drawsLastFrame,
		g_tuplesLastFrame);

	LOG_RAW("the chara stack was live for %d of %d palette rows set", g_charaHits, g_rowSets);

	for (int i = 0; i < g_tupleCount; ++i)
	{
		LOG_RAW("tuple %d: texture 0x%08x stage %d row %d %s  %d draws in %d frames, chara 0x%08x, "
			"last %df ago", i, static_cast<unsigned>(g_tuples[i].texture), g_tuples[i].stage,
			g_tuples[i].row, g_tuples[i].mainPath ? "main" : "ex", g_tuples[i].count,
			g_tuples[i].frames, static_cast<unsigned>(g_tuples[i].chara),
			g_frameSerial - g_lastSeenFrame[i]);
	}
}
