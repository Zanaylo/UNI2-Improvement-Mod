#include "Palette/PaletteChoice.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/Config/Settings.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Menus/ColourPicker.h"
#include "Game/Tables/PartColourTable.h"
#include "Palette/ColourSlots.h"
#include "Palette/EffectPaint.h"
#include "Palette/NetworkPick.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteLibrary.h"
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
	bool dressedHere = false;
	unsigned generation = 0;
	unsigned pick = 0;
	bool fromSlot = false;
	bool inBattle = false;
	bool freshMatch = false;
	char worn[PaletteFile::kNameLength + 8] = {};
};

Player g_players[PaletteChoice::kPlayers] = {};

int WornIndex(int player, int chara)
{
	const char* const worn = PaletteChoice::WornFile(player);

	if (worn[0] == '\0')
		return -1;

	for (int i = 0; i < PaletteLibrary::GetCount(chara); ++i)
	{
		if (strcmp(PaletteLibrary::GetName(chara, i), worn) == 0)
			return i;
	}

	return -1;
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

	if (!PaletteFile::Load(PaletteLibrary::PathOf(chara, file), colours, info, effects, &hasEffects))
		return false;

	PalettePaint::Stage(player, colours);

	uint8_t theirs[PaletteFile::kBytes] = {};
	PaletteFile::Info theirInfo = {};

	if (PaletteFile::Load(PaletteLibrary::PathOf(chara, PaletteFile::CompanionOf(file).c_str()), theirs, theirInfo))
		PalettePaint::StageCompanion(player, theirs);
	else
		PalettePaint::ClearCompanion(player);

	if (!hasEffects)
		hasEffects = PartColourTable::BuildAutoEffectBlock(chara, colours, effects);

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

int PaletteChoice::LocalPlayer()
{
	const int local = PaletteControl::LocalPlayer();

	return local >= 0 ? local : 0;
}

bool PaletteChoice::Step(int player, int steps)
{
	if (player < 0 || player >= kPlayers || steps == 0 || !PaletteControl::CanEdit(player))
		return false;

	const int chara = PaletteMemory::GetCharaNumber(player);

	if (chara < 0)
		return false;

	const int count = PaletteLibrary::GetCount(chara);

	if (count <= 0)
		return false;

	const int slots = count + 1;
	int target = (WornIndex(player, chara) + 1 + steps) % slots;

	if (target < 0)
		target += slots;

	if (target == 0)
	{
		Bare(player);
		return true;
	}

	return Wear(player, PaletteLibrary::GetName(chara, target - 1));
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
	g_players[player].fromSlot = false;
}

void PaletteChoice::NoteBare(int player)
{
	if (player < 0 || player >= kPlayers)
		return;

	g_players[player].worn[0] = '\0';
	g_players[player].fromSlot = false;
}

unsigned PaletteChoice::GetGeneration(int player)
{
	return player >= 0 && player < kPlayers ? g_players[player].generation : 0;
}

namespace {

bool Put(int player, int chara, const char* file)
{
	if (!PaletteChoice::Apply(player, chara, file))
		return false;

	++g_players[player].generation;
	g_players[player].dressedHere = true;
	return true;
}

void Dress(int player, int chara)
{
	char file[sizeof(g_remembered)] = {};
	strncpy_s(file, PaletteChoice::Remembered(chara), _TRUNCATE);

	if (file[0] == 0)
		return;

	if (!Put(player, chara, file))
	{
		LOG("palettes: p%d's remembered '%s' could not be read", player, file);
		PaletteChoice::Forget(chara);
		return;
	}

	LOG("palettes: p%d is wearing '%s' again", player, file);
}

std::string SlotFile(int player, int chara)
{
	const int colour = PaletteMemory::GetSelectColour(player);

	if (PaletteControl::IsOnline())
		return NetworkPick::FileFor(chara, colour);

	const int extended = ColourPicker::ExtendedFor(player, chara);

	if (extended != ColourSlots::kNoExtended)
		return ColourSlots::ExtendedFile(chara, extended);

	return ColourSlots::BoundFile(chara, colour);
}

void DressFromSlot(int player, int chara, const std::string& file)
{
	if (PalettePaint::IsStaged(player) && file == PaletteChoice::WornFile(player))
	{
		g_players[player].fromSlot = true;
		return;
	}

	if (!Put(player, chara, file.c_str()))
	{
		LOG("palettes: p%d's colour slot holds '%s', which could not be read", player, file.c_str());
		return;
	}

	g_players[player].fromSlot = true;
	LOG("palettes: p%d is wearing '%s' from its colour slot", player, file.c_str());
}

void TakeOff(int player)
{
	PalettePaint::Clear(player);
	EffectPaint::Clear(player);
	PaletteChoice::NoteBare(player);

	++g_players[player].generation;
}

void WearThePick(int player)
{
	if (!PalettePaint::IsStaged(player))
		return;

	TakeOff(player);
	LOG("palettes: p%d starts in the game colour picked at character select", player);
}

void Undress(int player, int chara)
{
	Player& entry = g_players[player];

	entry.chara = chara;
	entry.tried = false;
	entry.dressedHere = false;

	TakeOff(player);
}

void WatchBattle()
{
	const bool inBattle = SceneWatch::Current() == GameOffsets::kSceneBattle;

	for (Player& entry : g_players)
	{
		if (inBattle && !entry.inBattle)
			entry.freshMatch = true;

		entry.inBattle = inBattle;
	}
}

void StartFromThePick(Player& entry)
{
	const unsigned picks = ColourPicker::Picks();

	if (!entry.inBattle || (entry.pick == picks && !entry.freshMatch))
		return;

	entry.pick = picks;
	entry.freshMatch = false;
	entry.tried = false;
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

	StartFromThePick(entry);

	const bool mine = PaletteControl::CanEdit(player);

	if (entry.dressedHere && !mine)
	{
		LOG("palettes: p%d turned out not to be ours, so what this machine put on came off", player);
		Undress(player, chara);
		return;
	}

	if (entry.tried || !mine)
		return;

	entry.tried = true;

	const std::string slotted = SlotFile(player, chara);

	if (!slotted.empty())
	{
		DressFromSlot(player, chara, slotted);
		return;
	}

	if (g_modVals.paletteExtendedSlots)
	{
		WearThePick(player);
		return;
	}

	if (entry.fromSlot)
		TakeOff(player);

	if (PalettePaint::IsStaged(player))
		return;

	Dress(player, chara);
}

bool DistinctSides()
{
	void* const first = MemoryMap::GetCharaSlot(0);
	void* const second = MemoryMap::GetCharaSlot(1);

	if (first == nullptr || second == nullptr)
		return false;

	if (first == second)
		return false;

	uint32_t sideA = 0;
	uint32_t sideB = 0;

	if (!MemoryMap::ReadStructDword(first, GameOffsets::kCharaSideIndex, sideA) ||
		!MemoryMap::ReadStructDword(second, GameOffsets::kCharaSideIndex, sideB))
	{
		return false;
	}

	return (sideA & 0xff) != (sideB & 0xff);
}

}

void PaletteChoice::OnFrame()
{
	static bool s_undecided = false;

	WatchBattle();

	if (!DistinctSides())
	{
		if (!s_undecided)
			LOG("palettes: the two player slots have not settled into distinct sides yet, skipping");

		s_undecided = true;
		return;
	}

	s_undecided = false;

	for (int player = 0; player < kPlayers; ++player)
		Follow(player);
}
