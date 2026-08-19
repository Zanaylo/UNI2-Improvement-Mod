#pragma once

#include <cstdint>

namespace PaletteMemory
{
	constexpr int kColorsPerPalette = 256;
	constexpr int kPaletteBytes = kColorsPerPalette * 4;

	bool Install();
	bool IsInstalled();

	int Scan();

	int ProbeFromCharacters();

	uintptr_t GetPlayerPaletteAddress(int player);
	uintptr_t GetPlayerCurrentPalette(int player);
	uintptr_t GetPlayerPaletteTable(int player);

	bool ReadPlayerPalette(int player, uint8_t* out);

	bool ReadPlayerPaletteAt(int player, int index, uint8_t* out);

	int FindCopiesOfPlayerPalette(int player);

	int WritePlayerPalette(int player, const uint8_t* data);

	int GetLoadCount();
	int GetLoaderCallCount();

	void DescribeResolution(int player, char* out, int size);
	const char* GetLoadName(int index);
	uintptr_t GetLoadAddress(int index);
	int GetLoadPaletteCount(int index);
	void ForgetLoads();

	constexpr int kLoadBattle = 0;
	constexpr int kLoadCharSelect = 1;
	constexpr int kLoadColorEdit = 2;
	constexpr int kLoadLobbyAvatar = 3;
	int GetLoadChara(int index);
	int GetLoadKind(int index);
	int GetLoadFrame(int index);

	int FindLoadForPlayer(int player);

	int GetCharaNumber(int player);

	int GetPlayerSlot(int player);
	bool SetPlayerSlot(int player, int slot);

	bool ReadPalette(int load, int index, uint8_t* out);
	bool WritePalette(int load, int index, const uint8_t* data);
}
