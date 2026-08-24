#include "Palette/PalettePaint.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameState.h"
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

	uintptr_t owner;
	uintptr_t texture;

	int index;
	uint32_t painted;

	uint8_t alpha[2][PalettePaint::kColours];
	uint32_t haveAlpha;

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

int ResolveOwner(uintptr_t owner)
{
	uintptr_t candidates[PaletteSeat::kCandidates] = {};
	const int count = PaletteSeat::GetCandidates(owner, candidates, PaletteSeat::kCandidates);

	for (int i = 0; i < count; ++i)
	{
		const int index = ResolveNamed(candidates[i]);

		if (index >= 0)
			return index;
	}

	return -1;
}

void Release(Player& entry)
{
	for (unsigned row = 0; row < 2; ++row)
	{
		if ((entry.painted & (1u << row)) != 0)
			PaletteTexture::Restore(entry.index, row);
	}

	entry.painted = 0;
	entry.haveAlpha = 0;
	entry.index = -1;
	entry.texture = 0;
	entry.painting = false;
}

bool CacheAlpha(Player& entry, unsigned row)
{
	if ((entry.haveAlpha & (1u << row)) != 0)
		return true;

	uint8_t current[PalettePaint::kBytes] = {};

	if (!PaletteTexture::ReadRow(entry.index, row, current))
		return false;

	for (int i = 0; i < PalettePaint::kColours; ++i)
		entry.alpha[row][i] = current[i * 4 + 3];

	entry.haveAlpha |= 1u << row;
	return true;
}

void PaintRow(Player& entry, unsigned row, const uint8_t* source)
{
	if (!CacheAlpha(entry, row))
		return;

	uint8_t native[PalettePaint::kBytes] = {};

	for (int i = 0; i < PalettePaint::kColours; ++i)
	{
		native[i * 4 + 0] = source[i * 4 + 2];
		native[i * 4 + 1] = source[i * 4 + 1];
		native[i * 4 + 2] = source[i * 4 + 0];
		native[i * 4 + 3] = entry.alpha[row][i];
	}

	if (!PaletteTexture::WriteRow(entry.index, row, native))
		return;

	entry.painted |= 1u << row;
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

	if (owner != 0)
		entry.owner = owner;
}

void PalettePaint::Clear(int player)
{
	if (player < 0 || player >= kPlayers)
		return;

	Player& entry = g_players[player];

	entry.staged = false;
	entry.previewing = false;
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
				Release(entry);
				entry.owner = fresh;
			}
		}

		const int index = ResolveOwner(entry.owner);

		PaletteTexture::NoteInUse(index);

		const uintptr_t resolved = PaletteTexture::GetSeen(index);

		if (entry.index != index || entry.texture != resolved)
		{
			LOG("palette paint: p%d owner 0x%08x names texture %d, was %d", player,
				static_cast<unsigned>(entry.owner), index, entry.index);

			Release(entry);
		}

		if (!entry.staged && !entry.previewing && !entry.hasRemote && entry.painted != 0)
			Release(entry);

		entry.index = index;
		entry.texture = resolved;
		entry.painting = index >= 0 && entry.painted != 0;
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
			if (entry.painted != 0)
				Release(entry);

			continue;
		}

		const uint8_t* const source = entry.index >= 0 ? SourceFor(entry) : nullptr;

		if (source == nullptr)
			continue;

		for (unsigned row = 0; row < 2; ++row)
			PaintRow(entry, row, source);
	}
}

bool PalettePaint::IsPainting(int player)
{
	return player >= 0 && player < kPlayers && g_players[player].painting
		&& g_players[player].painted != 0;
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
