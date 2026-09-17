#include "Palette/PalettePaint.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteSeat.h"
#include "Palette/PaletteTexture.h"
#include "Palette/PlayerSides.h"

#include <cstring>

namespace {

struct Player
{
	uint8_t colours[PalettePaint::kBytes];
	bool staged;

	uint8_t preview[PalettePaint::kBytes];
	bool previewing;

	uint8_t remote[PalettePaint::kBytes];
	bool hasRemote;

	uint8_t companion[PalettePaint::kBytes];
	uint8_t previewCompanion[PalettePaint::kBytes];
	bool hasCompanion;

	uint32_t rows;

	uintptr_t owner;
	uintptr_t texture;

	int index;
	int indices[PaletteSeat::kCandidates];
	int count;
	uint32_t painted[PaletteSeat::kCandidates];

	uint8_t alpha[PaletteSeat::kCandidates][PalettePaint::kRows][PalettePaint::kColours];
	uint32_t haveAlpha[PaletteSeat::kCandidates];
	uint32_t reported[PaletteSeat::kCandidates];

	uint8_t lastNative[PaletteSeat::kCandidates][PalettePaint::kRows][PalettePaint::kBytes];

	int writes;
	bool painting;

	unsigned revision;
};

Player g_players[PalettePaint::kPlayers] = {};
int g_paintedFrame = -1;
bool g_allowed = false;

int g_innerOffset = -1;

int ResolveAt(uintptr_t named, int offset)
{
	uintptr_t inner = 0;

	if (!TryReadMemory(&inner, reinterpret_cast<const void*>(named + offset), sizeof(inner)))
		return -1;

	return PaletteTexture::FindByPointer(inner);
}

int LearnInnerOffset(uintptr_t named)
{
	int learned = -1;
	int index = -1;

	for (int offset = 4; offset <= 0x200; offset += 4)
	{
		const int at = ResolveAt(named, offset);

		if (at < 0)
			continue;

		if (learned >= 0)
			return -1;

		learned = offset;
		index = at;
	}

	if (learned < 0)
		return -1;

	g_innerOffset = learned;
	return index;
}

int ResolveNamed(uintptr_t named)
{
	if (named == 0)
		return -1;

	const int direct = PaletteTexture::FindByPointer(named);

	if (direct >= 0)
		return direct;

	if (g_innerOffset > 0)
		return ResolveAt(named, g_innerOffset);

	return LearnInnerOffset(named);
}

int ResolveOwners(uintptr_t owner, int* out, int max)
{
	uintptr_t candidates[PaletteSeat::kCandidates] = {};
	const int count = PaletteSeat::GetCandidates(owner, candidates, PaletteSeat::kCandidates);

	int found = 0;

	for (int i = 0; i < count && found < max; ++i)
	{
		const int index = ResolveNamed(candidates[i]);

		if (index < 0)
			continue;

		bool seen = false;

		for (int slot = 0; slot < found; ++slot)
			seen = seen || out[slot] == index;

		if (!seen)
			out[found++] = index;
	}

	return found;
}

bool AnyPainted(const Player& entry)
{
	for (int slot = 0; slot < entry.count; ++slot)
	{
		if (entry.painted[slot] != 0)
			return true;
	}

	return false;
}

void Release(Player& entry)
{
	for (int slot = 0; slot < entry.count; ++slot)
	{
		for (unsigned row = 0; row < PalettePaint::kRows; ++row)
		{
			if ((entry.painted[slot] & (1u << row)) != 0)
				PaletteTexture::Restore(entry.indices[slot], row);
		}
	}

	memset(entry.painted, 0, sizeof(entry.painted));
	memset(entry.haveAlpha, 0, sizeof(entry.haveAlpha));
	memset(entry.reported, 0, sizeof(entry.reported));

	entry.count = 0;
	entry.index = -1;
	entry.texture = 0;
	entry.painting = false;
}

bool CacheAlpha(Player& entry, int slot, unsigned row)
{
	if ((entry.haveAlpha[slot] & (1u << row)) != 0)
		return true;

	uint8_t current[PalettePaint::kBytes] = {};

	if (!PaletteTexture::ReadRow(entry.indices[slot], row, current))
		return false;

	for (int i = 0; i < PalettePaint::kColours; ++i)
		entry.alpha[slot][row][i] = current[i * 4 + 3];

	entry.haveAlpha[slot] |= 1u << row;
	return true;
}

void PaintSlot(Player& entry, int slot, const uint8_t* source, const uint8_t* companion)
{
	const uint32_t drawn = entry.rows != 0 ? entry.rows : 3u;

	uint8_t native[PalettePaint::kRows][PalettePaint::kBytes] = {};
	const uint8_t* ready[PalettePaint::kRows] = {};
	unsigned rows[PalettePaint::kRows] = {};
	int count = 0;

	for (unsigned row = 0; row < PalettePaint::kRows; ++row)
	{
		const uint8_t* const from = row < 2 ? source : companion;

		if (from == nullptr || (drawn & (1u << row)) == 0)
			continue;

		const bool tell = (entry.reported[slot] & (1u << row)) == 0;

		if (tell)
			entry.reported[slot] |= 1u << row;

		if (!CacheAlpha(entry, slot, row))
		{
			if (tell)
				LOG("palette paint: texture %d row %u not read, nothing written",
					entry.indices[slot], row);

			continue;
		}

		uint8_t composed[PalettePaint::kBytes];

		for (int i = 0; i < PalettePaint::kColours; ++i)
		{
			composed[i * 4 + 0] = from[i * 4 + 2];
			composed[i * 4 + 1] = from[i * 4 + 1];
			composed[i * 4 + 2] = from[i * 4 + 0];
			composed[i * 4 + 3] = entry.alpha[slot][row][i];
		}

		const bool alreadyPainted = (entry.painted[slot] & (1u << row)) != 0;

		if (alreadyPainted && memcmp(composed, entry.lastNative[slot][row], PalettePaint::kBytes) == 0)
			continue;

		memcpy(native[count], composed, PalettePaint::kBytes);
		memcpy(entry.lastNative[slot][row], composed, PalettePaint::kBytes);

		if (tell)
			LOG("palette paint: texture %d row %u written", entry.indices[slot], row);

		ready[count] = native[count];
		rows[count] = row;
		++count;
	}

	if (count == 0)
		return;

	if (!PaletteTexture::WriteRowSet(entry.indices[slot], rows, ready, count))
		return;

	for (int i = 0; i < count; ++i)
		entry.painted[slot] |= 1u << rows[i];

	++entry.writes;
}

const uint8_t* SourceFor(const Player& entry)
{
	if (entry.previewing)
		return entry.preview;

	if (entry.hasRemote)
		return entry.remote;

	if (entry.staged)
		return entry.colours;

	return nullptr;
}

int ReadBaseX(int player)
{
	void* const chara = MemoryMap::GetCharaSlot(player);
	uint32_t x = 0;

	if (chara == nullptr || !MemoryMap::ReadStructDword(chara, GameOffsets::kPlayerDataBaseX, x))
		return 0;

	return static_cast<int>(x);
}

int FindPointerIn(int player, uintptr_t value)
{
	void* const chara = MemoryMap::GetCharaSlot(player);

	if (chara == nullptr)
		return -1;

	uint32_t data[GameOffsets::kPlayerDataSize / 4] = {};

	if (!TryReadMemory(data, chara, sizeof(data)))
		return -1;

	for (int i = 0; i < static_cast<int>(GameOffsets::kPlayerDataSize / 4); ++i)
	{
		if (data[i] == value)
			return i * 4;
	}

	return -1;
}

void LogLatch(int player, uintptr_t owner)
{
	LOG("palette latch: p%d takes owner 0x%08x  screen side %d  seat side %d  x %d vs %d  "
		"in p0 at %d  in p1 at %d", player, static_cast<unsigned>(owner),
		PlayerSides::ScreenSideOf(player), PaletteSeat::GetSideByOwner(owner),
		ReadBaseX(player), ReadBaseX(player == 0 ? 1 : 0),
		FindPointerIn(0, owner), FindPointerIn(1, owner));
}

uintptr_t OwnerFor(int player)
{
	const int side = PlayerSides::ScreenSideOf(player);

	return side >= 0 ? PaletteSeat::GetOwner(side) : 0;
}

}

void PalettePaint::Stage(int player, const uint8_t* colours)
{
	if (player < 0 || player >= kPlayers || colours == nullptr)
		return;

	Player& entry = g_players[player];

	memcpy(entry.colours, colours, kBytes);
	entry.staged = true;
	++entry.revision;

	const uintptr_t owner = OwnerFor(player);

	if (owner == 0 || owner == entry.owner)
		return;

	LogLatch(player, owner);
	entry.owner = owner;
}

void PalettePaint::StageCompanion(int player, const uint8_t* colours)
{
	if (player < 0 || player >= kPlayers || colours == nullptr)
		return;

	Player& entry = g_players[player];

	memcpy(entry.companion, colours, kBytes);
	entry.hasCompanion = true;
	++entry.revision;
}

bool PalettePaint::HasCompanion(int player)
{
	return player >= 0 && player < kPlayers && g_players[player].hasCompanion;
}

void PalettePaint::PreviewCompanion(int player, const uint8_t* colours)
{
	if (player < 0 || player >= kPlayers || colours == nullptr)
		return;

	memcpy(g_players[player].previewCompanion, colours, kBytes);
}

void PalettePaint::ClearCompanion(int player)
{
	if (player < 0 || player >= kPlayers || !g_players[player].hasCompanion)
		return;

	g_players[player].hasCompanion = false;
	++g_players[player].revision;
}

bool PalettePaint::ReadCompanionColours(int player, uint8_t* rgba)
{
	if (player < 0 || player >= kPlayers || rgba == nullptr)
		return false;

	const int index = g_players[player].index;

	if (index < 0)
		return false;

	int side = PaletteSeat::GetSideByOwner(g_players[player].owner);

	if (side < 0)
		side = PlayerSides::ScreenSideOf(player);

	if (side < 0)
		return false;

	return PaletteTexture::ReadPristineRowAsRgba(index, static_cast<unsigned>(2 + side), rgba);
}

void PalettePaint::Clear(int player)
{
	if (player < 0 || player >= kPlayers)
		return;

	Player& entry = g_players[player];

	entry.staged = false;
	entry.previewing = false;
	entry.hasCompanion = false;
	++entry.revision;

	if (entry.hasRemote)
		return;

	Release(entry);

	entry.owner = 0;
}

void PalettePaint::StageRemote(int player, const uint8_t* colours)
{
	if (player < 0 || player >= kPlayers || colours == nullptr)
		return;

	Player& entry = g_players[player];

	memcpy(entry.remote, colours, kBytes);
	entry.hasRemote = true;

	if (entry.owner == 0)
		entry.owner = OwnerFor(player);
}

void PalettePaint::ClearRemote(int player)
{
	if (player < 0 || player >= kPlayers || !g_players[player].hasRemote)
		return;

	Player& entry = g_players[player];

	entry.hasRemote = false;

	if (!entry.staged && !entry.previewing)
	{
		Release(entry);
		entry.owner = 0;
	}
}

bool PalettePaint::HasRemote(int player)
{
	return player >= 0 && player < kPlayers && g_players[player].hasRemote;
}

const uint8_t* PalettePaint::GetRemote(int player)
{
	if (player < 0 || player >= kPlayers || !g_players[player].hasRemote)
		return nullptr;

	return g_players[player].remote;
}

bool PalettePaint::IsStaged(int player)
{
	return player >= 0 && player < kPlayers && g_players[player].staged;
}

void PalettePaint::Preview(int player, const uint8_t* colours)
{
	if (player < 0 || player >= kPlayers || colours == nullptr)
		return;

	Player& entry = g_players[player];

	memcpy(entry.preview, colours, kBytes);
	entry.previewing = true;

	if (entry.owner == 0)
		entry.owner = OwnerFor(player);
}

void PalettePaint::EndPreview(int player)
{
	if (player < 0 || player >= kPlayers || !g_players[player].previewing)
		return;

	Player& entry = g_players[player];

	entry.previewing = false;

	if (!entry.staged && !entry.hasRemote)
	{
		Release(entry);
		entry.owner = 0;
	}
}

void PalettePaint::OnFrame()
{

	static bool wasInMatch = false;
	const bool inMatch = GameState::IsInMatch();

	if (wasInMatch != inMatch)
	{
		wasInMatch = inMatch;
		EffectPaint::Forget();
	}

	g_allowed = inMatch || g_modVals.paletteOutOfMatch;

	if (!g_allowed)
	{
		for (int player = 0; player < kPlayers; ++player)
			Release(g_players[player]);

		return;
	}

	for (int player = 0; player < kPlayers; ++player)
	{
		Player& entry = g_players[player];

		uintptr_t named = 0;
		uint32_t rows = 0;

		if (entry.owner == 0 || !PaletteSeat::GetByOwner(entry.owner, named, rows))
		{
			const uintptr_t fresh = OwnerFor(player);
			const uintptr_t theirs = g_players[player == 0 ? 1 : 0].owner;

			if (fresh != 0 && fresh != entry.owner && fresh != theirs)
			{
				LogLatch(player, fresh);
				Release(entry);
				entry.owner = fresh;
			}
		}

		if (rows != 0)
			entry.rows = rows;

		int found[PaletteSeat::kCandidates] = {};
		const int count = ResolveOwners(entry.owner, found, PaletteSeat::kCandidates);

		for (int slot = 0; slot < count; ++slot)
			PaletteTexture::NoteInUse(found[slot]);

		const int index = count > 0 ? found[0] : -1;
		const uintptr_t resolved = PaletteTexture::GetSeen(index);

		bool same = count == entry.count && entry.texture == resolved;

		for (int slot = 0; slot < count && same; ++slot)
			same = entry.indices[slot] == found[slot];

		if (!same)
		{
			LOG("palette paint: p%d owner 0x%08x names %d texture(s) %d %d %d %d, was %d", player,
				static_cast<unsigned>(entry.owner), count,
				count > 0 ? found[0] : -1, count > 1 ? found[1] : -1,
				count > 2 ? found[2] : -1, count > 3 ? found[3] : -1, entry.index);

			Release(entry);
		}

		if (!entry.staged && !entry.previewing && !entry.hasRemote && AnyPainted(entry))
			Release(entry);

		for (int slot = 0; slot < count; ++slot)
			entry.indices[slot] = found[slot];

		entry.count = count;
		entry.index = index;
		entry.texture = resolved;
		entry.painting = index >= 0 && AnyPainted(entry);
	}
}

bool PalettePaint::ReadGameColours(int player, uint8_t* rgba)
{
	if (player < 0 || player >= kPlayers || rgba == nullptr)
		return false;

	const int index = g_players[player].index;

	if (index < 0)
		return false;

	int side = PaletteSeat::GetSideByOwner(g_players[player].owner);

	if (side < 0)
		side = PlayerSides::ScreenSideOf(player);

	if (side < 0)
		return false;

	return PaletteTexture::ReadPristineRowAsRgba(index, static_cast<unsigned>(side), rgba);
}


void PalettePaint::OnDraw()
{
	if (!g_allowed)
		return;

	const int frame = PaletteSeat::GetFrame();

	if (g_paintedFrame == frame)
		return;

	g_paintedFrame = frame;

	for (int player = 0; player < kPlayers; ++player)
	{
		Player& entry = g_players[player];

		if (!PaletteControl::CanWear(player))
		{
			if (AnyPainted(entry))
				Release(entry);

			continue;
		}

		const uint8_t* const source = entry.count > 0 ? SourceFor(entry) : nullptr;

		if (source == nullptr)
			continue;

		const uint8_t* companion = nullptr;

		if (entry.hasCompanion)
			companion = entry.previewing ? entry.previewCompanion : entry.companion;

		for (int slot = 0; slot < entry.count; ++slot)
			PaintSlot(entry, slot, source, companion);
	}
}

bool PalettePaint::IsPainting(int player)
{
	return player >= 0 && player < kPlayers && g_players[player].painting
		&& AnyPainted(g_players[player]);
}

const uint8_t* PalettePaint::GetStaged(int player)
{
	if (player < 0 || player >= kPlayers || !g_players[player].staged)
		return nullptr;

	return g_players[player].colours;
}

unsigned PalettePaint::GetRevision(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].revision : 0;
}

int PalettePaint::GetWrites(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].writes : 0;
}

int PalettePaint::GetIndex(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].index : -1;
}

uintptr_t PalettePaint::GetOwner(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].owner : 0;
}

int PalettePaint::GetInnerOffset()
{
	return g_innerOffset;
}
