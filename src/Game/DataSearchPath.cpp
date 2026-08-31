#include "Game/DataSearchPath.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>

namespace {

char* Slot()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kSearchPathBase);

	if (!IsAddressInGameModule(address))
		return nullptr;

	return reinterpret_cast<char*>(address);
}

unsigned char* Gate()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kSearchPathEnabled);

	if (!IsAddressInGameModule(address))
		return nullptr;

	return reinterpret_cast<unsigned char*>(address);
}

std::string Separated(const std::string& prefix)
{
	if (prefix.empty() || prefix.back() == '\\')
		return prefix;

	return prefix + "\\";
}

bool WriteSlot(const std::string& prefix)
{
	char* const slot = Slot();

	if (slot == nullptr || prefix.size() >= GameOffsets::kSearchPathStride)
		return false;

	DWORD previous = 0;

	if (!VirtualProtect(slot, GameOffsets::kSearchPathStride, PAGE_READWRITE, &previous))
		return false;

	memset(slot, 0, GameOffsets::kSearchPathStride);
	memcpy(slot, prefix.c_str(), prefix.size());
	VirtualProtect(slot, GameOffsets::kSearchPathStride, previous, &previous);
	return true;
}

char g_slotOriginal[GameOffsets::kSearchPathStride] = {};
unsigned char g_gateOriginal = 0;
bool g_originalSaved = false;

void SaveOriginal()
{
	if (g_originalSaved)
		return;

	const char* const slot = Slot();
	const unsigned char* const gate = Gate();

	if (slot == nullptr || gate == nullptr)
		return;

	memcpy(g_slotOriginal, slot, GameOffsets::kSearchPathStride);
	g_gateOriginal = *gate;
	g_originalSaved = true;
}

bool RestoreOriginal()
{
	char* const slot = Slot();

	if (!g_originalSaved || slot == nullptr)
		return false;

	DWORD previous = 0;

	if (!VirtualProtect(slot, GameOffsets::kSearchPathStride, PAGE_READWRITE, &previous))
		return false;

	memcpy(slot, g_slotOriginal, GameOffsets::kSearchPathStride);
	VirtualProtect(slot, GameOffsets::kSearchPathStride, previous, &previous);
	return true;
}

uint8_t g_gateWriteOriginal[2][GameOffsets::kSearchPathGateWriteSize] = {};
bool g_gateWriteSaved = false;
bool g_gateWriteSilenced = false;

void SilenceGateClobber(bool silence)
{
	if (silence == g_gateWriteSilenced)
		return;

	for (int i = 0; i < 2; ++i)
	{
		const uintptr_t address = RvaToAddress(GameOffsets::kSearchPathGateWrite[i]);

		if (!IsAddressInGameModule(address))
			return;

		uint8_t* const site = reinterpret_cast<uint8_t*>(address);
		DWORD previous = 0;

		if (!VirtualProtect(site, GameOffsets::kSearchPathGateWriteSize, PAGE_EXECUTE_READWRITE,
			&previous))
		{
			return;
		}

		if (!g_gateWriteSaved)
			memcpy(g_gateWriteOriginal[i], site, GameOffsets::kSearchPathGateWriteSize);

		if (silence)
			memset(site, 0x90, GameOffsets::kSearchPathGateWriteSize);
		else
			memcpy(site, g_gateWriteOriginal[i], GameOffsets::kSearchPathGateWriteSize);

		VirtualProtect(site, GameOffsets::kSearchPathGateWriteSize, previous, &previous);
	}

	g_gateWriteSaved = true;
	g_gateWriteSilenced = silence;
}

bool WriteGate(bool on)
{
	unsigned char* const gate = Gate();

	if (gate == nullptr)
		return false;

	DWORD previous = 0;

	if (!VirtualProtect(gate, 1, PAGE_READWRITE, &previous))
		return false;

	*gate = on ? 1 : 0;
	VirtualProtect(gate, 1, previous, &previous);
	return true;
}

}

bool DataSearchPath::IsSupported()
{
	return Slot() != nullptr && Gate() != nullptr;
}

bool DataSearchPath::Point(const std::string& prefix)
{
	SaveOriginal();
	SilenceGateClobber(true);
	return WriteSlot(Separated(prefix)) && WriteGate(true);
}

bool DataSearchPath::Release()
{
	SilenceGateClobber(false);

	if (!g_originalSaved)
		return true;

	return RestoreOriginal() && WriteGate(g_gateOriginal != 0);
}

bool DataSearchPath::PointsAt(const std::string& prefix)
{
	const char* const slot = Slot();
	const unsigned char* const gate = Gate();

	if (slot == nullptr || gate == nullptr)
		return false;

	return *gate != 0 && _stricmp(slot, Separated(prefix).c_str()) == 0;
}
