#include "Palette/EffectPaint.h"

#include "Core/utils.h"
#include "Hooks/HookManager.h"
#include "Core/interfaces.h"
#include "Palette/EffectOwner.h"
#include "Palette/PaletteControl.h"

#include <intrin.h>

#include <cstring>

namespace {

constexpr uintptr_t kSetParamRva = 0x156d0;
constexpr uintptr_t kTintCallerRva = 0x1535d;

constexpr uintptr_t kValuesToFrame = 0x5c;
constexpr uintptr_t kPieceSlot = 0x50;

constexpr uintptr_t kIndexOffset = 0x40;

typedef void(__fastcall* SetParam)(void* self, void* edx, const char* name, const float* values,
	int count);

SetParam g_original = nullptr;
bool g_installed = false;

int g_tintCalls = 0;
int g_badIndex = 0;
int g_unowned = 0;
int g_passedThrough = 0;
int g_suppressedByWear = 0;

bool g_forcedOn = false;
uint8_t g_forcedRgb[3] = { 255, 0, 255 };
int g_forcedEntry = -1;
int g_forcedCount = 0;

struct AnySeen
{
	uint8_t rgb[3];
	bool seen;
};

AnySeen g_anySeen[EffectPaint::kColours] = {};

constexpr int kMaxSeenCalls = 24;

EffectPaint::Call g_seen[kMaxSeenCalls] = {};
int g_seenCount = 0;

void NoteCall(int entry, uint8_t r, uint8_t g, uint8_t b, int route, int answer, bool substituted)
{
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_seen[i].entry != entry)
			continue;

		++g_seen[i].calls;
		g_seen[i].rgb[0] = r;
		g_seen[i].rgb[1] = g;
		g_seen[i].rgb[2] = b;
		g_seen[i].route = route;
		g_seen[i].answer = answer;
		g_seen[i].substituted += substituted ? 1 : 0;
		return;
	}

	if (g_seenCount >= kMaxSeenCalls)
		return;

	EffectPaint::Call& row = g_seen[g_seenCount++];

	row.entry = entry;
	row.rgb[0] = r;
	row.rgb[1] = g;
	row.rgb[2] = b;
	row.calls = 1;
	row.substituted = substituted ? 1 : 0;
	row.route = route;
	row.answer = answer;
}

uintptr_t g_tintCaller = 0;

struct Entry
{
	uint8_t observed[3];
	bool seen;

	uint8_t edit[3];
	bool edited;

	uint8_t remote[3];
	bool hasRemote;
};

struct Player
{
	Entry entries[EffectPaint::kColours];

	uint8_t previewRgb[3];
	int previewExcept;
	uint8_t previewExceptRgb[3];
	bool previewing;

	int observedCount;
	int substitutions;

	bool wear = true;
	unsigned revision = 0;
};

Player g_players[EffectPaint::kPlayers] = {};

bool IsTintName(const char* name)
{
	return name != nullptr && name[0] == 'g' && name[1] == '_' && name[2] == 'f' && name[3] == 'P'
		&& strcmp(name, "g_fPtLastColor") == 0;
}

int ReadEntry(const float* values)
{
	const uintptr_t frame = reinterpret_cast<uintptr_t>(values) + kValuesToFrame;

	uint32_t piece = 0;
	if (!TryReadMemory(&piece, reinterpret_cast<const void*>(frame - kPieceSlot), sizeof(piece))
		|| piece == 0)
	{
		return -1;
	}

	uint8_t index = 0;
	if (!TryReadMemory(&index, reinterpret_cast<const void*>(piece + kIndexOffset), sizeof(index)))
		return -1;

	return index;
}

uint8_t Byte(float value)
{
	const int scaled = static_cast<int>(value * 255.0f + 0.5f);

	return static_cast<uint8_t>(scaled < 0 ? 0 : (scaled > 255 ? 255 : scaled));
}

void Observe(Player& player, int index, uint8_t r, uint8_t g, uint8_t b)
{
	Entry& entry = player.entries[index];

	if (entry.seen)
		return;

	entry.observed[0] = r;
	entry.observed[1] = g;
	entry.observed[2] = b;
	entry.seen = true;

	++player.observedCount;
}

const uint8_t* WantedIgnoringWear(const Player& player, int index)
{

	if (player.previewing)
		return index == player.previewExcept ? player.previewExceptRgb : player.previewRgb;

	if (player.entries[index].hasRemote)
		return player.entries[index].remote;

	return player.entries[index].edited ? player.entries[index].edit : nullptr;
}

int SoleWanter(int entry)
{
	const bool wants[EffectPaint::kPlayers] = {
		g_players[0].wear && WantedIgnoringWear(g_players[0], entry) != nullptr,
		g_players[1].wear && WantedIgnoringWear(g_players[1], entry) != nullptr,
	};

	if (wants[0] == wants[1])
		return -1;

	const int player = wants[0] ? 0 : 1;

	if (g_players[player].previewing && !EffectOwner::Claims(player, entry))
		return -1;

	if (PaletteControl::LocalPlayer() >= 0 && !g_modVals.showOnlinePalettes
		&& EffectOwner::Claims(1 - player, entry))
	{
		return -1;
	}

	return player;
}

struct Tint
{
	void* self;
	void* edx;
	const char* name;
	const float* values;
	int count;
	float alpha;
};

void PassThrough(const Tint& tint)
{
	g_original(tint.self, tint.edx, tint.name, tint.values, tint.count);
}

void Replace(const Tint& tint, const uint8_t* rgb)
{
	const float swapped[4] = {
		rgb[0] / 255.0f,
		rgb[1] / 255.0f,
		rgb[2] / 255.0f,
		tint.alpha,
	};

	g_original(tint.self, tint.edx, tint.name, swapped, tint.count);
}

bool IsOurCall(const char* name, const float* values, int count, uintptr_t caller)
{
	return IsTintName(name) && values != nullptr && count >= 3 && caller == g_tintCaller;
}

void NoteDrawn(int entry, uint8_t r, uint8_t g, uint8_t b)
{
	AnySeen& seen = g_anySeen[entry];

	if (seen.seen)
		return;

	seen.rgb[0] = r;
	seen.rgb[1] = g;
	seen.rgb[2] = b;
	seen.seen = true;
}

int Owner(int entry, uint8_t r, uint8_t g, uint8_t b, EffectOwner::Route& outRoute)
{
	const int owner = EffectOwner::PlayerFor(entry, r, g, b, outRoute);

	if (owner >= 0)
		return owner;

	const int wanter = SoleWanter(entry);

	if (wanter < 0)
		return -1;

	outRoute = EffectOwner::Route::SoleWanter;
	EffectOwner::NoteSoleWanter();

	return wanter;
}

void __fastcall Detour(void* self, void* edx, const char* name, const float* values, int count)
{
	const Tint tint = { self, edx, name, values, count,
		count > 3 && values != nullptr ? values[3] : 0.0f };

	if (!IsOurCall(name, values, count, reinterpret_cast<uintptr_t>(_ReturnAddress())))
	{
		PassThrough(tint);
		return;
	}

	++g_tintCalls;

	const int entry = ReadEntry(values);

	if (entry <= 0 || entry >= EffectPaint::kColours)
	{
		++g_badIndex;
		PassThrough(tint);
		return;
	}

	const uint8_t r = Byte(values[0]);
	const uint8_t g = Byte(values[1]);
	const uint8_t b = Byte(values[2]);

	NoteDrawn(entry, r, g, b);

	if (g_forcedOn && (g_forcedEntry < 0 || g_forcedEntry == entry))
	{
		++g_forcedCount;
		NoteCall(entry, r, g, b, static_cast<int>(EffectOwner::Route::None), -1, true);
		Replace(tint, g_forcedRgb);
		return;
	}

	EffectOwner::Route route = EffectOwner::Route::None;
	const int player = Owner(entry, r, g, b, route);

	if (player < 0 || player >= EffectPaint::kPlayers)
	{
		++g_unowned;
		NoteCall(entry, r, g, b, static_cast<int>(route), -1, false);
		PassThrough(tint);
		return;
	}

	Observe(g_players[player], entry, r, g, b);

	if (!g_players[player].wear)
	{
		++g_suppressedByWear;
		NoteCall(entry, r, g, b, static_cast<int>(route), player, false);
		PassThrough(tint);
		return;
	}

	const uint8_t* const rgb = WantedIgnoringWear(g_players[player], entry);

	if (rgb == nullptr)
	{
		++g_passedThrough;
		NoteCall(entry, r, g, b, static_cast<int>(route), player, false);
		PassThrough(tint);
		return;
	}

	++g_players[player].substitutions;
	NoteCall(entry, r, g, b, static_cast<int>(route), player, true);

	Replace(tint, rgb);
}

bool Valid(int player, int entry)
{
	return player >= 0 && player < EffectPaint::kPlayers
		&& entry > 0 && entry < EffectPaint::kColours;
}

}

bool EffectPaint::Install()
{
	if (g_installed)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(kSetParamRva));

	if (target == nullptr || !HookManager::CreateAndEnableHook(target, &Detour,
		reinterpret_cast<void**>(&g_original), "effect tint"))
	{
		return false;
	}

	g_tintCaller = RvaToAddress(kTintCallerRva);

	g_installed = true;

	return g_tintCaller != 0;
}

bool EffectPaint::IsInstalled()
{
	return g_installed;
}

bool EffectPaint::GetObserved(int player, int entry, uint8_t* outRgb)
{
	if (!Valid(player, entry) || !g_players[player].entries[entry].seen)
		return false;

	if (outRgb != nullptr)
		memcpy(outRgb, g_players[player].entries[entry].observed, 3);

	return true;
}

int EffectPaint::GetObservedCount(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].observedCount : 0;
}

void EffectPaint::SetEntry(int player, int entry, const uint8_t* rgb)
{
	if (!Valid(player, entry) || rgb == nullptr)
		return;

	memcpy(g_players[player].entries[entry].edit, rgb, 3);
	g_players[player].entries[entry].edited = true;

	++g_players[player].revision;
}

void EffectPaint::ClearEntry(int player, int entry)
{
	if (!Valid(player, entry))
		return;

	g_players[player].entries[entry].edited = false;
	++g_players[player].revision;
}

void EffectPaint::Clear(int player)
{
	if (player < 0 || player >= kPlayers)
		return;

	for (Entry& entry : g_players[player].entries)
		entry.edited = false;

	++g_players[player].revision;
}

void EffectPaint::GetBlock(int player, uint8_t* block)
{
	if (player < 0 || player >= kPlayers || block == nullptr)
		return;

	memset(block, 0, kBlockBytes);

	for (int entry = 1; entry < kColours; ++entry)
	{
		if (!g_players[player].entries[entry].edited)
			continue;

		memcpy(block + entry * 4, g_players[player].entries[entry].edit, 3);
		block[entry * 4 + 3] = 255;
	}
}

void EffectPaint::SetBlock(int player, const uint8_t* block)
{
	if (player < 0 || player >= kPlayers || block == nullptr)
		return;

	for (int entry = 1; entry < kColours; ++entry)
	{
		const bool set = block[entry * 4 + 3] == 255;

		g_players[player].entries[entry].edited = set;

		if (set)
			memcpy(g_players[player].entries[entry].edit, block + entry * 4, 3);
	}

	++g_players[player].revision;
}

void EffectPaint::SetRemote(int player, const uint8_t* block)
{
	if (player < 0 || player >= kPlayers)
		return;

	for (int entry = 1; entry < kColours; ++entry)
	{
		const bool set = block != nullptr && block[entry * 4 + 3] == 255;

		g_players[player].entries[entry].hasRemote = set;

		if (set)
			memcpy(g_players[player].entries[entry].remote, block + entry * 4, 3);
	}
}

void EffectPaint::ClearRemote(int player)
{
	if (player < 0 || player >= kPlayers)
		return;

	for (Entry& entry : g_players[player].entries)
		entry.hasRemote = false;
}

bool EffectPaint::HasRemote(int player)
{
	if (player < 0 || player >= kPlayers)
		return false;

	for (const Entry& entry : g_players[player].entries)
	{
		if (entry.hasRemote)
			return true;
	}

	return false;
}

bool EffectPaint::GetRemoteEntry(int player, int entry, uint8_t* outRgb)
{
	if (!Valid(player, entry) || !g_players[player].entries[entry].hasRemote)
		return false;

	if (outRgb != nullptr)
		memcpy(outRgb, g_players[player].entries[entry].remote, 3);

	return true;
}

void EffectPaint::SetWear(int player, bool allowed)
{
	if (player >= 0 && player < kPlayers)
		g_players[player].wear = allowed;
}

unsigned EffectPaint::GetRevision(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].revision : 0;
}

bool EffectPaint::IsEdited(int player, int entry)
{
	return Valid(player, entry) && g_players[player].entries[entry].edited;
}

bool EffectPaint::GetEdit(int player, int entry, uint8_t* outRgb)
{
	if (!IsEdited(player, entry) || outRgb == nullptr)
		return false;

	memcpy(outRgb, g_players[player].entries[entry].edit, 3);
	return true;
}

int EffectPaint::GetEditedCount(int player)
{
	if (player < 0 || player >= kPlayers)
		return 0;

	int count = 0;

	for (const Entry& entry : g_players[player].entries)
		count += entry.edited ? 1 : 0;

	return count;
}

void EffectPaint::PreviewObserved(int player, const uint8_t* rgb, int except,
	const uint8_t* exceptRgb)
{
	if (player < 0 || player >= kPlayers || rgb == nullptr)
		return;

	Player& entry = g_players[player];

	memcpy(entry.previewRgb, rgb, 3);

	entry.previewExcept = except;

	if (exceptRgb != nullptr)
		memcpy(entry.previewExceptRgb, exceptRgb, 3);
	else
		memcpy(entry.previewExceptRgb, rgb, 3);

	entry.previewing = true;
}

void EffectPaint::EndPreview(int player)
{
	if (player >= 0 && player < kPlayers)
		g_players[player].previewing = false;
}

void EffectPaint::Forget()
{
	for (Player& player : g_players)
	{
		for (Entry& entry : player.entries)
		{
			entry.seen = false;
			memset(entry.observed, 0, sizeof(entry.observed));
		}

		player.observedCount = 0;
		player.previewing = false;
	}

	memset(g_anySeen, 0, sizeof(g_anySeen));
}

int EffectPaint::GetSubstitutions(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].substitutions : 0;
}

bool EffectPaint::Wants(int player, int entry)
{
	if (player < 0 || player >= kPlayers || entry <= 0 || entry >= kColours)
		return false;

	return WantedIgnoringWear(g_players[player], entry) != nullptr;
}

bool EffectPaint::GetAnyObserved(int entry, uint8_t* outRgb)
{
	if (entry <= 0 || entry >= kColours || !g_anySeen[entry].seen)
		return false;

	if (outRgb != nullptr)
		memcpy(outRgb, g_anySeen[entry].rgb, 3);

	return true;
}

void EffectPaint::SetForced(bool on, const uint8_t* rgb, int onlyEntry)
{
	if (rgb != nullptr)
		memcpy(g_forcedRgb, rgb, 3);

	g_forcedEntry = onlyEntry;
	g_forcedOn = on;
}

bool EffectPaint::IsForced()
{
	return g_forcedOn;
}

bool EffectPaint::GetForced(uint8_t* outRgb, int& outEntry)
{
	if (outRgb != nullptr)
		memcpy(outRgb, g_forcedRgb, 3);

	outEntry = g_forcedEntry;
	return g_forcedOn;
}

int EffectPaint::GetForcedCount()
{
	return g_forcedCount;
}

int EffectPaint::GetSeenCallCount()
{
	return g_seenCount;
}

bool EffectPaint::GetSeenCall(int index, Call& out)
{
	if (index < 0 || index >= g_seenCount)
		return false;

	out = g_seen[index];
	return true;
}

int EffectPaint::GetTintCalls()
{
	return g_tintCalls;
}

int EffectPaint::GetBadIndex()
{
	return g_badIndex;
}

int EffectPaint::GetUnowned()
{
	return g_unowned;
}

int EffectPaint::GetPassedThrough()
{
	return g_passedThrough;
}

int EffectPaint::GetSuppressedByWear()
{
	return g_suppressedByWear;
}

void EffectPaint::ResetCounts()
{
	for (Player& player : g_players)
		player.substitutions = 0;

	g_tintCalls = 0;
	g_badIndex = 0;
	g_unowned = 0;
	g_passedThrough = 0;
	g_suppressedByWear = 0;
	g_forcedCount = 0;
	g_seenCount = 0;

	memset(g_seen, 0, sizeof(g_seen));
}
