#include "Game/MemoryMap.h"

#include "Core/Profiler.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>
#include <cstring>

namespace {

constexpr uintptr_t kStackEntryStride = 8;

bool g_initialized = false;
bool g_valid = false;
bool g_versionMatches = false;
char g_status[192] = "not initialized";

uintptr_t g_charaStackBase = 0;
uintptr_t g_charaStackTop = 0;

thread_local void* t_effectCache[MemoryMap::kEffectCacheMax] = {};
thread_local int t_effectCacheCount = 0;
thread_local bool t_effectCacheValid = false;

void SetStatus(const char* text)
{
	strncpy_s(g_status, text, _TRUNCATE);
	LOG("MemoryMap: %s", text);
}

template <typename T>
bool SafeRead(uintptr_t address, T& out)
{
	return TryReadMemory(&out, reinterpret_cast<const void*>(address), sizeof(T));
}

}

bool MemoryMap::Initialize()
{
	if (g_initialized)
		return g_valid;

	g_initialized = true;

	if (GetGameBaseAddress() == 0)
	{
		SetStatus("game module base could not be resolved");
		return false;
	}

	g_charaStackBase = RvaToAddress(GameOffsets::kCharaStackBase);
	g_charaStackTop = RvaToAddress(GameOffsets::kCharaStackTop);

	if (!IsReadableMemory(reinterpret_cast<void*>(g_charaStackBase), sizeof(void*)) ||
		!IsReadableMemory(reinterpret_cast<void*>(g_charaStackTop), sizeof(void*)))
	{
		SetStatus("chara data stack globals are not readable");
		return false;
	}

	char version[32] = {};
	if (GetGameVersion(version, sizeof(version)))
	{
		g_versionMatches = strcmp(version, UNI2_IM_SUPPORTED_GAME_VERSION) == 0;
		if (!g_versionMatches)
		{
			char message[192] = {};
			sprintf_s(message, "game reports %s but offsets target %s; features disabled",
				version, UNI2_IM_SUPPORTED_GAME_VERSION);
			SetStatus(message);
			return false;
		}
	}
	else
	{
		SetStatus("game version string not readable at the expected address; features disabled");
		return false;
	}

	g_valid = true;
	SetStatus("ok");
	return true;
}

bool MemoryMap::IsValid()
{
	return g_valid;
}

const char* MemoryMap::GetStatusText()
{
	return g_status;
}

bool MemoryMap::IsSupportedVersion()
{
	return g_versionMatches;
}

bool MemoryMap::GetGameVersion(char* buffer, size_t bufferSize)
{
	if (buffer == nullptr || bufferSize == 0)
		return false;

	const uintptr_t address = RvaToAddress(GameOffsets::kVersionString);
	char source[32] = {};
	if (!TryReadMemory(source, reinterpret_cast<const void*>(address), sizeof(source)))
		return false;

	size_t length = 0;
	while (length < bufferSize - 1 && length < 31 && source[length] >= 0x20 && source[length] < 0x7f)
	{
		buffer[length] = source[length];
		++length;
	}

	buffer[length] = '\0';
	return length > 0;
}

bool MemoryMap::ReadCharaStack(CharaStackView& out)
{
	memset(&out, 0, sizeof(out));

	if (!g_valid)
		return false;

	uintptr_t base = 0;
	uintptr_t top = 0;
	if (!SafeRead(g_charaStackBase, base) || !SafeRead(g_charaStackTop, top))
		return false;

	out.basePointer = base;
	out.topPointer = top;

	if (base == 0 || top < base)
		return true;

	const size_t byteCount = top - base;
	if (byteCount % kStackEntryStride != 0 || byteCount > kStackEntryStride * 4096)
		return true;

	int depth = static_cast<int>(byteCount / kStackEntryStride);
	out.depth = depth;

	const int toRead = depth < kMaxStackEntries ? depth : kMaxStackEntries;
	for (int i = 0; i < toRead; ++i)
	{
		const uintptr_t slot = top - kStackEntryStride * (i + 1);
		void* value = nullptr;
		if (SafeRead(slot, value))
			out.entries[i] = value;
	}

	return true;
}

bool MemoryMap::IsPlausibleCharaData(void* charaData)
{
	if (charaData == nullptr)
		return false;

	const uintptr_t address = reinterpret_cast<uintptr_t>(charaData);
	if (address < 0x10000)
		return false;

	uint32_t probe = 0;
	return SafeRead(address + GameOffsets::kCharaParamBlock, probe);
}

void* MemoryMap::ResolveOwner(void* charaData)
{
	if (!IsPlausibleCharaData(charaData))
		return nullptr;

	void* owner = nullptr;
	if (!SafeRead(reinterpret_cast<uintptr_t>(charaData) + GameOffsets::kCharaOwner, owner))
		return charaData;

	if (owner != nullptr && IsPlausibleCharaData(owner))
		return owner;

	return charaData;
}

void* MemoryMap::GetParamBlock(void* charaData)
{
	void* resolved = ResolveOwner(charaData);
	if (resolved == nullptr)
		return nullptr;

	void* paramBlock = nullptr;
	if (!SafeRead(reinterpret_cast<uintptr_t>(resolved) + GameOffsets::kCharaParamBlock, paramBlock))
		return nullptr;

	if (paramBlock == nullptr)
		return nullptr;

	uint32_t probe = 0;
	if (!SafeRead(reinterpret_cast<uintptr_t>(paramBlock) + GameOffsets::kParamArray, probe))
		return nullptr;

	return paramBlock;
}

bool MemoryMap::ReadParam(void* charaData, int index, int& outValue)
{
	if (index < 0)
		return false;

	void* paramBlock = GetParamBlock(charaData);
	if (paramBlock == nullptr)
		return false;

	const uintptr_t address = reinterpret_cast<uintptr_t>(paramBlock) +
		GameOffsets::kParamArray + static_cast<uintptr_t>(index) * sizeof(int);

	return SafeRead(address, outValue);
}

MemoryMap::EntityKind MemoryMap::ClassifyEntity(void* candidate)
{
	if (candidate == nullptr)
		return EntityKind::Unknown;

	const uintptr_t address = reinterpret_cast<uintptr_t>(candidate);
	if (address < 0x10000 || (address & 3) != 0)
		return EntityKind::Unknown;

	uintptr_t vtable = 0;
	if (!SafeRead(address, vtable))
		return EntityKind::Unknown;

	if (vtable == RvaToAddress(GameOffsets::kPlayerDataVTable))
	{
		uint32_t tail = 0;
		if (!SafeRead(address + GameOffsets::kPlayerDataSize - sizeof(uint32_t), tail))
			return EntityKind::Unknown;

		return EntityKind::Player;
	}

	if (vtable == RvaToAddress(GameOffsets::kEffectVTable))
	{

		uint32_t owner = 0;
		if (!SafeRead(address + GameOffsets::kCharaOwner, owner))
			return EntityKind::Unknown;

		return EntityKind::Effect;
	}

	return EntityKind::Unknown;
}

bool MemoryMap::IsEffectData(void* candidate)
{
	return ClassifyEntity(candidate) == EntityKind::Effect;
}

bool MemoryMap::IsPlayerData(void* candidate)
{
	if (candidate == nullptr)
		return false;

	const uintptr_t address = reinterpret_cast<uintptr_t>(candidate);
	if (address < 0x10000 || (address & 3) != 0)
		return false;

	uintptr_t vtable = 0;
	if (!SafeRead(address, vtable))
		return false;

	if (vtable != RvaToAddress(GameOffsets::kPlayerDataVTable))
		return false;

	uint32_t tail = 0;
	return SafeRead(address + GameOffsets::kPlayerDataSize - sizeof(uint32_t), tail);
}

int MemoryMap::EnumeratePlayerData(void* seed, void** outEntries, int maxEntries)
{
	if (outEntries == nullptr || maxEntries <= 0 || !IsPlayerData(seed))
		return 0;

	const uintptr_t stride = GameOffsets::kPlayerDataSize;
	uintptr_t first = reinterpret_cast<uintptr_t>(seed);

	for (int i = 0; i < maxEntries; ++i)
	{
		const uintptr_t previous = first - stride;
		if (!IsPlayerData(reinterpret_cast<void*>(previous)))
			break;

		first = previous;
	}

	int count = 0;
	for (uintptr_t current = first; count < maxEntries; current += stride)
	{
		if (!IsPlayerData(reinterpret_cast<void*>(current)))
			break;

		outEntries[count++] = reinterpret_cast<void*>(current);
	}

	return count;
}

int MemoryMap::EnumerateCharaSlots(void** outEntries, int maxEntries, bool activeOnly)
{
	if (outEntries == nullptr || maxEntries <= 0)
		return 0;

	const uintptr_t base = RvaToAddress(GameOffsets::kCharaArrayBase);
	if (base == 0)
		return 0;

	int count = 0;
	for (int i = 0; i < GameOffsets::kCharaArrayCount && count < maxEntries; ++i)
	{
		void* slot = reinterpret_cast<void*>(base + static_cast<uintptr_t>(i) * GameOffsets::kPlayerDataSize);

		if (!IsPlayerData(slot))
			continue;

		if (activeOnly && !IsSlotActive(slot))
			continue;

		outEntries[count++] = slot;
	}

	return count;
}

void* MemoryMap::GetEffectSlot(int index)
{
	if (index < 0 || index >= GameOffsets::kEffectArrayCount)
		return nullptr;

	const uintptr_t base = RvaToAddress(GameOffsets::kEffectArrayBase);
	if (base == 0)
		return nullptr;

	return reinterpret_cast<void*>(base + static_cast<uintptr_t>(index) * GameOffsets::kEffectArrayStride);
}

int MemoryMap::GetEffectSlotIndex(void* entity)
{
	const uintptr_t base = RvaToAddress(GameOffsets::kEffectArrayBase);
	if (entity == nullptr || base == 0)
		return -1;

	const uintptr_t address = reinterpret_cast<uintptr_t>(entity);
	if (address < base)
		return -1;

	const uintptr_t delta = address - base;
	if (delta % GameOffsets::kEffectArrayStride != 0)
		return -1;

	const uintptr_t index = delta / GameOffsets::kEffectArrayStride;
	if (index >= static_cast<uintptr_t>(GameOffsets::kEffectArrayCount))
		return -1;

	return static_cast<int>(index);
}

int MemoryMap::EnumerateEffectSlots(void** outEntries, int maxEntries)
{
	if (outEntries == nullptr || maxEntries <= 0)
		return 0;

	const uintptr_t base = RvaToAddress(GameOffsets::kEffectArrayBase);
	if (base == 0)
		return 0;

	const uintptr_t last = base + static_cast<uintptr_t>(GameOffsets::kEffectArrayCount - 1) *
		GameOffsets::kEffectArrayStride;

	uint8_t probe = 0;
	if (!SafeRead(base, probe) || !SafeRead(last + GameOffsets::kCharaSlotActive, probe))
		return 0;

	const uintptr_t effectVTable = RvaToAddress(GameOffsets::kEffectVTable);
	int count = 0;

	__try
	{
		for (int i = 0; i < GameOffsets::kEffectArrayCount && count < maxEntries; ++i)
		{
			const uintptr_t slot = base + static_cast<uintptr_t>(i) * GameOffsets::kEffectArrayStride;

			if (*reinterpret_cast<const uint8_t*>(slot + GameOffsets::kCharaSlotActive) == 0)
				continue;

			if (*reinterpret_cast<const uintptr_t*>(slot) != effectVTable)
				continue;

			outEntries[count++] = reinterpret_cast<void*>(slot);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return count;
}

int MemoryMap::EnumerateEffectSlotsCached(void** outEntries, int maxEntries)
{
	if (outEntries == nullptr || maxEntries <= 0)
		return 0;

	if (maxEntries > kEffectCacheMax)
		return EnumerateEffectSlots(outEntries, maxEntries);

	if (!t_effectCacheValid)
	{
		Profiler::Scope scope(Profiler::Section_TickEffectScan);
		t_effectCacheCount = EnumerateEffectSlots(t_effectCache, kEffectCacheMax);
		t_effectCacheValid = true;
	}

	const int count = t_effectCacheCount < maxEntries ? t_effectCacheCount : maxEntries;

	for (int i = 0; i < count; ++i)
		outEntries[i] = t_effectCache[i];

	return count;
}

void MemoryMap::InvalidateEffectSlotCache()
{
	t_effectCacheValid = false;
}

void* MemoryMap::GetCharaSlot(int index)
{
	if (index < 0 || index >= GameOffsets::kCharaArrayCount)
		return nullptr;

	const uintptr_t base = RvaToAddress(GameOffsets::kCharaArrayBase);
	if (base == 0)
		return nullptr;

	return reinterpret_cast<void*>(base + static_cast<uintptr_t>(index) * GameOffsets::kPlayerDataSize);
}

int MemoryMap::GetCharaSlotIndex(void* entity)
{
	const uintptr_t base = RvaToAddress(GameOffsets::kCharaArrayBase);
	if (entity == nullptr || base == 0)
		return -1;

	const uintptr_t address = reinterpret_cast<uintptr_t>(entity);
	if (address < base)
		return -1;

	const uintptr_t delta = address - base;
	if (delta % GameOffsets::kPlayerDataSize != 0)
		return -1;

	const uintptr_t index = delta / GameOffsets::kPlayerDataSize;
	if (index >= static_cast<uintptr_t>(GameOffsets::kCharaArrayCount))
		return -1;

	return static_cast<int>(index);
}

bool MemoryMap::ReadVTable(void* entity, uintptr_t& outRva)
{
	outRva = 0;

	if (entity == nullptr)
		return false;

	uintptr_t vtable = 0;
	if (!SafeRead(reinterpret_cast<uintptr_t>(entity), vtable))
		return false;

	const uintptr_t base = GetGameBaseAddress();
	outRva = vtable > base ? vtable - base : vtable;
	return true;
}

bool MemoryMap::IsSlotActive(void* entity)
{
	if (entity == nullptr)
		return false;

	uint8_t active = 0;
	if (!SafeRead(reinterpret_cast<uintptr_t>(entity) + GameOffsets::kCharaSlotActive, active))
		return false;

	return active != 0;
}

bool MemoryMap::IsSpawnedObject(void* entity)
{
	if (entity == nullptr)
		return false;

	uint32_t objectId = 0;
	if (!SafeRead(reinterpret_cast<uintptr_t>(entity) + GameOffsets::kCharaObjectId, objectId))
		return false;

	return objectId != 0;
}

int MemoryMap::GetObjectType(void* entity)
{
	if (!IsSpawnedObject(entity))
		return 0;

	uint32_t type = 0;
	if (!SafeRead(reinterpret_cast<uintptr_t>(entity) + GameOffsets::kCharaObjectType, type))
		return 0;

	return static_cast<int>(type);
}

bool MemoryMap::ReadStructDword(void* base, uintptr_t offset, uint32_t& outValue)
{
	if (base == nullptr || offset + sizeof(uint32_t) > GameOffsets::kPlayerDataSize)
		return false;

	return SafeRead(reinterpret_cast<uintptr_t>(base) + offset, outValue);
}

int MemoryMap::FindDwordInStruct(void* base, uint32_t needle, uintptr_t* outOffsets, int maxOffsets)
{
	if (base == nullptr || outOffsets == nullptr || maxOffsets <= 0)
		return 0;

	int found = 0;
	for (uintptr_t offset = 0; offset + sizeof(uint32_t) <= GameOffsets::kPlayerDataSize;
		offset += sizeof(uint32_t))
	{
		uint32_t value = 0;
		if (!SafeRead(reinterpret_cast<uintptr_t>(base) + offset, value))
			break;

		if (value != needle)
			continue;

		outOffsets[found++] = offset;
		if (found >= maxOffsets)
			break;
	}

	return found;
}

int MemoryMap::ReadStructString(void* base, uintptr_t offset, char* out, int maxLength)
{
	if (base == nullptr || out == nullptr || maxLength <= 1)
		return 0;

	int length = 0;
	while (length < maxLength - 1 && offset + length < GameOffsets::kPlayerDataSize)
	{
		uint8_t c = 0;
		if (!SafeRead(reinterpret_cast<uintptr_t>(base) + offset + length, c))
			break;

		if (c == 0)
			break;

		if (c < 0x20 || c >= 0x7f)
			return 0;

		out[length++] = static_cast<char>(c);
	}

	out[length] = '\0';
	return length;
}

bool MemoryMap::ReadVector(void* playerData, VectorView& out)
{
	memset(&out, 0, sizeof(out));

	if (!IsPlayerData(playerData))
		return false;

	const uintptr_t base = reinterpret_cast<uintptr_t>(playerData);

	if (!SafeRead(base + GameOffsets::kPlayerDataVectorBegin, out.begin) ||
		!SafeRead(base + GameOffsets::kPlayerDataVectorEnd, out.end) ||
		!SafeRead(base + GameOffsets::kPlayerDataVectorCapacity, out.capacity))
	{
		return false;
	}

	if (out.begin == 0 || out.end < out.begin)
		return true;

	const uintptr_t bytes = out.end - out.begin;
	if (bytes % GameOffsets::kPlayerDataVectorStride == 0 &&
		bytes <= GameOffsets::kPlayerDataVectorStride * 4096)
	{
		out.count = static_cast<int>(bytes / GameOffsets::kPlayerDataVectorStride);
	}

	return true;
}

bool MemoryMap::ReadDwordAt(uintptr_t address, uint32_t& outValue)
{
	if (address < 0x10000 || (address & 3) != 0)
		return false;

	return SafeRead(address, outValue);
}

uint32_t MemoryMap::ReadContext(void* charaData)
{
	if (!IsPlausibleCharaData(charaData))
		return 0;

	uint32_t value = 0;
	SafeRead(reinterpret_cast<uintptr_t>(charaData) + GameOffsets::kCharaFrameObject, value);
	return value;
}
