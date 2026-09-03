#include "Game/DataSearchPath.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>

namespace {

constexpr int kPatchSlot = 0;

char* SlotAt(int index)
{
	if (index < 0 || index >= GameOffsets::kSearchPathCount)
		return nullptr;

	const uintptr_t address = RvaToAddress(GameOffsets::kSearchPathBase +
		index * GameOffsets::kSearchPathStride);

	if (!IsAddressInGameModule(address))
		return nullptr;

	return reinterpret_cast<char*>(address);
}

char* Slot()
{
	return SlotAt(kPatchSlot);
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

bool WriteSlotAt(int index, const std::string& prefix)
{
	char* const slot = SlotAt(index);

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

std::string g_overridePrefix;
int g_overrideSlot = -1;

int FreeSlot()
{
	for (int index = GameOffsets::kSearchPathCount - 1; index > kPatchSlot; --index)
	{
		const char* const slot = SlotAt(index);

		if (slot != nullptr && slot[0] == '\0')
			return index;
	}

	return -1;
}

uint8_t g_gateWriteOriginal[2][GameOffsets::kSearchPathGateWriteSize] = {};
bool g_gateWriteSaved = false;
bool g_gateWriteSilenced = false;

constexpr unsigned kOwnerPatch = 1;
constexpr unsigned kOwnerOverrides = 2;

unsigned g_silenceOwners = 0;

bool ClobberSitesAreKnown()
{
	const uintptr_t gate = RvaToAddress(GameOffsets::kSearchPathEnabled);

	uint8_t expected[GameOffsets::kSearchPathGateWriteSize] = { 0xa2 };
	const uint32_t address = static_cast<uint32_t>(gate);
	memcpy(expected + 1, &address, sizeof(address));

	for (const uintptr_t rva : GameOffsets::kSearchPathGateWrite)
	{
		const uintptr_t site = RvaToAddress(rva);

		if (!IsAddressInGameModule(site))
			return false;

		if (memcmp(reinterpret_cast<const void*>(site), expected, sizeof(expected)) != 0)
			return false;
	}

	return true;
}

void SilenceGateClobber(bool silence)
{
	if (silence == g_gateWriteSilenced)
		return;

	if (silence && !g_gateWriteSaved && !ClobberSitesAreKnown())
	{
		LOG("DataSearchPath: the gate writes are not where this build was measured; leaving the "
			"code alone");
		return;
	}

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

void ClaimSilence(unsigned owner, bool claim)
{
	if (claim)
		g_silenceOwners |= owner;
	else
		g_silenceOwners &= ~owner;

	SilenceGateClobber(g_silenceOwners != 0);
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
	ClaimSilence(kOwnerPatch, true);
	return WriteSlotAt(kPatchSlot, Separated(prefix)) && WriteGate(true);
}

bool DataSearchPath::Release()
{
	ClaimSilence(kOwnerPatch, false);

	if (!g_originalSaved)
		return true;

	return RestoreOriginal() && WriteGate(g_gateOriginal != 0 || !g_overridePrefix.empty());
}

bool DataSearchPath::PointOverrides(const std::string& prefix)
{
	const std::string separated = Separated(prefix);

	if (separated.empty())
		return false;

	if (g_overridePrefix == separated && g_overrideSlot >= 0)
		return true;

	const int slot = g_overrideSlot >= 0 ? g_overrideSlot : FreeSlot();

	if (slot < 0)
	{
		LOG("DataSearchPath: every search path slot is the game's; leaving them alone");
		return false;
	}

	g_overrideSlot = slot;
	g_overridePrefix = separated;
	ClaimSilence(kOwnerOverrides, true);

	const bool ok = WriteSlotAt(slot, separated) && WriteGate(true);

	LOG("DataSearchPath: slot %d now looks in %s for an overridden file", slot, separated.c_str());

	return ok;
}

bool DataSearchPath::ReleaseOverrides()
{
	if (g_overridePrefix.empty() || g_overrideSlot < 0)
		return true;

	const int slot = g_overrideSlot;

	g_overridePrefix.clear();
	g_overrideSlot = -1;
	ClaimSilence(kOwnerOverrides, false);

	return WriteSlotAt(slot, std::string());
}

void DataSearchPath::Assert()
{
	if (g_overridePrefix.empty() || g_overrideSlot < 0)
		return;

	const char* const slot = SlotAt(g_overrideSlot);
	const unsigned char* const gate = Gate();

	if (slot == nullptr || gate == nullptr)
		return;

	if (_stricmp(slot, g_overridePrefix.c_str()) != 0)
		WriteSlotAt(g_overrideSlot, g_overridePrefix);

	if (*gate == 0)
		WriteGate(true);
}

void DataSearchPath::LogSlots(const char* when)
{
	const unsigned char* const gate = Gate();

	for (int index = 0; index < GameOffsets::kSearchPathCount; ++index)
	{
		const char* const slot = SlotAt(index);

		if (slot == nullptr)
			continue;

		LOG("DataSearchPath: %s, slot %d = '%s'", when, index, slot);
	}

	if (gate != nullptr)
		LOG("DataSearchPath: %s, the gate is %d", when, static_cast<int>(*gate));
}

bool DataSearchPath::PointsAt(const std::string& prefix)
{
	const char* const slot = Slot();
	const unsigned char* const gate = Gate();

	if (slot == nullptr || gate == nullptr)
		return false;

	return *gate != 0 && _stricmp(slot, Separated(prefix).c_str()) == 0;
}
