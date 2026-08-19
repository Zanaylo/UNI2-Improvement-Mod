#include "Palette/PaletteMemory.h"

#include "Palette/PaletteTexture.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"
#include "Game/MemoryScanner.h"
#include "Hooks/HookManager.h"

#include "MinHook.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kMaxLoads = 64;
constexpr int kHeaderBytes = 16;
constexpr int kCountOffset = 12;
constexpr int kMaxPalettes = 4096;

struct Load
{
	char name[32];
	uintptr_t address;
	int count;
	int chara;
	int kind;
	int frame;
};

int ParseLoadChara(const char* name)
{
	int chara = -1;

	for (const char* at = name; *at != '\0'; ++at)
	{
		if ((at[0] == 'c' || at[0] == 'C') && (at[1] == 'h' || at[1] == 'H') &&
			(at[2] == 'r' || at[2] == 'R') &&
			at[3] >= '0' && at[3] <= '9' && at[4] >= '0' && at[4] <= '9' &&
			at[5] >= '0' && at[5] <= '9')
		{
			chara = (at[3] - '0') * 100 + (at[4] - '0') * 10 + (at[5] - '0');
		}
	}

	return chara;
}

bool ContainsNoCase(const char* haystack, const char* needle)
{
	const size_t length = strlen(needle);

	for (const char* at = haystack; *at != '\0'; ++at)
	{
		size_t i = 0;
		while (i < length && (at[i] | 0x20) == (needle[i] | 0x20))
			++i;

		if (i == length)
			return true;
	}

	return false;
}

int ParseLoadKind(const char* name)
{
	if (ContainsNoCase(name, "_csel"))
		return PaletteMemory::kLoadCharSelect;

	if (ContainsNoCase(name, "coloredit"))
		return PaletteMemory::kLoadColorEdit;

	if (ContainsNoCase(name, "avater"))
		return PaletteMemory::kLoadLobbyAvatar;

	return PaletteMemory::kLoadBattle;
}

Load g_loads[kMaxLoads] = {};
int g_loadCount = 0;
bool g_installed = false;
int g_loaderCalls = 0;
constexpr int kLoaderCallsLogged = 24;

using LoadPalette_t = void*(__fastcall*)(void*, void*, const char*);
LoadPalette_t oLoadPalette = nullptr;

void Record(const char* name, void* buffer)
{
	if (name == nullptr || buffer == nullptr)
		return;

	const uintptr_t address = reinterpret_cast<uintptr_t>(buffer);

	for (int i = 0; i < g_loadCount; ++i)
	{
		if (g_loads[i].address == address)
			return;
	}

	if (g_loadCount >= kMaxLoads)
		return;

	uint32_t count = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(address + kCountOffset), count))
		return;

	if (count == 0 || count > kMaxPalettes)
		return;

	uint8_t probe = 0;
	const uintptr_t last = address + kHeaderBytes + (count - 1) * PaletteMemory::kPaletteBytes;
	if (!TryReadMemory(&probe, reinterpret_cast<const void*>(last), sizeof(probe)))
		return;

	Load& load = g_loads[g_loadCount];
	strncpy_s(load.name, name, _TRUNCATE);
	load.address = address;
	load.count = static_cast<int>(count);
	load.chara = ParseLoadChara(name);
	load.kind = ParseLoadKind(name);
	load.frame = PaletteTexture::GetFrameSerial();
	++g_loadCount;

	uint32_t head[4] = {};
	for (int i = 0; i < 4; ++i)
		TryReadDword(reinterpret_cast<const void*>(address + i * 4), head[i]);

	LOG("palette: '%s' loaded at 0x%08x, %d palettes  [%08x %08x %08x %08x]",
		load.name, static_cast<unsigned>(address), load.count, head[0], head[1], head[2], head[3]);
}

void* __fastcall HookedLoadPalette(void* self, void* unused, const char* name)
{
	++g_loaderCalls;

	void* buffer = oLoadPalette(self, unused, name);

	if (g_loaderCalls <= kLoaderCallsLogged)
	{
		uint32_t head = 0;
		const bool readable = buffer != nullptr &&
			TryReadDword(reinterpret_cast<const void*>(
				reinterpret_cast<uintptr_t>(buffer) + kCountOffset), head);

		LOG("palette loader: call %d '%s' -> 0x%08x, count dword %s %u", g_loaderCalls,
			name != nullptr ? name : "(null)", static_cast<unsigned>(
				reinterpret_cast<uintptr_t>(buffer)), readable ? "=" : "unreadable", head);
	}

	Record(name, buffer);
	return buffer;
}

void* PlayerData(int player)
{
	if (player < 0 || player > 1)
		return nullptr;

	return MemoryMap::GetCharaSlot(player);
}

int CharaNumber(int player)
{
	void* chara = PlayerData(player);
	if (chara == nullptr)
		return -1;

	uint32_t side = 0;
	if (!MemoryMap::ReadStructDword(chara, GameOffsets::kCharaSideIndex, side))
		return -1;

	side &= 0xff;
	if (side > 1)
		return -1;

	uint32_t number = 0;
	if (!MemoryMap::ReadDwordAt(RvaToAddress(GameOffsets::kCharaRecordBase) +
		side * GameOffsets::kCharaRecordStride, number))
	{
		return -1;
	}

	return static_cast<int>(number);
}

uintptr_t PaletteAddress(int load, int index)
{
	if (load < 0 || load >= g_loadCount)
		return 0;

	if (index < 0 || index >= g_loads[load].count)
		return 0;

	return g_loads[load].address + kHeaderBytes +
		static_cast<uintptr_t>(index) * PaletteMemory::kPaletteBytes;
}

}

bool PaletteMemory::Install()
{
	if (g_installed)
		return true;

	void* target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnLoadCharaPalette));

	if (!HookManager::CreateAndEnableHook(target, &HookedLoadPalette,
		reinterpret_cast<void**>(&oLoadPalette), "LoadCharaPalette"))
	{
		return false;
	}

	g_installed = true;
	LOG("PaletteMemory installed on the palette loader at 0x%p", target);
	return true;
}

bool PaletteMemory::IsInstalled()
{
	return g_installed;
}

bool LooksLikePalette(uintptr_t address)
{
	uint8_t sample[256] = {};
	if (!TryReadMemory(sample, reinterpret_cast<const void*>(address), sizeof(sample)))
		return false;

	int opaque = 0;
	for (int i = 0; i < 64; ++i)
	{
		if (sample[i * 4 + 3] == 0xff)
			++opaque;
	}

	return opaque >= 60;
}

int PaletteMemory::ProbeFromCharacters()
{
	constexpr int kCharaDwords = 0xba4 / 4;
	int found = 0;

	for (int player = 0; player < 2; ++player)
	{
		void* chara = PlayerData(player);
		if (chara == nullptr)
			continue;

		for (int i = 0; i < kCharaDwords; ++i)
		{
			uint32_t pointer = 0;
			if (!MemoryMap::ReadStructDword(chara, i * 4, pointer))
				continue;

			if (pointer < 0x10000)
				continue;

			if (LooksLikePalette(pointer))
			{
				LOG("palette: p%d +0x%03x -> 0x%08x looks like colours", player, i * 4, pointer);
				++found;
			}

			for (int k = 0; k < 64; ++k)
			{
				uint32_t inner = 0;
				if (!TryReadDword(reinterpret_cast<const void*>(pointer + k * 4), inner))
					break;

				if (inner < 0x10000 || !LooksLikePalette(inner))
					continue;

				LOG("palette: p%d +0x%03x +0x%02x -> 0x%08x looks like colours",
					player, i * 4, k * 4, inner);
				++found;
			}
		}
	}

	LOG("palette: %d place(s) reachable from a character look like colours", found);
	return found;
}

int PaletteMemory::Scan()
{

	static const uint8_t header[16] = {
		0xff, 0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x82, 0x00, 0x00, 0x00
	};

	uintptr_t hits[kMaxLoads] = {};
	const int found = MemoryScanner::FindBytes(header, sizeof(header), hits, kMaxLoads);

	for (int i = 0; i < found; ++i)
	{
		char name[32] = {};
		sprintf_s(name, "scan %d", i);
		Record(name, reinterpret_cast<void*>(hits[i]));
	}

	LOG("palette: scan matched %d place(s), %d of them the size of a palette file",
		found, g_loadCount);

	return g_loadCount;
}

void PaletteMemory::DescribeResolution(int player, char* out, int size)
{
	void* const chara = PlayerData(player);

	if (chara == nullptr)
	{
		sprintf_s(out, size, "P%d no chara", player + 1);
		return;
	}

	uint32_t table = 0;
	uint32_t alt = 0;
	MemoryMap::ReadStructDword(chara, GameOffsets::kPlayerDataPaletteTable, table);
	MemoryMap::ReadStructDword(chara, GameOffsets::kPlayerDataPaletteTableAlt, alt);

	uint32_t slotRaw = 0;
	MemoryMap::ReadStructDword(chara, GameOffsets::kPlayerDataPaletteSlot, slotRaw);

	sprintf_s(out, size, "P%d chara 0x%08x +654 0x%08x +754 0x%08x slot %u -> table 0x%08x pal 0x%08x",
		player + 1, static_cast<unsigned>(reinterpret_cast<uintptr_t>(chara)), table, alt, slotRaw,
		static_cast<unsigned>(GetPlayerPaletteTable(player)),
		static_cast<unsigned>(GetPlayerPaletteAddress(player)));
}

int PaletteMemory::GetLoaderCallCount()
{
	return g_loaderCalls;
}

int PaletteMemory::GetLoadCount()
{
	return g_loadCount;
}

const char* PaletteMemory::GetLoadName(int index)
{
	if (index < 0 || index >= g_loadCount)
		return "";

	return g_loads[index].name;
}

uintptr_t PaletteMemory::GetLoadAddress(int index)
{
	if (index < 0 || index >= g_loadCount)
		return 0;

	return g_loads[index].address;
}

int PaletteMemory::GetLoadPaletteCount(int index)
{
	if (index < 0 || index >= g_loadCount)
		return 0;

	return g_loads[index].count;
}

void PaletteMemory::ForgetLoads()
{
	g_loadCount = 0;
}

int PaletteMemory::GetLoadChara(int index)
{
	if (index < 0 || index >= g_loadCount)
		return -1;

	return g_loads[index].chara;
}

int PaletteMemory::GetLoadKind(int index)
{
	if (index < 0 || index >= g_loadCount)
		return kLoadBattle;

	return g_loads[index].kind;
}

int PaletteMemory::GetLoadFrame(int index)
{
	if (index < 0 || index >= g_loadCount)
		return -1;

	return g_loads[index].frame;
}

int PaletteMemory::FindLoadForPlayer(int player)
{
	const int number = CharaNumber(player);
	if (number < 0)
		return -1;

	char wanted[16] = {};
	sprintf_s(wanted, "chr%03d", number);

	for (int i = g_loadCount - 1; i >= 0; --i)
	{
		if (strstr(g_loads[i].name, wanted) != nullptr && g_loads[i].count > 0)
			return i;
	}

	return -1;
}

uintptr_t PaletteMemory::GetPlayerPaletteTable(int player)
{
	void* chara = PlayerData(player);
	if (chara == nullptr)
		return 0;

	const int slot = GetPlayerSlot(player);

	const uintptr_t candidates[] = {
		GameOffsets::kPlayerDataPaletteTable, GameOffsets::kPlayerDataPaletteTableAlt
	};

	for (int i = 0; i < 2; ++i)
	{
		uint32_t table = 0;
		if (!MemoryMap::ReadStructDword(chara, candidates[i], table) || table < 0x10000)
			continue;

		if (slot < 0)
			return table;

		uint32_t entry = 0;
		if (TryReadDword(reinterpret_cast<const void*>(
			table + GameOffsets::kPaletteTableFirst + slot * 4), entry) && entry >= 0x10000)
		{
			return table;
		}

	}

	return 0;
}

uintptr_t PaletteMemory::GetPlayerPaletteAddress(int player)
{
	const uintptr_t table = GetPlayerPaletteTable(player);
	const int slot = GetPlayerSlot(player);

	if (table == 0 || slot < 0 || slot >= GameOffsets::kPaletteSlots)
		return 0;

	uint32_t palette = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(
		table + GameOffsets::kPaletteTableFirst + slot * 4), palette))
	{
		return 0;
	}

	return palette < 0x10000 ? 0 : palette;
}

uintptr_t PaletteMemory::GetPlayerCurrentPalette(int player)
{
	const uintptr_t table = GetPlayerPaletteTable(player);
	if (table == 0)
		return 0;

	uint32_t palette = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(
		table + GameOffsets::kPaletteTableCurrent), palette))
	{
		return 0;
	}

	return palette < 0x10000 ? 0 : palette;
}

namespace {

bool WriteBlock(uintptr_t address, const uint8_t* data, size_t size)
{
	if (address == 0 || data == nullptr)
		return false;

	DWORD previous = 0;
	const bool unprotected = VirtualProtect(reinterpret_cast<void*>(address), size,
		PAGE_EXECUTE_READWRITE, &previous) != 0;

	bool ok = true;
	__try
	{
		memcpy(reinterpret_cast<void*>(address), data, size);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ok = false;
	}

	if (unprotected)
	{
		DWORD ignored = 0;
		VirtualProtect(reinterpret_cast<void*>(address), size, previous, &ignored);
	}

	if (!ok)
		return false;

	uint8_t check[8] = {};
	if (!TryReadMemory(check, reinterpret_cast<const void*>(address), sizeof(check)))
		return false;

	return memcmp(check, data, sizeof(check)) == 0;
}

}

bool PaletteMemory::ReadPlayerPalette(int player, uint8_t* out)
{
	const uintptr_t address = GetPlayerPaletteAddress(player);
	if (address == 0 || out == nullptr)
		return false;

	return TryReadMemory(out, reinterpret_cast<const void*>(address), kPaletteBytes);
}

bool PaletteMemory::ReadPlayerPaletteAt(int player, int index, uint8_t* out)
{
	if (out == nullptr || index < 0 || index >= GameOffsets::kPaletteSlots)
		return false;

	const uintptr_t table = GetPlayerPaletteTable(player);
	if (table == 0)
		return false;

	uint32_t palette = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(
		table + GameOffsets::kPaletteTableFirst + index * 4), palette))
	{
		return false;
	}

	if (palette < 0x10000)
		return false;

	return TryReadMemory(out, reinterpret_cast<const void*>(palette), kPaletteBytes);
}

int PaletteMemory::FindCopiesOfPlayerPalette(int player)
{
	const uintptr_t source = GetPlayerPaletteAddress(player);
	if (source == 0)
		return 0;

	uint8_t sample[64] = {};
	if (!TryReadMemory(sample, reinterpret_cast<const void*>(source + 256), sizeof(sample)))
		return 0;

	uintptr_t hits[64] = {};
	const int found = MemoryScanner::FindBytes(sample, sizeof(sample), hits, 64);

	int elsewhere = 0;
	uintptr_t previous = 0;

	for (int i = 0; i < found; ++i)
	{
		if (hits[i] == source + 256)
			continue;

		if (previous != 0 && hits[i] - previous < kPaletteBytes)
			continue;

		previous = hits[i];

		LOG("palette: p%d colours also at 0x%08x (%+d from the slot's copy)", player,
			static_cast<unsigned>(hits[i]), static_cast<int>(hits[i] - (source + 256)));
		++elsewhere;
	}

	LOG("palette: p%d slot palette 0x%08x appears in %d other place(s)", player,
		static_cast<unsigned>(source), elsewhere);

	return elsewhere;
}

int PaletteMemory::WritePlayerPalette(int player, const uint8_t* data)
{
	const uintptr_t slotPalette = GetPlayerPaletteAddress(player);
	const uintptr_t current = GetPlayerCurrentPalette(player);

	int written = 0;

	if (WriteBlock(slotPalette, data, kPaletteBytes))
		++written;

	if (current != slotPalette && WriteBlock(current, data, kPaletteBytes))
		++written;

	LOG("palette: p%d wrote %d of 2 (slot 0x%08x, current 0x%08x)", player, written,
		static_cast<unsigned>(slotPalette), static_cast<unsigned>(current));

	return written;
}

int PaletteMemory::GetCharaNumber(int player)
{
	return CharaNumber(player);
}

int PaletteMemory::GetPlayerSlot(int player)
{
	void* chara = PlayerData(player);
	if (chara == nullptr)
		return -1;

	uint32_t slot = 0;
	if (!MemoryMap::ReadStructDword(chara, GameOffsets::kPlayerDataPaletteSlot, slot))
		return -1;

	return static_cast<int>(slot);
}

bool PaletteMemory::SetPlayerSlot(int player, int slot)
{
	void* chara = PlayerData(player);
	if (chara == nullptr || slot < 0)
		return false;

	if (!TryWriteDword(reinterpret_cast<uint8_t*>(chara) + GameOffsets::kPlayerDataPaletteSlot,
		static_cast<uint32_t>(slot)))
	{
		return false;
	}

	void* effects[64] = {};
	const int count = MemoryMap::EnumerateEffectSlots(effects, 64);

	for (int i = 0; i < count; ++i)
	{
		uint32_t owner = 0;
		if (!MemoryMap::ReadStructDword(effects[i], GameOffsets::kCharaOwner, owner))
			continue;

		if (owner != reinterpret_cast<uint32_t>(chara))
			continue;

		TryWriteDword(reinterpret_cast<uint8_t*>(effects[i]) + GameOffsets::kPlayerDataPaletteSlot,
			static_cast<uint32_t>(slot));
	}

	return true;
}

bool PaletteMemory::ReadPalette(int load, int index, uint8_t* out)
{
	const uintptr_t address = PaletteAddress(load, index);
	if (address == 0 || out == nullptr)
		return false;

	return TryReadMemory(out, reinterpret_cast<const void*>(address), kPaletteBytes);
}

bool PaletteMemory::WritePalette(int load, int index, const uint8_t* data)
{
	const uintptr_t address = PaletteAddress(load, index);
	if (address == 0 || data == nullptr)
		return false;

	const uint32_t* words = reinterpret_cast<const uint32_t*>(data);

	for (int i = 0; i < kPaletteBytes / 4; ++i)
	{
		if (!TryWriteDword(reinterpret_cast<uint8_t*>(address) + i * 4, words[i]))
			return false;
	}

	return true;
}
