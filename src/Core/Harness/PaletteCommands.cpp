#include "Core/Harness/PaletteCommands.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Menus/LobbyAvatar.h"
#include "Network/LobbyPalettes.h"
#include "Palette/PaletteOwnerProbe.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSeat.h"
#include "Palette/PaletteSignature.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using Words = std::vector<std::string>;

constexpr int kRgbBase = 16;

bool ParsePlayer(const std::string& word, int& player)
{
	player = atoi(word.c_str());
	return player >= 0 && player < PalettePaint::kPlayers;
}

bool ParseRgb(const std::string& word, uint8_t* rgb)
{
	char* end = nullptr;
	const unsigned long value = strtoul(word.c_str(), &end, kRgbBase);

	if (end == word.c_str() || *end != '\0')
		return false;

	rgb[0] = static_cast<uint8_t>(value >> 16);
	rgb[1] = static_cast<uint8_t>(value >> 8);
	rgb[2] = static_cast<uint8_t>(value);
	return true;
}

void Fill(uint8_t* colours, const uint8_t* rgb)
{
	for (int entry = 0; entry < PalettePaint::kColours; ++entry)
	{
		colours[entry * 4 + 0] = rgb[0];
		colours[entry * 4 + 1] = rgb[1];
		colours[entry * 4 + 2] = rgb[2];
		colours[entry * 4 + 3] = 0xFF;
	}
}

std::string RunProbe(const Words& words)
{
	if (words.size() >= 3 && words[2] == "reset")
		PaletteOwnerProbe::Reset();
	else if (words.size() >= 3)
		PaletteOwnerProbe::SetEnabled(words[2] == "on");

	return PaletteOwnerProbe::IsEnabled() ? "ok probe on" : "ok probe off";
}

std::string RunRows()
{
	std::string reply = "ok";
	char text[160] = {};

	for (int i = 0; i < PaletteOwnerProbe::GetCount(); ++i)
	{
		PaletteOwnerProbe::Row row = {};

		if (!PaletteOwnerProbe::Get(i, row))
			continue;

		sprintf_s(text, " | owner %08x texture %08x override %08x row %d draws %d",
			static_cast<unsigned>(row.owner), static_cast<unsigned>(row.texture),
			static_cast<unsigned>(row.override), row.row, row.draws);
		reply += text;
	}

	return reply;
}

std::string RunState()
{
	std::string reply = "ok frame " + std::to_string(PaletteSeat::GetFrame());
	char text[160] = {};

	for (int i = 0; i < PaletteSeat::GetSeatCount(); ++i)
	{
		PaletteSeat::Seat seat = {};

		if (!PaletteSeat::GetSeat(i, seat))
			continue;

		sprintf_s(text, " | seat owner %08x texture %08x side %d rows %x draws %d last %d",
			static_cast<unsigned>(seat.owner), static_cast<unsigned>(seat.texture), seat.side, seat.rows,
			seat.draws, seat.lastSeenFrame);
		reply += text;
	}

	for (int player = 0; player < PalettePaint::kPlayers; ++player)
	{
		const uintptr_t owner = PalettePaint::GetOwner(player);

		sprintf_s(text, " | p%d owner %08x index %d writes %d companion %d candidates", player,
			static_cast<unsigned>(owner), PalettePaint::GetIndex(player), PalettePaint::GetWrites(player),
			PalettePaint::HasCompanion(player) ? 1 : 0);
		reply += text;

		uintptr_t candidates[PaletteSeat::kCandidates] = {};
		const int count = PaletteSeat::GetCandidates(owner, candidates, PaletteSeat::kCandidates);

		for (int i = 0; i < count; ++i)
		{
			sprintf_s(text, " %08x", static_cast<unsigned>(candidates[i]));
			reply += text;
		}
	}

	return reply;
}

std::string RunFill(const Words& words)
{
	int player = 0;
	uint8_t character[3] = {};
	uint8_t doppel[3] = {};

	if (words.size() < 5 || !ParsePlayer(words[2], player) || !ParseRgb(words[3], character)
		|| !ParseRgb(words[4], doppel))
	{
		return "error palette fill <player> <rrggbb> <rrggbb>";
	}

	uint8_t colours[PalettePaint::kBytes] = {};

	Fill(colours, doppel);
	PalettePaint::StageCompanion(player, colours);

	Fill(colours, character);
	PalettePaint::Stage(player, colours);

	return "ok";
}

std::string RunLobbyFake(const Words& words)
{
	uint8_t rgb[3] = {};
	uint8_t shown[PalettePaint::kBytes] = {};
	LobbyPaletteCodec::Entry entry = {};

	if (words.size() < 4 || !ParseRgb(words[3], rgb))
		return "error palette lobby fake <rrggbb>";

	if (!PalettePaint::ReadGameColours(0, shown))
		return "error no avatar colours to sign";

	PaletteSignature::Take(shown, entry.signature);
	Fill(entry.rgba, rgb);

	LobbyPalettes::InjectForTest(entry);
	return "ok";
}

std::string RunLobby(const Words& words)
{
	const std::string action = words.size() >= 3 ? words[2] : std::string();

	if (action == "fake")
		return RunLobbyFake(words);

	if (action == "clear")
	{
		LobbyPalettes::ClearTest();
		return "ok";
	}

	return "ok members " + std::to_string(LobbyPalettes::Count()) + " | avatar wears " + LobbyAvatar::StatusText();
}

std::string RunLife(const Words& words)
{
	int player = 0;

	if (words.size() < 4 || !ParsePlayer(words[2], player))
		return "error palette life <player> <hp>";

	if (!GameState::AllowsTrainingTools())
		return "error only in offline training";

	void* const chara = MemoryMap::GetCharaSlot(player);

	if (chara == nullptr)
		return "error no character in that slot";

	const uint32_t value = static_cast<uint32_t>(atoi(words[3].c_str()));
	void* const field = static_cast<uint8_t*>(chara) + GameOffsets::kPlayerDataHp;

	return TryWriteDword(field, value) ? "ok" : "error the hp could not be written";
}

}

bool PaletteCommands::Execute(const Words& words, std::string& reply)
{
	if (words.empty() || words[0] != "palette")
		return false;

	const std::string verb = words.size() >= 2 ? words[1] : std::string();

	if (verb == "probe")
		reply = RunProbe(words);
	else if (verb == "rows")
		reply = RunRows();
	else if (verb == "state")
		reply = RunState();
	else if (verb == "fill")
		reply = RunFill(words);
	else if (verb == "life")
		reply = RunLife(words);
	else if (verb == "lobby")
		reply = RunLobby(words);
	else
		reply = "error palette probe|rows|state|fill|life|lobby";

	return true;
}
