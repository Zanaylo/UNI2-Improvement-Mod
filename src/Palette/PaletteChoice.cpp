#include "Palette/PaletteChoice.h"

#include "Core/logger.h"
#include "Core/Settings.h"
#include "Core/utils.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PalettePaint.h"

#include <Windows.h>

#include <cstring>
#include <string>

namespace {

constexpr const char* kSection = "PaletteChoice";

char g_remembered[PaletteFile::kNameLength + 8] = {};

struct Player
{
	int chara = -1;
	bool tried = false;
	unsigned generation = 0;
	char worn[PaletteFile::kNameLength + 8] = {};
};

Player g_players[PaletteChoice::kPlayers] = {};

std::string PathFor(int chara, const char* file)
{
	return GetModPalettePath(PaletteManager::GetCharaName(chara)) + "\\" + file;
}

}

const char* PaletteChoice::Remembered(int chara)
{
	g_remembered[0] = '\0';

	if (chara < 0)
		return g_remembered;

	GetPrivateProfileStringA(kSection, PaletteManager::GetCharaName(chara), "", g_remembered,
		sizeof(g_remembered), Settings::GetIniPath().c_str());

	return g_remembered;
}

void PaletteChoice::Remember(int chara, const char* file)
{
	if (chara < 0 || file == nullptr || file[0] == '\0')
		return;

	Settings::SaveString(kSection, PaletteManager::GetCharaName(chara), file);
}

void PaletteChoice::Forget(int chara)
{
	if (chara < 0)
		return;

	Settings::SaveString(kSection, PaletteManager::GetCharaName(chara), "");
}

bool PaletteChoice::Apply(int player, int chara, const char* file)
{
	if (player < 0 || player >= kPlayers || chara < 0 || file == nullptr || file[0] == '\0')
		return false;

	uint8_t colours[PaletteFile::kBytes] = {};
	uint8_t effects[PaletteFile::kBytes] = {};
	PaletteFile::Info info = {};
	bool hasEffects = false;

	if (!PaletteFile::Load(PathFor(chara, file), colours, info, effects, &hasEffects))
		return false;

	PalettePaint::Stage(player, colours);
	EffectPaint::SetBlock(player, hasEffects ? effects : nullptr);

	NoteWorn(player, file);
	return true;
}

bool PaletteChoice::Wear(int player, const char* file)
{
	if (player < 0 || player >= kPlayers)
		return false;

	const int chara = PaletteMemory::GetCharaNumber(player);

	if (!Apply(player, chara, file))
		return false;

	Remember(chara, file);

	g_players[player].chara = chara;
	g_players[player].tried = true;
	++g_players[player].generation;

	return true;
}

void PaletteChoice::Bare(int player)
{
	if (player < 0 || player >= kPlayers)
		return;

	PalettePaint::Clear(player);
	EffectPaint::Clear(player);

	Forget(PaletteMemory::GetCharaNumber(player));
	NoteBare(player);

	g_players[player].tried = true;
	++g_players[player].generation;
}

const char* PaletteChoice::WornFile(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].worn : "";
}

void PaletteChoice::NoteWorn(int player, const char* file)
{
	if (player < 0 || player >= kPlayers)
		return;

	strncpy_s(g_players[player].worn, file != nullptr ? file : "", _TRUNCATE);
}

void PaletteChoice::NoteBare(int player)
{
	if (player >= 0 && player < kPlayers)
		g_players[player].worn[0] = '\0';
}

unsigned PaletteChoice::GetGeneration(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].generation : 0;
}

namespace {

void Dress(int player, int chara)
{
	char file[sizeof(g_remembered)] = {};
	strncpy_s(file, PaletteChoice::Remembered(chara), _TRUNCATE);

	if (file[0] == 0)
		return;

	if (!PaletteChoice::Apply(player, chara, file))
	{
		LOG("palettes: p%d's remembered '%s' could not be read", player, file);
		PaletteChoice::Forget(chara);
		return;
	}

	++g_players[player].generation;
	LOG("palettes: p%d is wearing '%s' again", player, file);
}

void Undress(int player, int chara)
{
	Player& entry = g_players[player];

	entry.chara = chara;
	entry.tried = false;

	PalettePaint::Clear(player);
	EffectPaint::Clear(player);
	PaletteChoice::NoteBare(player);

	++entry.generation;
}

void Follow(int player)
{
	Player& entry = g_players[player];
	const int chara = PaletteMemory::GetCharaNumber(player);

	if (chara < 0)
	{
		entry.tried = false;
		return;
	}

	if (chara != entry.chara)
		Undress(player, chara);

	if (entry.tried || !PaletteControl::CanEdit(player))
		return;

	entry.tried = true;

	if (PalettePaint::IsStaged(player))
		return;

	Dress(player, chara);
}

}

void PaletteChoice::OnFrame()
{
	for (int player = 0; player < kPlayers; ++player)
		Follow(player);
}
