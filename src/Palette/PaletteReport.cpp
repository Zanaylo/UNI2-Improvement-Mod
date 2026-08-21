#include "Palette/PaletteReport.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"
#include "Network/PaletteShare.h"
#include "Network/SteamNetwork.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteBinder.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteIdentity.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTexture.h"
#include "Palette/PaletteTrace.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr int kColoursSampled = 8;
constexpr int kSlotsSampled = 4;
constexpr int kSeatWindowDwords = 12;
constexpr uintptr_t kSeatWindowStart = 0x597930;

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
	case PaletteTexture::Claim::Guess: return "guess";
	case PaletteTexture::Claim::Strong: return "strong";
	default: return "other";
	}
}

void WriteColours(FILE* file, const uint8_t* rgba)
{
	for (int i = 0; i < kColoursSampled; ++i)
		fprintf(file, " %02x%02x%02x", rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2]);

	fprintf(file, "\n");
}

const char* AgreementWithGuess(int index, int said)
{
	const PaletteTexture::TextureOwner owner = PaletteTexture::GetOwnerInfo(index);
	if (owner.kind != PaletteTexture::OwnerKind::Player)
		return "";

	if (said < 0)
		return ", naming nobody";

	const int side = (owner.players & PaletteTexture::PlayerBit(0)) != 0 ? 0 : 1;

	return said == PaletteManager::GetCharaNumber(side) ? ", agreeing with the guess"
		: ", DISAGREEING with the guess";
}

int EntriesDiffering(const uint8_t* wanted, int texture, int row)
{
	if (wanted == nullptr || texture < 0 || row < 0)
		return -1;

	uint8_t colours[PaletteTexture::kRowBytes] = {};
	if (!PaletteTexture::ReadRowAsRgba(texture, static_cast<unsigned>(row), colours))
		return -1;

	int differing = 0;

	for (int c = 0; c < PaletteFile::kColors; ++c)
	{
		if (colours[c * 4 + 0] == wanted[c * 4 + 0] && colours[c * 4 + 1] == wanted[c * 4 + 1] &&
			colours[c * 4 + 2] == wanted[c * 4 + 2])
		{
			continue;
		}

		++differing;
	}

	return differing;
}

void WriteWearing(FILE* file, const char* label, const uint8_t* wanted, int texture, int row)
{
	if (wanted == nullptr)
	{
		fprintf(file, "  %-12s nothing to compare\n", label);
		return;
	}

	const int differing = EntriesDiffering(wanted, texture, row);

	if (differing < 0)
	{
		fprintf(file, "  %-12s the row could not be read\n", label);
		return;
	}

	fprintf(file, "  %-12s %s, %d of %d entries differ\n", label,
		differing == 0 ? "the texture is wearing it" : "THE TEXTURE IS NOT WEARING IT", differing,
		PaletteFile::kColors);
}

void WriteRowSample(FILE* file, const char* label, int texture, int row, bool pristine)
{
	if (texture < 0 || row < 0)
	{
		fprintf(file, "    %-9s no texture or row\n", label);
		return;
	}

	uint8_t colours[PaletteTexture::kRowBytes] = {};

	const bool ok = pristine ? PaletteTexture::ReadPristineRowAsRgba(texture, row, colours)
		: PaletteTexture::ReadRowAsRgba(texture, row, colours);

	if (!ok)
	{
		fprintf(file, "    %-9s unreadable\n", label);
		return;
	}

	fprintf(file, "    %-9s", label);
	WriteColours(file, colours);
}

void WriteSeatSignals(FILE* file)
{
	fprintf(file, "\n=== who the mod thinks it is ===\n");
	fprintf(file, "  PaletteShare::GetOwnSide()  %d\n", PaletteShare::GetOwnSide());

	uint32_t raw = 0;
	const bool read = TryReadDword(
		reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kPlayerSideIndex)), raw);

	if (read)
		fprintf(file, "  [0x%06x] side index        %u\n",
			static_cast<unsigned>(GameOffsets::kPlayerSideIndex), raw);
	else
		fprintf(file, "  [0x%06x] side index        unreadable\n",
			static_cast<unsigned>(GameOffsets::kPlayerSideIndex));

	fprintf(file, "  rows swapped                %d\n", PaletteTexture::GetRowsSwapped() ? 1 : 0);
	fprintf(file, "  swap sides                  %d\n", PaletteTexture::GetSwapSides() ? 1 : 0);

	fprintf(file, "  online                      %d (blind %d) - %s\n",
		OnlineState::IsOnline() ? 1 : 0, OnlineState::IsBlind() ? 1 : 0,
		OnlineState::GetStatusText());

	fprintf(file, "  steam ready %d, sees traffic %d, has peer %d, peer 0x%016llx\n",
		SteamNetwork::IsReady() ? 1 : 0, SteamNetwork::CanSeePeerTraffic() ? 1 : 0,
		SteamNetwork::HasPeer() ? 1 : 0,
		static_cast<unsigned long long>(SteamNetwork::GetPeer()));

	fprintf(file, "  share status                %s\n", PaletteShare::GetStatusText());

	fprintf(file, "\n  dwords around the side index, for comparing host against client:\n");

	for (int i = 0; i < kSeatWindowDwords; ++i)
	{
		const uintptr_t rva = kSeatWindowStart + static_cast<uintptr_t>(i) * 4;

		const char* const mark = rva == GameOffsets::kPlayerSideIndex
			? "   <- the one the mod uses" : "";

		uint32_t value = 0;

		if (TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value))
			fprintf(file, "    [0x%06x] 0x%08x %11u%s\n", static_cast<unsigned>(rva), value, value,
				mark);
		else
			fprintf(file, "    [0x%06x] unreadable%s\n", static_cast<unsigned>(rva), mark);
	}
}

void WriteIdentity(FILE* file)
{
	fprintf(file, "\n=== what the colours say the sides are ===\n");

	fprintf(file, "  IdentifyByColours %d, so the verdict below %s\n",
		g_modVals.paletteIdentifyByColours ? 1 : 0,
		g_modVals.paletteIdentifyByColours ? "is what the mod acts on" : "is advice only");

	fprintf(file, "  bind-order guess %s\n",
		PaletteTexture::IsHeuristicEnabled() ? "still deciding" : "handed over");

	fprintf(file, "  colours %s, %d comparisons made\n",
		PaletteIdentity::ColoursEverMatched() ? "have matched" : "have never matched",
		PaletteIdentity::GetComparisons());

	for (int player = 0; player < 2; ++player)
	{
		fprintf(file, "  reference P%d %-10s %d of %d palettes read\n", player + 1,
			PaletteManager::GetCharaName(PaletteManager::GetCharaNumber(player)),
			PaletteIdentity::GetCatalogueCount(player), GameOffsets::kPaletteSlots);
	}

	fprintf(file, "  chara stack live for %d of %d rows set\n", PaletteDrawProbe::GetCharaHits(),
		PaletteDrawProbe::GetRowSets());
}

void WriteLoads(FILE* file)
{
	const int count = PaletteMemory::GetLoadCount();

	fprintf(file, "\n=== what the game loaded ===\n");
	fprintf(file, "  loads %d, loader calls %d, P1's load %d, P2's load %d\n", count,
		PaletteMemory::GetLoaderCallCount(), PaletteMemory::FindLoadForPlayer(0),
		PaletteMemory::FindLoadForPlayer(1));

	for (int i = 0; i < count; ++i)
	{
		fprintf(file, "  [%2d] %-28s chara %d kind %d frame %d palettes %d\n", i,
			PaletteMemory::GetLoadName(i), PaletteMemory::GetLoadChara(i),
			PaletteMemory::GetLoadKind(i), PaletteMemory::GetLoadFrame(i),
			PaletteMemory::GetLoadPaletteCount(i));

		uint8_t palette[PaletteMemory::kPaletteBytes] = {};
		if (!PaletteMemory::ReadPalette(i, 0, palette))
		{
			fprintf(file, "       palette 0 unreadable\n");
			continue;
		}

		fprintf(file, "       palette 0");
		WriteColours(file, palette);
	}
}

void WriteSide(FILE* file, int player)
{
	const int chara = PaletteManager::GetCharaNumber(player);
	const int texture = PaletteTexture::FindForPlayer(player);
	const int row = PaletteTexture::GetRowForPlayer(player);
	const int applied = PaletteManager::GetApplied(player);

	fprintf(file, "\n=== P%d ===\n", player + 1);
	fprintf(file, "  character   %d %s (palette memory says %d)\n", chara,
		PaletteManager::GetCharaName(chara), PaletteMemory::GetCharaNumber(player));
	fprintf(file, "  game slot   %d\n", PaletteMemory::GetPlayerSlot(player));
	fprintf(file, "  texture     %d, row %d\n", texture, row);
	fprintf(file, "  applied     %d %s%s\n", applied,
		applied >= 0 ? PaletteManager::GetAppliedName(player) : "(none)",
		PaletteManager::IsHandEdited(player) ? " (hand edited)" : "");

	fprintf(file, "  foreign     %s\n", PaletteManager::HasForeign(player)
		? PaletteManager::GetForeignName(player) : "(none)");

	fprintf(file, "  effects     %d entries edited\n", EffectPaint::GetEditedCount(player));

	fprintf(file, "  palettes on disk for this character: %d\n", PaletteManager::GetCount(player));

	WriteWearing(file, "ours:", PaletteManager::GetAppliedColors(player), texture, row);
	WriteWearing(file, "theirs:", PaletteManager::GetForeignColors(player), texture, row);

	WriteRowSample(file, "worn", texture, row, false);
	WriteRowSample(file, "pristine", texture, row, true);

	uint8_t game[PaletteMemory::kPaletteBytes] = {};
	if (PaletteMemory::ReadPlayerPalette(player, game))
	{
		fprintf(file, "    %-9s", "in memory");
		WriteColours(file, game);
	}
	else
	{
		fprintf(file, "    %-9s unreadable\n", "in memory");
	}

	fprintf(file, "  palette address 0x%08x, table 0x%08x, current 0x%08x\n",
		static_cast<unsigned>(PaletteMemory::GetPlayerPaletteAddress(player)),
		static_cast<unsigned>(PaletteMemory::GetPlayerPaletteTable(player)),
		static_cast<unsigned>(PaletteMemory::GetPlayerCurrentPalette(player)));

	for (int slot = 0; slot < kSlotsSampled; ++slot)
	{
		uint8_t entry[PaletteMemory::kPaletteBytes] = {};

		if (!PaletteMemory::ReadPlayerPaletteAt(player, slot, entry))
		{
			fprintf(file, "    slot %-4d unreadable\n", slot);
			continue;
		}

		fprintf(file, "    slot %-4d", slot);
		WriteColours(file, entry);
	}
}

void WriteTextures(FILE* file)
{
	fprintf(file, "\n=== palette textures the mod is tracking ===\n");
	fprintf(file, "  generation %d, seen %d, frame serial %d\n", PaletteTexture::GetGeneration(),
		PaletteTexture::GetSeenCount(), PaletteTexture::GetFrameSerial());

	fprintf(file, "  binder authoritative %d, observed P1 %d, P2 %d\n",
		PaletteBinder::IsAuthoritative() ? 1 : 0, PaletteBinder::GetObservedTexture(0),
		PaletteBinder::GetObservedTexture(1));

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		const PaletteTexture::TextureOwner owner = PaletteTexture::GetOwnerInfo(i);

		fprintf(file, "  [%2d] 0x%08x %-10s players 0x%x chara %d %-6s%s binds %d frames %d "
			"age %d%s\n",
			i, static_cast<unsigned>(PaletteTexture::GetSeen(i)), OwnerKindName(owner.kind),
			owner.players, owner.chara, ClaimName(PaletteTexture::GetClaim(i)),
			PaletteTexture::IsOwnerByHand(i) ? " byhand" : "",
			PaletteTexture::GetBindCount(i), PaletteTexture::GetFrameCount(i),
			PaletteTexture::GetLastBoundAge(i),
			PaletteTexture::WasBoundInMatch(i) ? " inmatch" : "");

		const int said = PaletteIdentity::GetChara(i);

		fprintf(file, "       first seen frame %d, colours say %s (%s)%s\n",
			PaletteTexture::GetFirstSeenFrame(i), PaletteManager::GetCharaName(said),
			PaletteIdentity::GetSourceName(PaletteIdentity::GetSource(i)),
			AgreementWithGuess(i, said));

		unsigned votes[2] = {};
		PaletteDrawProbe::GetSeatVotes(PaletteTexture::GetSeen(i), votes[0], votes[1]);

		fprintf(file, "       drawn under P1 %u times, under P2 %u times\n", votes[0], votes[1]);

		for (unsigned row = 0; row < 2; ++row)
		{
			uint8_t colours[PaletteTexture::kRowBytes] = {};

			if (!PaletteTexture::ReadRowAsRgba(i, row, colours))
			{
				fprintf(file, "       row %u unreadable\n", row);
				continue;
			}

			fprintf(file, "       row %u %-9s", row,
				PaletteTexture::HasBackup(i, row) ? "painted" : "untouched");

			WriteColours(file, colours);

			if (!PaletteTexture::HasBackup(i, row))
				continue;

			uint8_t pristine[PaletteTexture::kRowBytes] = {};
			if (!PaletteTexture::ReadPristineRowAsRgba(i, row, pristine))
				continue;

			fprintf(file, "             pristine ");
			WriteColours(file, pristine);
		}
	}
}

void WriteTuples(FILE* file)
{
	const int count = PaletteDrawProbe::GetTupleCount();

	fprintf(file, "\n=== what the renderer was seen drawing ===\n");
	fprintf(file, "  capturing %d, tuples %d, calls/frame %d, draws/frame %d\n",
		PaletteDrawProbe::IsCapturing() ? 1 : 0, count,
		PaletteDrawProbe::GetCallsLastFrame(), PaletteDrawProbe::GetDrawsLastFrame());

	fprintf(file, "  row sets %d, chara hits %d, tuples last frame %d\n",
		PaletteDrawProbe::GetRowSets(), PaletteDrawProbe::GetCharaHits(),
		PaletteDrawProbe::GetTuplesLastFrame());

	fprintf(file, "  chara slots the game keeps: P1 0x%08x, P2 0x%08x\n",
		static_cast<unsigned>(reinterpret_cast<uintptr_t>(MemoryMap::GetCharaSlot(0))),
		static_cast<unsigned>(reinterpret_cast<uintptr_t>(MemoryMap::GetCharaSlot(1))));

	for (int i = 0; i < count; ++i)
	{
		PaletteDrawProbe::Tuple tuple = {};
		if (!PaletteDrawProbe::GetTuple(i, tuple))
			continue;

		fprintf(file, "  [%2d] texture 0x%08x stage %d row %d %-4s chara 0x%08x count %d "
			"frames %d age %d\n",
			i, static_cast<unsigned>(tuple.texture), tuple.stage, tuple.row,
			tuple.mainPath ? "main" : "ex", static_cast<unsigned>(tuple.chara), tuple.count,
			tuple.frames, tuple.lastAgeFrames);
	}
}

void WriteSharing(FILE* file)
{
	PaletteShare::Diagnostics share = {};
	PaletteShare::GetDiagnostics(share);

	fprintf(file, "\n=== what crossed the wire ===\n");
	fprintf(file, "  our seat %d, %d frames into the match, peer 0x%016llx\n", share.ownSide,
		share.framesInMatch, static_cast<unsigned long long>(share.matchPeer));

	fprintf(file, "  sends %d of %d used, next at frame %d, %d packets out, %d in\n",
		share.sendsDone, share.sendsAllowed, share.nextSendFrame, share.sent, share.received);

	fprintf(file, "  last sent revision %d, %d send(s) started by a change\n",
		share.lastSentChoice, share.resends);

	fprintf(file, "  ShowOnlinePalettes %d, Steam attempts %d\n",
		g_modVals.showOnlinePalettes ? 1 : 0, share.steamAttempts);

	for (int side = 0; side < 2; ++side)
	{
		if (!share.pendingValid[side])
		{
			fprintf(file, "  p%d has nothing waiting\n", side + 1);
			continue;
		}

		fprintf(file, "  p%d holds '%s' for chara %d, %s\n", side + 1, share.pendingName[side],
			share.pendingChara[side],
			share.pendingApplied[side] ? "applied" : "STILL WAITING to be applied");
	}
}

void WriteTrace(FILE* file)
{
	const int count = PaletteTrace::GetCount();
	const int dropped = PaletteTrace::GetDropped();

	fprintf(file, "\n=== what happened, oldest first ===\n");

	if (dropped > 0)
		fprintf(file, "  %d earlier lines fell off the ring\n", dropped);

	for (int i = 0; i < count; ++i)
		fprintf(file, "  %7d  %s\n", PaletteTrace::GetFrame(i), PaletteTrace::GetText(i));
}

void WriteLibrary(FILE* file)
{
	fprintf(file, "\n=== palettes this player has ===\n");

	for (int player = 0; player < 2; ++player)
	{
		const int count = PaletteManager::GetCount(player);
		fprintf(file, "  P%d %s: %d\n", player + 1,
			PaletteManager::GetCharaName(PaletteManager::GetCharaNumber(player)), count);

		for (int i = 0; i < count; ++i)
		{
			fprintf(file, "    [%2d] %-32s by %s\n", i, PaletteManager::GetName(player, i),
				PaletteManager::GetCreator(player, i));
		}
	}
}

}

bool PaletteReport::Write(std::string& outPath)
{
	SYSTEMTIME now = {};
	GetLocalTime(&now);

	char name[80] = {};
	sprintf_s(name, "UNI2_IM_palettes_%04d%02d%02d_%02d%02d%02d.txt", now.wYear, now.wMonth,
		now.wDay, now.wHour, now.wMinute, now.wSecond);

	outPath = GetModLogPath(name);

	FILE* file = nullptr;
	if (fopen_s(&file, outPath.c_str(), "w") != 0 || file == nullptr)
		return false;

	fprintf(file, "%s %s palette report\n", UNI2_IM_NAME, UNI2_IM_VERSION);
	fprintf(file, "%04d-%02d-%02d %02d:%02d:%02d\n", now.wYear, now.wMonth, now.wDay, now.wHour,
		now.wMinute, now.wSecond);
	fprintf(file, "in match %d, battle mode %d, sub mode %d\n", GameState::IsInMatch() ? 1 : 0,
		GameState::GetBattleMode(), GameState::GetTrainingFlag());

	WriteSeatSignals(file);
	WriteIdentity(file);

	for (int player = 0; player < 2; ++player)
		WriteSide(file, player);

	WriteSharing(file);
	WriteTextures(file);
	WriteTuples(file);
	WriteLoads(file);
	WriteLibrary(file);
	WriteTrace(file);

	fclose(file);
	return true;
}
