#include "Palette/PaletteManager.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/Settings.h"
#include "Core/utils.h"
#include "Game/Camera.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Game/PartColourTable.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteIdentity.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTexture.h"
#include "Palette/PaletteTrace.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

struct Entry
{
	PaletteFile::Info info;
	uint8_t colors[PaletteFile::kBytes];
	uint8_t effectColors[PaletteFile::kBytes];
	bool hasEffect;
};

struct Folder
{
	int chara;
	int count;
	Entry entries[PaletteManager::kMaxPalettes];
};

const char* const kCharaNames[] = {
	"Hyde", "Linne", "Waldstein", "Carmine", "Orie", "Gordeau", "Merkava", "Vatista", "Seth",
	"Yuzuriha", "Hilda", "Eltnum", "Nanase", "Byakuya", "Akatsuki", "Chaos", "Wagner", "Enkidu",
	"Londrekia", "Tsurugi", "Uzuki", "Mika", "Kaguya", "Kuon", "Phonon", "Ogre", "Izumi"
};

constexpr int kCharaCount = static_cast<int>(sizeof(kCharaNames) / sizeof(kCharaNames[0]));

constexpr int kMaxFolders = 32;

Folder g_folders[kMaxFolders] = {};
int g_folderCount = 0;

int g_applied[2] = { -1, -1 };

bool g_handEdited[2] = {};

uint8_t g_foreign[2][PaletteFile::kBytes] = {};
bool g_hasForeign[2] = {};
char g_foreignName[2][PaletteFile::kNameLength] = {};

uint8_t g_foreignEffect[2][PaletteFile::kBytes] = {};
bool g_hasForeignEffect[2] = {};

constexpr int kSessionSettleFrames = 30;

constexpr int kEffectRowFreshFrames = 120;

bool WriteSideRows(int player, int texture, const uint8_t* colors)
{
	const int row = PaletteTexture::GetRowForPlayer(player);
	if (row < 0)
		return false;

	if (!PaletteTexture::WriteRowKeepingAlpha(texture, static_cast<unsigned>(row), colors))
		return false;

	const int other = PaletteTexture::FindForPlayer(1 - player);
	if (other < 0 || other == texture)
		return true;

	PaletteTexture::WriteRowKeepingAlpha(texture, static_cast<unsigned>(1 - row), colors);
	return true;
}

bool FreshestEffectDrawRow(int texture, int& outRow)
{
	outRow = -1;

	if (PaletteTexture::GetOwnerInfo(texture).kind != PaletteTexture::OwnerKind::Effect)
		return false;

	const uintptr_t pointer = PaletteTexture::GetSeen(texture);
	int freshest = kEffectRowFreshFrames;

	for (int t = 0; t < PaletteDrawProbe::GetTupleCount(); ++t)
	{
		PaletteDrawProbe::Tuple tuple = {};
		if (!PaletteDrawProbe::GetTuple(t, tuple))
			continue;

		if (tuple.texture != pointer || tuple.lastAgeFrames >= freshest)
			continue;

		freshest = tuple.lastAgeFrames;
		outRow = tuple.row;
	}

	return outRow >= 0;
}

void ApplyEffectColours(int player, const uint8_t* colors)
{
	EffectPaint::SetBlock(player, colors);
}

void RestoreEffectRows()
{
	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		if (PaletteTexture::GetOwnerInfo(i).kind != PaletteTexture::OwnerKind::Effect)
			continue;

		PaletteTexture::Restore(i, 0);
		PaletteTexture::Restore(i, 1);
	}
}

bool g_inMatch = false;
bool g_pending = false;
int g_pendingFrames = 0;

constexpr int kMaxRemembered = 32;
char g_choice[kMaxRemembered][PaletteFile::kNameLength] = {};
bool g_choicesLoaded = false;

void LoadChoices()
{
	if (g_choicesLoaded)
		return;

	g_choicesLoaded = true;

	char text[1024] = {};
	GetPrivateProfileStringA("Palette", "Chosen", "", text, sizeof(text),
		Settings::GetIniPath().c_str());

	const char* at = text;

	while (*at != '\0')
	{
		int chara = 0;
		while (*at >= '0' && *at <= '9')
			chara = chara * 10 + (*at++ - '0');

		if (*at != '=')
			break;

		++at;

		const char* const start = at;
		while (*at != '\0' && *at != ';')
			++at;

		if (chara >= 0 && chara < kMaxRemembered)
		{
			const std::string name(start, static_cast<size_t>(at - start));
			strncpy_s(g_choice[chara], name.c_str(), _TRUNCATE);
		}

		if (*at == ';')
			++at;
	}
}

void SaveChoices()
{
	std::string text;

	for (int i = 0; i < kMaxRemembered; ++i)
	{
		if (g_choice[i][0] == '\0')
			continue;

		if (!text.empty())
			text += ';';

		text += std::to_string(i) + "=" + g_choice[i];
	}

	Settings::SaveString("Palette", "Chosen", text.c_str());
}

constexpr int kReapplyWindowFrames = 240;
constexpr int kReapplyEveryFrames = 15;

constexpr int kCrossWindowFrames = 60;

int g_reapplyFramesLeft = 0;
int g_reapplyIn = 0;
int g_lastGeneration = -1;
int g_lastSeenCount = -1;

bool g_suppressRemember = false;

Folder* FindFolder(int chara)
{
	for (int i = 0; i < g_folderCount; ++i)
	{
		if (g_folders[i].chara == chara)
			return &g_folders[i];
	}

	return nullptr;
}

void LoadFolder(int chara)
{
	if (FindFolder(chara) != nullptr || g_folderCount >= kMaxFolders)
		return;

	const std::string folder = GetModPalettePath(PaletteManager::GetCharaName(chara));
	CreateDirectoryA(folder.c_str(), nullptr);

	Folder& target = g_folders[g_folderCount++];
	target.chara = chara;
	target.count = 0;

	WIN32_FIND_DATAA found = {};
	HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);
	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (target.count >= PaletteManager::kMaxPalettes)
			break;

		Entry& entry = target.entries[target.count];
		if (!PaletteFile::Load(folder + "\\" + found.cFileName, entry.colors, entry.info,
			entry.effectColors, &entry.hasEffect))
		{
			continue;
		}

		++target.count;
	}
	while (FindNextFileA(search, &found));

	FindClose(search);

	LOG("palettes: %s has %d", PaletteManager::GetCharaName(chara), target.count);
}

}

void PaletteManager::Refresh()
{
	char worn[2][PaletteFile::kNameLength] = {};

	for (int player = 0; player < 2; ++player)
		strncpy_s(worn[player], GetAppliedName(player), _TRUNCATE);

	g_folderCount = 0;

	for (int player = 0; player < 2; ++player)
	{
		const int chara = GetCharaNumber(player);
		if (chara >= 0)
			LoadFolder(chara);
	}

	for (int player = 0; player < 2; ++player)
	{
		g_applied[player] = -1;

		if (worn[player][0] == '\0')
			continue;

		const Folder* const folder = FindFolder(GetCharaNumber(player));
		if (folder == nullptr)
			continue;

		for (int i = 0; i < folder->count; ++i)
		{
			if (strncmp(folder->entries[i].info.name, worn[player], PaletteFile::kNameLength) == 0)
			{
				g_applied[player] = i;
				break;
			}
		}
	}
}

void PaletteManager::NoteHandEdited(int player)
{
	if (player >= 0 && player <= 1)
		g_handEdited[player] = true;
}

bool PaletteManager::IsHandEdited(int player)
{
	return player >= 0 && player <= 1 && g_handEdited[player];
}

int PaletteManager::GetCharaNumber(int player)
{
	return PaletteMemory::GetCharaNumber(player);
}

const char* PaletteManager::GetCharaName(int chara)
{
	if (chara < 0 || chara >= kCharaCount)
		return "unknown";

	return kCharaNames[chara];
}

int PaletteManager::GetCount(int player)
{
	const Folder* folder = FindFolder(GetCharaNumber(player));
	return folder == nullptr ? 0 : folder->count;
}

const char* PaletteManager::GetName(int player, int index)
{
	const Folder* folder = FindFolder(GetCharaNumber(player));
	if (folder == nullptr || index < 0 || index >= folder->count)
		return "";

	return folder->entries[index].info.name;
}

const char* PaletteManager::GetCreator(int player, int index)
{
	const Folder* folder = FindFolder(GetCharaNumber(player));
	if (folder == nullptr || index < 0 || index >= folder->count)
		return "";

	return folder->entries[index].info.creator;
}

int PaletteManager::GetApplied(int player)
{
	if (player < 0 || player > 1)
		return -1;

	return g_applied[player];
}

namespace {
const uint8_t* AppliedColors(int player);
}

const uint8_t* PaletteManager::GetAppliedColors(int player)
{
	if (player < 0 || player > 1)
		return nullptr;

	return AppliedColors(player);
}

const char* PaletteManager::GetAppliedName(int player)
{
	if (player < 0 || player > 1)
		return "";

	const Folder* const folder = FindFolder(GetCharaNumber(player));
	const int index = g_applied[player];

	if (folder == nullptr || index < 0 || index >= folder->count)
		return "";

	return folder->entries[index].info.name;
}

bool PaletteManager::FindEffectRow(int player, int& outTexture, int& outRow)
{
	outTexture = -1;
	outRow = -1;

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		int row = -1;
		if (!FreshestEffectDrawRow(i, row))
			continue;

		outTexture = i;
		outRow = row;
		return true;
	}

	return false;
}

namespace {

const char* OwnerKindName(PaletteTexture::OwnerKind kind)
{
	switch (kind)
	{
	case PaletteTexture::OwnerKind::Player: return "player";
	case PaletteTexture::OwnerKind::Effect: return "effect";
	case PaletteTexture::OwnerKind::CharSelect: return "charselect";
	case PaletteTexture::OwnerKind::LobbyAvatar: return "lobby";
	default: return "unknown";
	}
}

const char* ClaimName(PaletteTexture::Claim claim)
{
	switch (claim)
	{
	case PaletteTexture::Claim::Strong: return "strong";
	case PaletteTexture::Claim::Hand: return "hand";
	default: return "guess";
	}
}

}

void PaletteManager::LogTextureReport(int player)
{
	const int charaRow = PaletteTexture::GetRowForPlayer(player);
	const int owned = PaletteTexture::FindForPlayer(player);

	LOG("[wings] player %d, its row %d, its texture %d, chara %d (%s)", player, charaRow, owned,
		GetCharaNumber(player), GetCharaName(GetCharaNumber(player)));

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		const PaletteTexture::TextureOwner owner = PaletteTexture::GetOwnerInfo(i);
		const uintptr_t pointer = PaletteTexture::GetSeen(i);

		LOG("[wings] tex %d ptr 0x%08x kind %s players 0x%x chara %d claim %s binds %d frames %d "
			"lastBoundAge %d", i, static_cast<unsigned>(pointer), OwnerKindName(owner.kind),
			owner.players, owner.chara, ClaimName(PaletteTexture::GetClaim(i)),
			PaletteTexture::GetBindCount(i), PaletteTexture::GetFrameCount(i),
			PaletteTexture::GetLastBoundAge(i));

		for (int t = 0; t < PaletteDrawProbe::GetTupleCount(); ++t)
		{
			PaletteDrawProbe::Tuple tuple = {};
			if (!PaletteDrawProbe::GetTuple(t, tuple) || tuple.texture != pointer)
				continue;

			LOG("[wings]     draw row %d stage %d main %d count %d frames %d age %d chara 0x%08x",
				tuple.row, tuple.stage, tuple.mainPath ? 1 : 0, tuple.count, tuple.frames,
				tuple.lastAgeFrames, static_cast<unsigned>(tuple.chara));
		}
	}

	LOG("[wings] end of report");
}

bool PaletteManager::ApplyForeign(int player, const uint8_t* colors, const char* name,
	const uint8_t* effectColors)
{
	if (player < 0 || player > 1 || colors == nullptr)
		return false;

	const int texture = PaletteTexture::FindForPlayer(player);
	if (texture < 0)
	{
		PaletteTrace::Note("foreign p%d '%s' has no texture yet", player,
			name != nullptr ? name : "");
		return false;
	}

	if (!WriteSideRows(player, texture, colors))
	{
		PaletteTrace::Note("foreign p%d '%s' failed to write texture %d", player,
			name != nullptr ? name : "", texture);
		return false;
	}

	memcpy(g_foreign[player], colors, PaletteFile::kBytes);
	g_hasForeign[player] = true;
	strncpy_s(g_foreignName[player], name != nullptr ? name : "", _TRUNCATE);

	g_hasForeignEffect[player] = effectColors != nullptr;

	if (effectColors != nullptr)
		memcpy(g_foreignEffect[player], effectColors, PaletteFile::kBytes);

	ApplyEffectColours(player, effectColors);

	PaletteTrace::Note("foreign p%d '%s' onto texture %d row %d, effects %s", player,
		g_foreignName[player], texture, PaletteTexture::GetRowForPlayer(player),
		effectColors != nullptr ? "included" : "none");

	LOG("palettes: p%d wearing '%s' from the other player", player, g_foreignName[player]);
	return true;
}

bool PaletteManager::HasForeign(int player)
{
	return player >= 0 && player <= 1 && g_hasForeign[player];
}

const char* PaletteManager::GetForeignName(int player)
{
	if (player < 0 || player > 1)
		return "";

	return g_foreignName[player];
}

void PaletteManager::ClearForeign()
{
	for (int player = 0; player < 2; ++player)
	{
		if (g_hasForeign[player])
			PaletteTrace::Note("foreign p%d '%s' dropped", player, g_foreignName[player]);

		g_hasForeign[player] = false;
		g_hasForeignEffect[player] = false;
		g_foreignName[player][0] = '\0';
	}
}

const uint8_t* PaletteManager::GetAppliedEffectColors(int player)
{
	const Folder* const folder = FindFolder(GetCharaNumber(player));
	if (folder == nullptr)
		return nullptr;

	const int index = g_applied[player];
	if (index < 0 || index >= folder->count || !folder->entries[index].hasEffect)
		return nullptr;

	return folder->entries[index].effectColors;
}

const uint8_t* PaletteManager::GetForeignColors(int player)
{
	if (player < 0 || player > 1 || !g_hasForeign[player])
		return nullptr;

	return g_foreign[player];
}

bool PaletteManager::Apply(int player, int index)
{
	if (player < 0 || player > 1)
		return false;

	const Folder* folder = FindFolder(GetCharaNumber(player));
	if (folder == nullptr || index < 0 || index >= folder->count)
	{
		PaletteTrace::Note("apply p%d %d refused, no folder or index out of range", player, index);
		return false;
	}

	const int texture = PaletteTexture::FindForPlayer(player);
	const int row = PaletteTexture::GetRowForPlayer(player);

	if (texture < 0 || row < 0)
	{
		PaletteTrace::Note("apply p%d '%s' refused, texture %d row %d", player,
			folder->entries[index].info.name, texture, row);
		return false;
	}

	const Entry& entry = folder->entries[index];

	if (!WriteSideRows(player, texture, entry.colors))
	{
		PaletteTrace::Note("apply p%d '%s' failed to write texture %d row %d", player,
			entry.info.name, texture, row);
		return false;
	}

	PaletteTrace::Note("apply p%d '%s' onto texture %d row %d%s", player, entry.info.name, texture,
		row, g_suppressRemember ? " (repaint)" : "");

	const bool changed = g_applied[player] != index;
	g_applied[player] = index;

	PaletteDrawProbe::SyncEffectSides(GetCharaNumber(0), g_applied[0],
		GetCharaNumber(1), g_applied[1]);

	uint8_t autoEffect[PaletteFile::kBytes] = {};
	const uint8_t* effectColors = entry.hasEffect ? entry.effectColors : nullptr;

	if (effectColors == nullptr &&
		PartColourTable::BuildAutoEffectBlock(folder->chara, entry.colors, autoEffect))
	{
		effectColors = autoEffect;
	}

	ApplyEffectColours(player, effectColors);

	if (!g_suppressRemember)
		g_handEdited[player] = false;

	if (!g_suppressRemember && folder->chara >= 0 && folder->chara < kMaxRemembered)
	{
		strncpy_s(g_choice[folder->chara], folder->entries[index].info.name, _TRUNCATE);
		SaveChoices();
	}

	if (changed)
		LOG("palettes: p%d wearing '%s'", player, folder->entries[index].info.name);

	return true;
}

bool PaletteManager::Restore(int player, bool alsoEffects)
{
	if (player < 0 || player > 1)
		return false;

	const int texture = PaletteTexture::FindForPlayer(player);
	if (texture < 0)
		return false;

	PaletteTrace::Note("restore p%d texture %d, effects %s", player, texture,
		alsoEffects ? "too" : "kept");

	PaletteTexture::RestoreAll(texture);

	if (alsoEffects)
		EffectPaint::Clear(player);

	g_handEdited[player] = false;

	const int effectRow = PaletteTexture::GetRowForPlayer(player);
	if (effectRow >= 0)
	{
		for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
		{
			if (PaletteTexture::GetOwnerInfo(i).kind == PaletteTexture::OwnerKind::Effect)
				PaletteTexture::Restore(i, static_cast<unsigned>(effectRow));
		}
	}

	g_applied[player] = -1;

	const int chara = GetCharaNumber(player);

	if (chara >= 0 && chara < kMaxRemembered)
	{
		g_choice[chara][0] = '\0';
		SaveChoices();
	}

	return true;
}

namespace {

const uint8_t* AppliedColors(int player)
{
	const Folder* const folder = FindFolder(PaletteManager::GetCharaNumber(player));
	if (folder == nullptr)
		return nullptr;

	const int index = g_applied[player];
	if (index < 0 || index >= folder->count)
		return nullptr;

	return folder->entries[index].colors;
}

void ProbeCells()
{
	const uint8_t* const colors[2] = { AppliedColors(0), AppliedColors(1) };

	if (colors[0] == nullptr && colors[1] == nullptr)
		return;

	char line[256] = {};
	int at = 0;

	for (int i = 0; i < PaletteTexture::GetSeenCount() && at < 200; ++i)
	{
		at += sprintf_s(line + at, sizeof(line) - at, "  t%d(p%d)", i, PaletteTexture::GetOwner(i));

		for (unsigned row = 0; row < 2; ++row)
		{
			char who = '-';

			for (int player = 0; player < 2; ++player)
			{
				if (colors[player] != nullptr && PaletteTexture::RowHasRgb(i, row, colors[player]))
					who = static_cast<char>('0' + player);
			}

			at += sprintf_s(line + at, sizeof(line) - at, " r%u=%c", row, who);
		}
	}

	LOG("palettes: mod colours are at%s", line);
}

constexpr int kSideSettleFrames = 3;

int g_sideSettling = 0;

bool IsPlayerOnTheRight(int player, bool& outKnown)
{
	outKnown = false;

	void* const first = MemoryMap::GetCharaSlot(0);
	void* const second = MemoryMap::GetCharaSlot(1);

	if (first == nullptr || second == nullptr)
		return false;

	int firstX = 0;
	int secondX = 0;
	int ignored = 0;

	if (!Camera::GetWorldPosition(first, firstX, ignored) ||
		!Camera::GetWorldPosition(second, secondX, ignored))
	{
		return false;
	}

	if (firstX == secondX)
		return false;

	outKnown = true;

	const bool firstIsRight = firstX > secondX;
	return player == 0 ? firstIsRight : !firstIsRight;
}

void WatchSides()
{
	bool known = false;
	const bool swapped = IsPlayerOnTheRight(0, known);

	if (!known)
		return;

	if (swapped == PaletteTexture::GetRowsSwapped())
	{
		g_sideSettling = 0;
		return;
	}

	if (++g_sideSettling < kSideSettleFrames)
		return;

	g_sideSettling = 0;

	PaletteTexture::SetRowsSwapped(swapped);
	RestoreEffectRows();

	if (g_applied[0] >= 0 || g_applied[1] >= 0)
		ProbeCells();

	LOG("palettes: P1 is on the %s now, rows %s - repainting", swapped ? "right" : "left",
		swapped ? "swapped" : "as they started");

	PaletteManager::Reapply();

	if (g_reapplyFramesLeft < kCrossWindowFrames)
		g_reapplyFramesLeft = kCrossWindowFrames;

	g_reapplyIn = 0;
}

void EnsureFolders()
{
	for (int player = 0; player < 2; ++player)
	{
		const int chara = PaletteManager::GetCharaNumber(player);
		if (chara < 0 || FindFolder(chara) != nullptr)
			continue;

		LoadFolder(chara);
		PaletteTrace::Note("loaded %s's palettes late", PaletteManager::GetCharaName(chara));

		if (FindFolder(chara) != nullptr)
			PaletteManager::Reapply();
	}
}

bool HasCompanion(int player)
{
	const int chara = PaletteManager::GetCharaNumber(player);

	return chara >= 0 && chara < 32 && g_modVals.paletteCompanion[chara];
}

constexpr int kOutOfMatchFreshFrames = 60;
constexpr int kOutOfMatchRepaintFrames = 15;

int g_oomPaintedRow[PaletteTexture::kMaxSeen] = {};
bool g_oomPaintedAll[PaletteTexture::kMaxSeen] = {};
int g_oomGeneration = -1;
int g_oomRepaintIn = 0;

const Entry* FindChoiceEntry(int chara)
{
	if (chara < 0 || chara >= kMaxRemembered || g_choice[chara][0] == '\0')
		return nullptr;

	Folder* folder = FindFolder(chara);
	if (folder == nullptr)
	{
		LoadFolder(chara);
		folder = FindFolder(chara);
		if (folder == nullptr)
			return nullptr;
	}

	for (int i = 0; i < folder->count; ++i)
	{
		if (strncmp(folder->entries[i].info.name, g_choice[chara], PaletteFile::kNameLength) == 0)
			return &folder->entries[i];
	}

	return nullptr;
}

void ApplyOutOfMatch()
{
	if (!g_modVals.paletteOutOfMatch)
		return;

	LoadChoices();

	const int generation = PaletteTexture::GetGeneration();
	if (generation != g_oomGeneration)
	{
		g_oomGeneration = generation;
		for (int i = 0; i < PaletteTexture::kMaxSeen; ++i)
		{
			g_oomPaintedRow[i] = -1;
			g_oomPaintedAll[i] = false;
		}
	}

	if (--g_oomRepaintIn > 0)
		return;
	g_oomRepaintIn = kOutOfMatchRepaintFrames;

	for (int i = 0; i < PaletteTexture::GetSeenCount() && i < PaletteTexture::kMaxSeen; ++i)
	{
		const PaletteTexture::TextureOwner owner = PaletteTexture::GetOwnerInfo(i);

		if (owner.kind != PaletteTexture::OwnerKind::CharSelect &&
			owner.kind != PaletteTexture::OwnerKind::LobbyAvatar)
		{
			continue;
		}

		const Entry* const entry = FindChoiceEntry(owner.chara);
		if (entry == nullptr)
			continue;

		int row = -1;
		int bestAge = kOutOfMatchFreshFrames;
		const uintptr_t pointer = PaletteTexture::GetSeen(i);

		for (int t = 0; t < PaletteDrawProbe::GetTupleCount(); ++t)
		{
			PaletteDrawProbe::Tuple tuple = {};
			if (!PaletteDrawProbe::GetTuple(t, tuple))
				continue;

			if (tuple.texture != pointer || tuple.lastAgeFrames >= bestAge)
				continue;

			bestAge = tuple.lastAgeFrames;
			row = tuple.row;
		}

		if (row < 0)
		{
			if (g_oomPaintedAll[i] && PaletteTexture::RowHasRgb(i, 0, entry->colors))
				continue;

			for (unsigned r = 0; r < PaletteTexture::kRows; ++r)
				PaletteTexture::WriteRowKeepingAlpha(i, r, entry->colors);

			if (!g_oomPaintedAll[i])
			{
				g_oomPaintedAll[i] = true;
				LOG("palettes: %s texture of %s wearing '%s' on every row",
					owner.kind == PaletteTexture::OwnerKind::LobbyAvatar ? "lobby" : "select",
					PaletteManager::GetCharaName(owner.chara), entry->info.name);
			}

			continue;
		}

		if (row >= static_cast<int>(PaletteTexture::kRows))
			continue;

		if (g_oomPaintedRow[i] != row)
		{
			if (g_oomPaintedRow[i] >= 0)
				PaletteTexture::Restore(i, static_cast<unsigned>(g_oomPaintedRow[i]));

			g_oomPaintedRow[i] = row;
			LOG("palettes: %s portrait of %s wearing '%s' on row %d",
				owner.kind == PaletteTexture::OwnerKind::LobbyAvatar ? "lobby" : "select",
				PaletteManager::GetCharaName(owner.chara), entry->info.name, row);
		}

		PaletteTexture::WriteRowKeepingAlpha(i, static_cast<unsigned>(row), entry->colors);
	}
}

}

void PaletteManager::DetectSwapSides()
{
	const bool p1 = HasCompanion(0);
	const bool p2 = HasCompanion(1);
	const bool swap = p1 && !p2;

	if (swap == PaletteTexture::GetSwapSides())
		return;

	PaletteTexture::SetSwapSides(swap);

	LOG("palettes: companions p1=%d (%s) p2=%d (%s), sides %s", p1 ? 1 : 0,
		GetCharaName(GetCharaNumber(0)), p2 ? 1 : 0, GetCharaName(GetCharaNumber(1)),
		swap ? "swapped" : "as bound");
}

void PaletteManager::RepaintSides()
{
	for (int player = 0; player < 2; ++player)
	{
		const int texture = PaletteTexture::FindForPlayer(player);
		if (texture < 0)
			continue;

		const uint8_t* const colors = g_hasForeign[player] ? g_foreign[player]
			: GetAppliedColors(player);

		if (colors == nullptr)
			continue;

		WriteSideRows(player, texture, colors);

		if (!g_hasForeign[player])
			continue;

		ApplyEffectColours(player, g_hasForeignEffect[player] ? g_foreignEffect[player] : nullptr);
	}
}

void PaletteManager::Reapply()
{
	LoadChoices();

	g_suppressRemember = true;

	for (int player = 0; player < 2; ++player)
	{
		if (g_handEdited[player])
			continue;

		const int chara = GetCharaNumber(player);
		if (chara < 0 || chara >= kMaxRemembered || g_choice[chara][0] == '\0')
			continue;

		const Folder* const folder = FindFolder(chara);
		if (folder == nullptr)
			continue;

		for (int i = 0; i < folder->count; ++i)
		{
			if (strncmp(folder->entries[i].info.name, g_choice[chara],
				PaletteFile::kNameLength) != 0)
			{
				continue;
			}

			Apply(player, i);
			break;
		}
	}

	g_suppressRemember = false;
}

void PaletteManager::OnFrame()
{
	const bool inMatch = GameState::IsInMatch();

	if (inMatch == g_inMatch)
	{
		g_pending = false;
		g_pendingFrames = 0;

		if (g_inMatch)
			DetectSwapSides();

		if (!g_inMatch)
		{
			ApplyOutOfMatch();
			return;
		}

		EnsureFolders();
		WatchSides();

		PaletteDrawProbe::SyncEffectSides(GetCharaNumber(0), g_applied[0],
			GetCharaNumber(1), g_applied[1]);

		const int generation = PaletteTexture::GetGeneration();
		const int seen = PaletteTexture::GetSeenCount();

		if (generation != g_lastGeneration || seen != g_lastSeenCount)
		{
			PaletteTrace::Note("textures turned over, generation %d -> %d, seen %d -> %d, "
				"repainting for %d frames", g_lastGeneration, generation, g_lastSeenCount, seen,
				kReapplyWindowFrames);

			g_lastGeneration = generation;
			g_lastSeenCount = seen;
			g_reapplyFramesLeft = kReapplyWindowFrames;
			g_reapplyIn = 0;
		}

		if (g_reapplyFramesLeft > 0)
		{
			--g_reapplyFramesLeft;

			if (--g_reapplyIn <= 0)
			{
				g_reapplyIn = kReapplyEveryFrames;
				Reapply();
			}
		}

		return;
	}

	if (!g_pending)
	{
		g_pending = true;
		g_pendingFrames = 0;
	}

	if (!inMatch && ++g_pendingFrames < kSessionSettleFrames)
		return;

	g_inMatch = inMatch;
	g_pending = false;
	g_pendingFrames = 0;

	const int before = PaletteTexture::GetSeenCount();

	if (inMatch)
	{
		PaletteTrace::Reset();
		PaletteTrace::Note("match started");
		PaletteTexture::ResetMatchEvidence();
	}
	else
	{
		PaletteTexture::ForgetMatchTextures();
		PaletteMemory::ForgetLoads();
	}

	PaletteIdentity::Reset();
	PaletteDrawProbe::ResetMatchState();

	const int forgotten = before - PaletteTexture::GetSeenCount();

	if (inMatch)
		Refresh();

	g_reapplyFramesLeft = 0;
	g_lastGeneration = -1;
	g_lastSeenCount = -1;
	g_sideSettling = 0;

	if (!inMatch)
		PaletteTrace::Note("match ended, %d texture(s) forgotten", forgotten);

	LOG("palettes: match %s, %d texture(s) forgotten", inMatch ? "started" : "ended", forgotten);
}
