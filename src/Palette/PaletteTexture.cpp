#include "Palette/PaletteTexture.h"

#include "Core/logger.h"
#include "D3D9/DeviceHooks.h"
#include "Game/GameOffsets.h"
#include "Core/utils.h"
#include "Training/FrameStepper.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTrace.h"

#include <d3d9.h>

#include <cstring>

namespace {

constexpr int kMaxSeen = PaletteTexture::kMaxSeen;

struct Backup
{
	uint8_t colors[PaletteTexture::kRowBytes];
	bool taken;
};

IDirect3DTexture9* g_seen[kMaxSeen] = {};
PaletteTexture::TextureOwner g_ownerOf[kMaxSeen] = {};
PaletteTexture::Claim g_claim[kMaxSeen] = {};
bool g_ownerByHand[kMaxSeen] = {};
int g_binds[kMaxSeen] = {};

bool g_boundInMatch[kMaxSeen] = {};

int g_lastBound[kMaxSeen] = {};
int g_firstSeenAt[kMaxSeen] = {};
bool g_volatilePool[kMaxSeen] = {};
bool g_bornInMatch[kMaxSeen] = {};
int g_frameSerial = 0;

constexpr int kInUseFrames = 8;

constexpr int kMatchSettleFrames = 30;
bool g_matchOn = false;
int g_matchSettling = 0;

constexpr int kMaxEvicted = kMaxSeen;
uintptr_t g_evicted[kMaxEvicted] = {};
int g_evictedAt = 0;

void RemoveAt(int index, const char* why);

bool IsCharacterOwner(const PaletteTexture::TextureOwner& owner)
{
	return owner.kind == PaletteTexture::OwnerKind::Player ||
		owner.kind == PaletteTexture::OwnerKind::CharSelect ||
		owner.kind == PaletteTexture::OwnerKind::LobbyAvatar;
}

bool SameOwner(const PaletteTexture::TextureOwner& a, const PaletteTexture::TextureOwner& b)
{
	if (a.kind != b.kind)
		return false;

	switch (a.kind)
	{
	case PaletteTexture::OwnerKind::Player:
		return a.players == b.players;
	case PaletteTexture::OwnerKind::CharSelect:
	case PaletteTexture::OwnerKind::LobbyAvatar:
		return a.chara == b.chara;
	default:
		return true;
	}
}

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

int g_frameOrder[kMaxSeen] = {};
int g_frameCount = 0;
int g_firstSeen[kMaxSeen] = {};
int g_secondSeen[kMaxSeen] = {};
int g_framesWatched = 0;

int g_framesBound[kMaxSeen] = {};
Backup g_backup[kMaxSeen][PaletteTexture::kRows] = {};
int g_seenCount = 0;
int g_generation = 0;

bool g_swapSides = false;

bool g_heuristicEnabled = true;

bool g_rowsSwapped = false;
int g_rowGeneration = 0;

struct Shape
{
	unsigned width;
	unsigned height;
	unsigned format;
	int count;
};

constexpr int kMaxShapes = 48;
Shape g_shapes[kMaxShapes] = {};
int g_shapeCount = 0;

void RememberShape(const D3DSURFACE_DESC& desc)
{
	for (int i = 0; i < g_shapeCount; ++i)
	{
		if (g_shapes[i].width == desc.Width && g_shapes[i].height == desc.Height &&
			g_shapes[i].format == static_cast<unsigned>(desc.Format))
		{
			++g_shapes[i].count;
			return;
		}
	}

	if (g_shapeCount >= kMaxShapes)
		return;

	g_shapes[g_shapeCount].width = desc.Width;
	g_shapes[g_shapeCount].height = desc.Height;
	g_shapes[g_shapeCount].format = static_cast<unsigned>(desc.Format);
	g_shapes[g_shapeCount].count = 1;
	++g_shapeCount;

	LOG("texture shape: %ux%u format %u", desc.Width, desc.Height,
		static_cast<unsigned>(desc.Format));
}

constexpr int kRejectedSlots = 256;
IDirect3DTexture9* g_rejected[kRejectedSlots] = {};

IDirect3DTexture9*& RejectedSlot(IDirect3DTexture9* texture)
{
	const uintptr_t value = reinterpret_cast<uintptr_t>(texture);
	return g_rejected[(value >> 4) & (kRejectedSlots - 1)];
}

void ForgetRejected()
{
	memset(g_rejected, 0, sizeof(g_rejected));
}

bool IsPaletteShaped(IDirect3DTexture9* texture, bool& outVolatile)
{
	outVolatile = false;

	IDirect3DTexture9*& rejected = RejectedSlot(texture);
	if (rejected == texture)
		return false;

	D3DSURFACE_DESC desc = {};
	if (FAILED(texture->GetLevelDesc(0, &desc)))
		return false;

	RememberShape(desc);

	if (desc.Width == PaletteTexture::kWidth && desc.Height == PaletteTexture::kRows &&
		desc.Format == D3DFMT_A8R8G8B8)
	{

		outVolatile = desc.Pool == D3DPOOL_DEFAULT;
		return true;
	}

	rejected = texture;
	return false;
}

IDirect3DTexture9* Live(int index)
{
	if (index < 0 || index >= g_seenCount)
		return nullptr;

	return g_seen[index];
}

int FreeSlot()
{
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_seen[i] == nullptr)
			return i;
	}

	return g_seenCount < kMaxSeen ? g_seenCount++ : -1;
}

void MakeRoom()
{
	for (int i = g_seenCount - 1; i >= 0; --i)
	{
		if (g_seen[i] == nullptr || g_frameSerial - g_lastBound[i] <= kInUseFrames)
			continue;

		PaletteTexture::RestoreAll(i);
		RemoveAt(i, "the list was full");
	}
}

void WatchMatch(bool inMatch)
{
	if (inMatch == g_matchOn)
	{
		g_matchSettling = 0;
		return;
	}

	if (++g_matchSettling < kMatchSettleFrames)
		return;

	g_matchSettling = 0;
	g_matchOn = inMatch;

	if (inMatch)
	{
		PaletteTexture::ResetMatchEvidence();
		return;
	}

	PaletteTexture::ForgetMatchTextures();
}

int TakeSlot()
{
	const int free = FreeSlot();

	if (free >= 0)
		return free;

	MakeRoom();

	return FreeSlot();
}

void Remember(IDirect3DTexture9* texture, bool volatilePool)
{
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_seen[i] == texture)
		{
			++g_binds[i];
			g_lastBound[i] = g_frameSerial;

			bool already = false;
			for (int k = 0; k < g_frameCount; ++k)
				already |= g_frameOrder[k] == i;

			if (!already && g_frameCount < kMaxSeen)
				g_frameOrder[g_frameCount++] = i;

			return;
		}
	}

	for (int i = 0; i < kMaxEvicted; ++i)
	{
		if (g_evicted[i] == reinterpret_cast<uintptr_t>(texture))
			return;
	}

	const int slot = TakeSlot();

	if (slot < 0)
		return;

	g_ownerOf[slot] = {};
	g_claim[slot] = PaletteTexture::Claim::Guess;
	g_ownerByHand[slot] = false;
	g_firstSeen[slot] = 0;
	g_secondSeen[slot] = 0;
	g_framesBound[slot] = 0;
	g_binds[slot] = 1;
	g_lastBound[slot] = g_frameSerial;
	g_firstSeenAt[slot] = g_frameSerial;
	g_volatilePool[slot] = volatilePool;
	g_bornInMatch[slot] = GameState::IsInMatch();
	g_boundInMatch[slot] = g_bornInMatch[slot];
	memset(g_backup[slot], 0, sizeof(g_backup[slot]));

	texture->AddRef();

	if (g_frameCount < kMaxSeen)
		g_frameOrder[g_frameCount++] = slot;

	g_seen[slot] = texture;

	LOG("palette texture: 0x%p bound (slot %d of %d, %s)", texture, slot, g_seenCount,
		volatilePool ? "lost on a device reset" : "survives a device reset");
}

}

bool PaletteTexture::OnSetTexture(IDirect3DDevice9*, unsigned, IDirect3DBaseTexture9* texture)
{
	if (texture == nullptr)
		return false;

	if (texture->GetType() != D3DRTYPE_TEXTURE)
		return false;

	IDirect3DTexture9* const flat = static_cast<IDirect3DTexture9*>(texture);

	bool volatilePool = false;
	if (!IsPaletteShaped(flat, volatilePool))
		return false;

	Remember(flat, volatilePool);
	return true;
}

namespace {

constexpr int kFramesBeforeCounting = 30;

bool FindCharacterTextures(int& outFirst, int& outSecond)
{
	if (g_framesWatched < kFramesBeforeCounting || g_seenCount < 2)
		return false;

	int best = -1;
	int second = -1;

	for (int i = 0; i < g_seenCount; ++i)
	{

		if (!g_boundInMatch[i])
			continue;

		if (best < 0 || g_framesBound[i] > g_framesBound[best])
		{
			second = best;
			best = i;
		}
		else if (second < 0 || g_framesBound[i] > g_framesBound[second])
		{
			second = i;
		}
	}

	if (best < 0 || second < 0 || g_framesBound[second] == 0)
		return false;

	outFirst = best < second ? best : second;
	outSecond = best < second ? second : best;
	return true;
}

int FindStrongPlayer(int player)
{
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_claim[i] > PaletteTexture::Claim::Guess &&
			g_ownerOf[i].kind == PaletteTexture::OwnerKind::Player &&
			(g_ownerOf[i].players & PaletteTexture::PlayerBit(player)) != 0)
		{
			return i;
		}
	}

	return -1;
}

void RefreshOwners()
{
	int charaFirst = -1;
	int charaSecond = -1;
	const bool counted = FindCharacterTextures(charaFirst, charaSecond);

	int arrivedFirst = -1;
	int arrivedSecond = -1;
	int candidates = 0;
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (!g_boundInMatch[i])
			continue;

		++candidates;

		if (arrivedFirst < 0)
			arrivedFirst = i;
		else if (arrivedSecond < 0)
			arrivedSecond = i;
	}

	static int loggedCandidates = -1;
	if (candidates != loggedCandidates)
	{
		loggedCandidates = candidates;
		LOG("palette guess: %d of %d textures are the match's, counting %s", candidates, g_seenCount,
			counted ? "on" : "not yet");
	}

	for (int i = 0; i < g_seenCount; ++i)
	{

		if (g_claim[i] > PaletteTexture::Claim::Guess)
			continue;

		if (g_ownerOf[i].kind == PaletteTexture::OwnerKind::CharSelect ||
			g_ownerOf[i].kind == PaletteTexture::OwnerKind::LobbyAvatar)
		{
			continue;
		}

		if (!g_boundInMatch[i])
			continue;

		int player;

		if (counted)
			player = i == charaFirst ? 1 : (i == charaSecond ? 0 : -1);
		else
			player = i == arrivedFirst ? 1 : (i == arrivedSecond ? 0 : -1);

		if (g_swapSides && player >= 0)
			player = 1 - player;

		if (player >= 0 && FindStrongPlayer(player) >= 0)
			player = -1;

		PaletteTexture::TextureOwner owner = {};
		if (player >= 0)
		{
			owner.kind = PaletteTexture::OwnerKind::Player;
			owner.players = PaletteTexture::PlayerBit(player);
		}
		else if (counted)
		{
			owner.kind = PaletteTexture::OwnerKind::Effect;
		}

		if (SameOwner(g_ownerOf[i], owner))
			continue;

		g_ownerOf[i] = owner;

		const bool wasFirst = i == (counted ? charaFirst : arrivedFirst);

		if (player < 0)
		{
			PaletteTrace::Note("guess makes texture %d an effect's", i);
			continue;
		}

		PaletteTrace::Note("guess gives texture %d to P%d, the %s bound of the match%s", i,
			player + 1, wasFirst ? "first" : "second", g_swapSides ? ", sides swapped" : "");

		LOG("palette texture: 0x%p is P%d's, the %s of the match's%s", g_seen[i], player + 1,
			wasFirst ? "first" : "second", g_swapSides ? " (sides swapped)" : "");
	}
}

}

bool PaletteTexture::GetMatchTextures(int& outFirst, int& outSecond)
{
	return FindCharacterTextures(outFirst, outSecond);
}

void PaletteTexture::OnFrame()
{
	// Same reason as PaletteSeat: with the game's frame suppressed nothing binds a texture, so the
	// stale sweep below would drop every one of them while the user is simply paused.
	if (FrameStepper::IsFrozen())
		return;

	++g_frameSerial;

	ForgetRejected();

	const bool inMatch = GameState::IsInMatch();

	for (int i = 0; i < g_frameCount; ++i)
	{
		const int index = g_frameOrder[i];
		if (index >= 0 && index < kMaxSeen)
		{
			++g_framesBound[index];

			if (inMatch)
				g_boundInMatch[index] = true;
		}
	}

	if (g_frameCount > 0)
		++g_framesWatched;

	g_frameCount = 0;

	WatchMatch(inMatch);

	if (g_heuristicEnabled)
		RefreshOwners();
}

void PaletteTexture::ForgetMatchTextures()
{
	for (int i = g_seenCount - 1; i >= 0; --i)
	{
		if (g_ownerOf[i].kind == OwnerKind::CharSelect ||
			g_ownerOf[i].kind == OwnerKind::LobbyAvatar)
		{
			continue;
		}

		if (!g_boundInMatch[i] && !g_bornInMatch[i] && g_ownerOf[i].kind == OwnerKind::Unknown)
			continue;

		RemoveAt(i, "match over");
	}

	ResetMatchEvidence();
}

void PaletteTexture::ResetMatchEvidence()
{

	memset(g_evicted, 0, sizeof(g_evicted));
	g_evictedAt = 0;

	memset(g_frameOrder, 0, sizeof(g_frameOrder));
	g_frameCount = 0;
	g_framesWatched = 0;
	g_rowsSwapped = false;

	memset(g_firstSeen, 0, sizeof(g_firstSeen));
	memset(g_secondSeen, 0, sizeof(g_secondSeen));
	memset(g_framesBound, 0, sizeof(g_framesBound));
}

void PaletteTexture::SetHeuristicEnabled(bool enabled)
{
	g_heuristicEnabled = enabled;
}

bool PaletteTexture::IsHeuristicEnabled()
{
	return g_heuristicEnabled;
}

int PaletteTexture::FindByPointer(uintptr_t texture)
{
	if (texture == 0)
		return -1;

	for (int i = 0; i < g_seenCount; ++i)
	{
		if (reinterpret_cast<uintptr_t>(g_seen[i]) == texture)
			return i;
	}

	return -1;
}

void PaletteTexture::SetRowsSwapped(bool swapped)
{
	if (g_rowsSwapped == swapped)
		return;

	g_rowsSwapped = swapped;
	++g_rowGeneration;
}

bool PaletteTexture::GetRowsSwapped()
{
	return g_rowsSwapped;
}

int PaletteTexture::GetRowGeneration()
{
	return g_rowGeneration;
}

bool PaletteTexture::RowHasRgb(int index, unsigned row, const uint8_t* rgba)
{
	if (rgba == nullptr)
		return false;

	uint8_t current[kRowBytes] = {};
	if (!ReadRowAsRgba(index, row, current))
		return false;

	for (int c = 0; c < static_cast<int>(kWidth); ++c)
	{
		if (current[c * 4 + 0] != rgba[c * 4 + 0] || current[c * 4 + 1] != rgba[c * 4 + 1] ||
			current[c * 4 + 2] != rgba[c * 4 + 2])
		{
			return false;
		}
	}

	return true;
}

void PaletteTexture::SetSwapSides(bool swap)
{
	if (g_swapSides == swap)
		return;

	g_swapSides = swap;

	for (int i = 0; i < kMaxSeen; ++i)
	{
		if (g_ownerOf[i].kind == OwnerKind::CharSelect ||
			g_ownerOf[i].kind == OwnerKind::LobbyAvatar)
		{
			continue;
		}

		g_ownerByHand[i] = false;
		g_claim[i] = Claim::Guess;
		g_ownerOf[i] = {};
	}
}

PaletteTexture::Claim PaletteTexture::GetClaim(int index)
{
	if (index < 0 || index >= g_seenCount)
		return Claim::Guess;

	return g_claim[index];
}

bool PaletteTexture::GetSwapSides()
{
	return g_swapSides;
}

int PaletteTexture::GetRowForPlayer(int player)
{
	if (player < 0 || player > 1)
		return -1;

	return g_rowsSwapped ? 1 - player : player;
}

void PaletteTexture::Dump()
{
	LOG_SECTION("palette state");

	for (int player = 0; player < 2; ++player)
	{
		void* chara = MemoryMap::GetCharaSlot(player);
		const int slot = PaletteMemory::GetPlayerSlot(player);
		const int chara_no = PaletteMemory::GetCharaNumber(player);

		uint32_t side = 0;
		if (chara != nullptr)
			MemoryMap::ReadStructDword(chara, GameOffsets::kCharaSideIndex, side);

		uint8_t colors[PaletteMemory::kPaletteBytes] = {};
		const bool read = PaletteMemory::ReadPlayerPalette(player, colors);

		LOG_RAW("player %d: chara 0x%08x side %u number %d slot %d  table 0x%08x  palette 0x%08x",
			player, static_cast<unsigned>(reinterpret_cast<uintptr_t>(chara)), side & 0xff,
			chara_no, slot,
			static_cast<unsigned>(PaletteMemory::GetPlayerPaletteTable(player)),
			static_cast<unsigned>(PaletteMemory::GetPlayerPaletteAddress(player)));

		if (read)
		{
			LOG_RAW("   memory says  %02x%02x%02x %02x%02x%02x %02x%02x%02x %02x%02x%02x",
				colors[4], colors[5], colors[6], colors[8], colors[9], colors[10],
				colors[12], colors[13], colors[14], colors[16], colors[17], colors[18]);
		}
		else
		{
			LOG_RAW("   memory says  nothing readable");
		}
	}

	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_seen[i] == nullptr)
			continue;

		LOG_RAW("texture %d: 0x%p  %d binds  owner %s players 0x%x chara %d%s  age %d%s  first %d/%d  second %d/%d",
			i, g_seen[i], g_binds[i], OwnerKindName(g_ownerOf[i].kind), g_ownerOf[i].players,
			g_ownerOf[i].chara, g_ownerByHand[i] ? " (by hand)" : "",
			g_frameSerial - g_lastBound[i], g_boundInMatch[i] ? " (match's)" : "",
			g_firstSeen[i], g_framesWatched, g_secondSeen[i], g_framesWatched);

		for (int player = 0; player < 2; ++player)
		{
			const int slot = PaletteMemory::GetPlayerSlot(player);
			if (slot < 0 || slot >= static_cast<int>(kRows))
				continue;

			uint8_t row[kRowBytes] = {};
			if (!ReadRow(i, static_cast<unsigned>(slot), row))
				continue;

			LOG_RAW("   row %2d (p%d)  %02x%02x%02x %02x%02x%02x %02x%02x%02x %02x%02x%02x",
				slot, player,
				row[6], row[5], row[4], row[10], row[9], row[8],
				row[14], row[13], row[12], row[18], row[17], row[16]);
		}
	}
}

void PaletteTexture::Forget()
{
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_seen[i] != nullptr)
			g_seen[i]->Release();
	}

	ForgetRejected();

	g_seenCount = 0;
	memset(g_seen, 0, sizeof(g_seen));
	memset(g_binds, 0, sizeof(g_binds));
	memset(g_backup, 0, sizeof(g_backup));
	memset(g_ownerByHand, 0, sizeof(g_ownerByHand));
	memset(g_claim, 0, sizeof(g_claim));
	memset(g_firstSeen, 0, sizeof(g_firstSeen));
	memset(g_secondSeen, 0, sizeof(g_secondSeen));
	memset(g_framesBound, 0, sizeof(g_framesBound));
	memset(g_lastBound, 0, sizeof(g_lastBound));
	memset(g_firstSeenAt, 0, sizeof(g_firstSeenAt));
	memset(g_volatilePool, 0, sizeof(g_volatilePool));
	memset(g_bornInMatch, 0, sizeof(g_bornInMatch));
	memset(g_boundInMatch, 0, sizeof(g_boundInMatch));
	memset(g_evicted, 0, sizeof(g_evicted));
	g_evictedAt = 0;
	g_frameCount = 0;
	g_framesWatched = 0;
	g_rowsSwapped = false;
	++g_generation;

	for (int i = 0; i < kMaxSeen; ++i)
		g_ownerOf[i] = {};
}

int PaletteTexture::GetGeneration()
{
	return g_generation;
}

void PaletteTexture::OnDeviceLost()
{

	ForgetRejected();

	int dropped = 0;

	for (int i = g_seenCount - 1; i >= 0; --i)
	{
		if (!g_volatilePool[i])
			continue;

		RemoveAt(i, "lost with the device");
		++dropped;
	}

	LOG("palette texture: device lost, %d dropped, %d kept", dropped, g_seenCount);
}

int PaletteTexture::GetSeenCount()
{
	return g_seenCount;
}

uintptr_t PaletteTexture::GetSeen(int index)
{
	if (index < 0 || index >= g_seenCount)
		return 0;

	return reinterpret_cast<uintptr_t>(g_seen[index]);
}

bool PaletteTexture::HasPristineRow(int index, unsigned row)
{
	return HasBackup(index, row);
}

int PaletteTexture::GetBindCount(int index)
{
	if (index < 0 || index >= g_seenCount)
		return 0;

	return g_binds[index];
}

int PaletteTexture::GetFrameCount(int index)
{
	if (index < 0 || index >= g_seenCount)
		return 0;

	return g_framesBound[index];
}

PaletteTexture::TextureOwner PaletteTexture::GetOwnerInfo(int index)
{
	if (index < 0 || index >= g_seenCount)
		return {};

	return g_ownerOf[index];
}

bool PaletteTexture::IsOwnerByHand(int index)
{
	if (index < 0 || index >= g_seenCount)
		return false;

	return g_ownerByHand[index];
}

namespace {

void RemoveAt(int index, const char* why)
{
	PaletteTrace::Note("texture %d dropped, %s (%s players 0x%x chara %d)", index, why,
		OwnerKindName(g_ownerOf[index].kind), g_ownerOf[index].players, g_ownerOf[index].chara);

	LOG("palette texture: 0x%p %s (%s players 0x%x chara %d)", g_seen[index], why,
		OwnerKindName(g_ownerOf[index].kind), g_ownerOf[index].players, g_ownerOf[index].chara);

	if (g_seen[index] != nullptr)
		g_seen[index]->Release();

	g_seen[index] = nullptr;
	g_ownerOf[index] = {};
	g_claim[index] = PaletteTexture::Claim::Guess;
	g_ownerByHand[index] = false;
	g_binds[index] = 0;
	g_firstSeen[index] = 0;
	g_secondSeen[index] = 0;
	g_framesBound[index] = 0;
	g_lastBound[index] = 0;
	g_firstSeenAt[index] = 0;
	g_volatilePool[index] = false;
	g_bornInMatch[index] = false;
	g_boundInMatch[index] = false;
	memset(g_backup[index], 0, sizeof(g_backup[index]));

	int kept = 0;
	for (int k = 0; k < g_frameCount; ++k)
	{
		if (g_frameOrder[k] == index)
			continue;

		g_frameOrder[kept++] = g_frameOrder[k];
	}
	g_frameCount = kept;

	while (g_seenCount > 0 && g_seen[g_seenCount - 1] == nullptr)
		--g_seenCount;

	++g_generation;
}

void EvictAt(int index)
{
	g_evicted[g_evictedAt] = reinterpret_cast<uintptr_t>(g_seen[index]);
	g_evictedAt = (g_evictedAt + 1) % kMaxEvicted;

	RemoveAt(index, "evicted");
}

}

void PaletteTexture::Evict(int index)
{
	if (index < 0 || index >= g_seenCount)
		return;

	EvictAt(index);
}

void PaletteTexture::AssignOwner(int index, TextureOwner owner, Claim claim)
{
	if (index < 0 || index >= g_seenCount)
		return;

	if (claim < g_claim[index] && g_ownerOf[index].kind != OwnerKind::Unknown)
		return;

	const bool byHand = claim == Claim::Hand;

	if (owner.kind == OwnerKind::Player && owner.players != 0)
	{
		for (int i = g_seenCount - 1; i >= 0; --i)
		{
			if (i == index || g_ownerOf[i].kind != OwnerKind::Player)
				continue;

			const uint8_t remaining =
				static_cast<uint8_t>(g_ownerOf[i].players & ~owner.players);

			if (remaining == g_ownerOf[i].players)
				continue;

			if (remaining != 0)
			{
				g_ownerOf[i].players = remaining;
				continue;
			}

			if (g_frameSerial - g_lastBound[i] <= 2)
			{
				g_ownerOf[i] = {};
				continue;
			}

			EvictAt(i);
		}
	}
	else if (IsCharacterOwner(owner))
	{
		for (int i = g_seenCount - 1; i >= 0; --i)
		{
			if (i == index || !SameOwner(g_ownerOf[i], owner))
				continue;

			EvictAt(i);
		}
	}

	const bool changed = !SameOwner(g_ownerOf[index], owner);

	g_ownerOf[index] = owner;
	g_ownerByHand[index] = byHand;
	g_claim[index] = claim;

	if (!changed)
		return;

	PaletteTrace::Note("texture %d is now %s players 0x%x chara %d, claimed %s", index,
		OwnerKindName(owner.kind), owner.players, owner.chara,
		claim == Claim::Hand ? "by hand" : (claim == Claim::Strong ? "strongly" : "by the guess"));

	LOG("palette texture: 0x%p is %s players 0x%x chara %d%s", g_seen[index],
		OwnerKindName(owner.kind), owner.players, owner.chara, byHand ? " (by hand)" : "");
}

int PaletteTexture::FindOwned(TextureOwner owner)
{
	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_seen[i] == nullptr)
			continue;

		if (SameOwner(g_ownerOf[i], owner))
			return i;
	}

	return -1;
}

void PaletteTexture::NoteInUse(int index)
{
	if (Live(index) == nullptr)
		return;

	g_lastBound[index] = g_frameSerial;
}

int PaletteTexture::GetLastBoundAge(int index)
{
	if (index < 0 || index >= g_seenCount)
		return -1;

	return g_frameSerial - g_lastBound[index];
}

bool PaletteTexture::WasBoundInMatch(int index)
{
	if (index < 0 || index >= g_seenCount)
		return false;

	return g_boundInMatch[index];
}

int PaletteTexture::GetFirstSeenFrame(int index)
{
	if (index < 0 || index >= g_seenCount)
		return -1;

	return g_firstSeenAt[index];
}

int PaletteTexture::GetFrameSerial()
{
	return g_frameSerial;
}

int PaletteTexture::GetOwner(int index)
{
	if (index < 0 || index >= g_seenCount || g_ownerOf[index].kind != OwnerKind::Player)
		return -1;

	for (int player = 0; player < 2; ++player)
	{
		if ((g_ownerOf[index].players & PlayerBit(player)) != 0)
			return player;
	}

	return -1;
}

void PaletteTexture::SetOwner(int index, int player)
{
	if (index < 0 || index >= g_seenCount)
		return;

	if (player < 0)
	{
		g_ownerOf[index] = {};
		g_ownerByHand[index] = false;
		return;
	}

	TextureOwner owner = {};
	owner.kind = OwnerKind::Player;
	owner.players = PlayerBit(player);
	AssignOwner(index, owner, Claim::Hand);
}

int PaletteTexture::FindForPlayer(int player)
{
	if (player < 0 || player > 1)
		return -1;

	for (int i = 0; i < g_seenCount; ++i)
	{
		if (g_ownerOf[i].kind == OwnerKind::Player &&
			(g_ownerOf[i].players & PlayerBit(player)) != 0)
		{
			return i;
		}
	}

	return -1;
}

int PaletteTexture::GetShapeCount()
{
	return g_shapeCount;
}

bool PaletteTexture::GetShape(int index, unsigned& width, unsigned& height, unsigned& format,
	int& count)
{
	if (index < 0 || index >= g_shapeCount)
		return false;

	width = g_shapes[index].width;
	height = g_shapes[index].height;
	format = g_shapes[index].format;
	count = g_shapes[index].count;
	return true;
}

bool PaletteTexture::ReadRow(int index, unsigned row, uint8_t* out)
{
	IDirect3DTexture9* const texture = Live(index);

	if (texture == nullptr || row >= kRows || out == nullptr)
		return false;

	D3DLOCKED_RECT locked = {};
	if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
		return false;

	memcpy(out, static_cast<const uint8_t*>(locked.pBits) + row * locked.Pitch, kRowBytes);
	texture->UnlockRect(0);
	return true;
}

bool PaletteTexture::WriteRow(int index, unsigned row, const uint8_t* colors)
{
	IDirect3DTexture9* const texture = Live(index);

	if (texture == nullptr || row >= kRows || colors == nullptr)
		return false;

	if (!g_backup[index][row].taken)
	{
		if (ReadRow(index, row, g_backup[index][row].colors))
			g_backup[index][row].taken = true;
	}

	D3DLOCKED_RECT locked = {};
	if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
		return false;

	memcpy(static_cast<uint8_t*>(locked.pBits) + row * locked.Pitch, colors, kRowBytes);
	texture->UnlockRect(0);
	return true;
}

int PaletteTexture::RestoreAll(int index)
{
	int restored = 0;

	for (unsigned row = 0; row < kRows; ++row)
		restored += Restore(index, row) ? 1 : 0;

	return restored;
}

bool PaletteTexture::ReadRowAsRgba(int index, unsigned row, uint8_t* out)
{
	uint8_t raw[kRowBytes] = {};
	if (!ReadRow(index, row, raw))
		return false;

	for (int c = 0; c < static_cast<int>(kWidth); ++c)
	{
		out[c * 4 + 0] = raw[c * 4 + 2];
		out[c * 4 + 1] = raw[c * 4 + 1];
		out[c * 4 + 2] = raw[c * 4 + 0];
		out[c * 4 + 3] = raw[c * 4 + 3];
	}

	return true;
}

bool PaletteTexture::ReadPristineRowAsRgba(int index, unsigned row, uint8_t* out)
{
	if (Live(index) == nullptr || row >= kRows || out == nullptr)
		return false;

	if (!g_backup[index][row].taken)
		return ReadRowAsRgba(index, row, out);

	const uint8_t* const raw = g_backup[index][row].colors;

	for (int c = 0; c < static_cast<int>(kWidth); ++c)
	{
		out[c * 4 + 0] = raw[c * 4 + 2];
		out[c * 4 + 1] = raw[c * 4 + 1];
		out[c * 4 + 2] = raw[c * 4 + 0];
		out[c * 4 + 3] = raw[c * 4 + 3];
	}

	return true;
}

bool PaletteTexture::WriteRowKeepingAlpha(int index, unsigned row, const uint8_t* colors)
{
	uint8_t current[kRowBytes] = {};
	if (!ReadRow(index, row, current))
		return false;

	uint8_t blended[kRowBytes] = {};
	for (int c = 0; c < static_cast<int>(kWidth); ++c)
	{
		blended[c * 4 + 0] = colors[c * 4 + 2];
		blended[c * 4 + 1] = colors[c * 4 + 1];
		blended[c * 4 + 2] = colors[c * 4 + 0];
		blended[c * 4 + 3] = current[c * 4 + 3];
	}

	return WriteRow(index, row, blended);
}

bool PaletteTexture::HasBackup(int index, unsigned row)
{
	if (Live(index) == nullptr || row >= kRows)
		return false;

	return g_backup[index][row].taken;
}

bool PaletteTexture::Restore(int index, unsigned row)
{
	if (!HasBackup(index, row))
		return false;

	IDirect3DTexture9* const texture = g_seen[index];

	D3DLOCKED_RECT locked = {};
	if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
		return false;

	memcpy(static_cast<uint8_t*>(locked.pBits) + row * locked.Pitch,
		g_backup[index][row].colors, kRowBytes);
	texture->UnlockRect(0);
	return true;
}

int PaletteTexture::WriteToAll(const uint8_t* colors)
{
	int written = 0;

	for (int i = 0; i < g_seenCount; ++i)
	{
		for (unsigned row = 0; row < kRows; ++row)
			written += WriteRow(i, row, colors) ? 1 : 0;
	}

	return written;
}
