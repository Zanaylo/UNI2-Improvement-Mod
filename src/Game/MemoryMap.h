// Resolves and validates every game pointer the mod reads. Each accessor is defensive: a failed
// validation disables the feature rather than crashing.

#pragma once

#include <cstdint>

namespace MemoryMap
{
	constexpr int kMaxStackEntries = 32;

	struct CharaStackView
	{
		uintptr_t basePointer;
		uintptr_t topPointer;
		int depth;
		void* entries[kMaxStackEntries];
	};

	bool Initialize();
	bool IsValid();
	const char* GetStatusText();

	bool GetGameVersion(char* buffer, size_t bufferSize);
	bool IsSupportedVersion();

	bool ReadCharaStack(CharaStackView& out);

	void* ResolveOwner(void* charaData);
	void* GetParamBlock(void* charaData);
	bool ReadParam(void* charaData, int index, int& outValue);
	uint32_t ReadContext(void* charaData);

	bool IsPlausibleCharaData(void* charaData);

	enum class EntityKind
	{
		Unknown,
		Player,
		Effect
	};

	EntityKind ClassifyEntity(void* candidate);

	bool IsPlayerData(void* candidate);
	bool IsEffectData(void* candidate);
	int EnumeratePlayerData(void* seed, void** outEntries, int maxEntries);

	constexpr int kEffectCacheMax = 64;

	int EnumerateCharaSlots(void** outEntries, int maxEntries, bool activeOnly);
	int EnumerateEffectSlots(void** outEntries, int maxEntries);

	int EnumerateEffectSlotsCached(void** outEntries, int maxEntries);
	void InvalidateEffectSlotCache();

	void* GetEffectSlot(int index);
	int GetEffectSlotIndex(void* entity);
	void* GetCharaSlot(int index);
	int GetCharaSlotIndex(void* entity);
	bool ReadVTable(void* entity, uintptr_t& outRva);
	bool IsSlotActive(void* entity);
	bool IsSpawnedObject(void* entity);
	int GetObjectType(void* entity);

	bool ReadStructDword(void* base, uintptr_t offset, uint32_t& outValue);
	int FindDwordInStruct(void* base, uint32_t needle, uintptr_t* outOffsets, int maxOffsets);
	int ReadStructString(void* base, uintptr_t offset, char* out, int maxLength);

	struct VectorView
	{
		uintptr_t begin;
		uintptr_t end;
		uintptr_t capacity;
		int count;
	};

	bool ReadVector(void* playerData, VectorView& out);

	bool ReadDwordAt(uintptr_t address, uint32_t& outValue);
}
