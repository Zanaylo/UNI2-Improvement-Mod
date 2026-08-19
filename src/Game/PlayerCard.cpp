#include "Game/PlayerCard.h"

#include "Core/TextEncoding.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

struct LayerBinding
{
	const char* name;
	const char* assetPrefix;
	uintptr_t equippedRva;
	uintptr_t ownedListRva;
	uintptr_t ownedCountRva;
};

constexpr LayerBinding kLayers[PlayerCard::kLayerCount] = {
	{ "Frame", "fr", GameOffsets::kCardPlateFrame, GameOffsets::kOwnedFrameList,
		GameOffsets::kOwnedFrameCount },
	{ "Front panel", "pa", GameOffsets::kCardPlatePanel, GameOffsets::kOwnedPanelList,
		GameOffsets::kOwnedPanelCount },
	{ "Character", "ch", GameOffsets::kCardPlateChara, GameOffsets::kOwnedCharaList,
		GameOffsets::kOwnedCharaCount },
	{ "Base", "ba", GameOffsets::kCardPlateBase, GameOffsets::kOwnedBaseList,
		GameOffsets::kOwnedBaseCount },
};

const LayerBinding* Binding(PlayerCard::PlateLayer layer)
{
	const int index = static_cast<int>(layer);
	if (index < 0 || index >= PlayerCard::kLayerCount)
		return nullptr;

	return &kLayers[index];
}

void* Address(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);
	if (address == 0)
		return nullptr;

	return reinterpret_cast<void*>(address);
}

bool ReadField(uintptr_t rva, uint32_t& outValue)
{
	const void* const source = Address(rva);
	if (source == nullptr)
		return false;

	return TryReadUnaligned(source, outValue);
}

bool WriteField(uintptr_t rva, uint32_t value)
{
	void* const target = Address(rva);
	if (target == nullptr)
		return false;

	return TryWriteUnaligned(target, value);
}

bool WriteBuffer(uintptr_t rva, const void* source, size_t size)
{
	void* const target = Address(rva);
	if (target == nullptr)
		return false;

	return TryWriteMemory(target, source, size);
}

bool ReadOwnedCount(const LayerBinding& binding, int& outCount)
{
	uint32_t raw = 0;
	if (!ReadField(binding.ownedCountRva, raw))
		return false;

	if (raw > static_cast<uint32_t>(GameOffsets::kOwnedMax))
		return false;

	outCount = static_cast<int>(raw);
	return true;
}

bool WriteTitleText(const std::string& shiftJis)
{
	char buffer[GameOffsets::kCardTitleTextBytes] = {};
	memcpy(buffer, shiftJis.data(), shiftJis.size());

	return WriteBuffer(GameOffsets::kCardTitleText, buffer, sizeof(buffer));
}

bool WriteTitleWords(const std::string& shiftJis)
{
	constexpr size_t kSlotBytes = GameOffsets::kTitleWordStride;
	constexpr size_t kWordBytes = kSlotBytes - 1;

	char buffer[kSlotBytes * GameOffsets::kTitleWordCount] = {};

	size_t offset = 0;

	for (int slot = 0; slot < GameOffsets::kTitleWordCount && offset < shiftJis.size(); ++slot)
	{
		const std::string rest = shiftJis.substr(offset);
		const size_t take = TextEncoding::ShiftJisBoundary(rest, kWordBytes);
		if (take == 0)
			break;

		memcpy(buffer + slot * kSlotBytes, rest.data(), take);
		offset += take;
	}

	return WriteBuffer(GameOffsets::kTitleWords, buffer, sizeof(buffer));
}

}

bool PlayerCard::IsAvailable()
{
	const void* const block = Address(GameOffsets::kPlayerCardBlock);
	if (block == nullptr)
		return false;

	return IsReadableMemory(block, GameOffsets::kPlayerCardBlockSize);
}

const char* PlayerCard::LayerName(PlateLayer layer)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return "";

	return binding->name;
}

const char* PlayerCard::LayerAssetPrefix(PlateLayer layer)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return "";

	return binding->assetPrefix;
}

bool PlayerCard::GetPlate(PlateLayer layer, int& outId)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return false;

	uint32_t value = 0;
	if (!ReadField(binding->equippedRva, value))
		return false;

	outId = static_cast<int>(value);
	return true;
}

bool PlayerCard::SetPlate(PlateLayer layer, int id)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return false;

	if (id < 0)
		return false;

	if (!WriteField(binding->equippedRva, static_cast<uint32_t>(id)))
		return false;

	MarkChanged();
	return true;
}

bool PlayerCard::EquipPlate(PlateLayer layer, int id)
{
	if (!SetPlate(layer, id))
		return false;

	AddOwned(layer, id);
	return true;
}

int PlayerCard::GetOwnedCount(PlateLayer layer)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return 0;

	int count = 0;
	if (!ReadOwnedCount(*binding, count))
		return 0;

	return count;
}

bool PlayerCard::IsOwned(PlateLayer layer, int id)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return false;

	int count = 0;
	if (!ReadOwnedCount(*binding, count))
		return false;

	for (int i = 0; i < count; ++i)
	{
		uint32_t value = 0;
		if (!ReadField(binding->ownedListRva + i * sizeof(uint32_t), value))
			return false;

		if (static_cast<int>(value) == id)
			return true;
	}

	return false;
}

bool PlayerCard::AddOwned(PlateLayer layer, int id)
{
	const LayerBinding* const binding = Binding(layer);
	if (binding == nullptr)
		return false;

	if (IsOwned(layer, id))
		return true;

	int count = 0;
	if (!ReadOwnedCount(*binding, count))
		return false;

	if (count >= GameOffsets::kOwnedMax)
		return false;

	if (!WriteField(binding->ownedListRva + count * sizeof(uint32_t), static_cast<uint32_t>(id)))
		return false;

	if (!WriteField(binding->ownedCountRva, static_cast<uint32_t>(count + 1)))
		return false;

	MarkChanged();
	return true;
}

bool PlayerCard::GetTitle(std::string& outUtf8)
{
	char buffer[GameOffsets::kCardTitleTextBytes] = {};

	const void* const source = Address(GameOffsets::kCardTitleText);
	if (source == nullptr)
		return false;

	if (!TryReadMemory(buffer, source, sizeof(buffer)))
		return false;

	return TextEncoding::ShiftJisToUtf8(buffer, sizeof(buffer) - 1, outUtf8);
}

bool PlayerCard::SetTitle(const std::string& utf8, bool* outTruncated, bool* outLossy)
{
	if (outTruncated != nullptr)
		*outTruncated = false;

	std::string encoded;
	if (!TextEncoding::Utf8ToShiftJis(utf8, encoded, outLossy))
		return false;

	const size_t fits = TextEncoding::ShiftJisBoundary(encoded, kTitleMaxBytes);
	if (fits < encoded.size())
	{
		encoded.resize(fits);

		if (outTruncated != nullptr)
			*outTruncated = true;
	}

	if (!WriteField(GameOffsets::kCardTitleId, 0))
		return false;

	if (!WriteTitleText(encoded))
		return false;

	if (!WriteTitleWords(encoded))
		return false;

	MarkChanged();
	return true;
}

bool PlayerCard::GetIp(int& outIp)
{
	uint32_t value = 0;
	if (!ReadField(GameOffsets::kCardIp, value))
		return false;

	outIp = static_cast<int>(value);
	return true;
}

void PlayerCard::MarkChanged()
{
	const uint8_t needed = 1;
	WriteBuffer(GameOffsets::kSaveNeededFlag, &needed, sizeof(needed));
}
