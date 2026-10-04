#include "Game/Stages/Bbtag/BbtagDds.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr size_t kHeaderBytes = 128;
constexpr uint32_t kHeaderSize = 124;
constexpr uint32_t kRequiredFlags = 0x1 | 0x2 | 0x4 | 0x1000;
constexpr uint32_t kPitchFlag = 0x8;
constexpr uint32_t kLinearSizeFlag = 0x80000;
constexpr uint32_t kFourCcFlag = 0x4;
constexpr uint32_t kSmallestLevel = 4;
constexpr uint32_t kBitsPerByte = 8;
constexpr uint32_t kBlockSide = 4;
constexpr uint32_t kSmallBlockBytes = 8;
constexpr uint32_t kLargeBlockBytes = 16;

constexpr size_t kSizeAt = 4;
constexpr size_t kFlagsAt = 8;
constexpr size_t kHeightAt = 12;
constexpr size_t kWidthAt = 16;
constexpr size_t kPitchAt = 20;
constexpr size_t kFormatFlagsAt = 80;
constexpr size_t kFourCcAt = 84;
constexpr size_t kBitsAt = 88;

uint32_t Read(const std::vector<uint8_t>& dds, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, dds.data() + at, sizeof(value));

	return value;
}

void Write(std::vector<uint8_t>& dds, size_t at, uint32_t value)
{
	memcpy(dds.data() + at, &value, sizeof(value));
}

bool Parsed(const std::vector<uint8_t>& dds)
{
	return dds.size() >= kHeaderBytes && memcmp(dds.data(), "DDS ", 4) == 0
		&& Read(dds, kSizeAt) == kHeaderSize
		&& (Read(dds, kFlagsAt) & kRequiredFlags) == kRequiredFlags;
}

bool Pitched(const std::vector<uint8_t>& dds)
{
	return (Read(dds, kFlagsAt) & kPitchFlag) != 0;
}

uint32_t BlockBytes(const std::vector<uint8_t>& dds)
{
	if ((Read(dds, kFormatFlagsAt) & kFourCcFlag) == 0)
		return 0;

	if (memcmp(dds.data() + kFourCcAt, "DXT1", 4) == 0)
		return kSmallBlockBytes;

	if (memcmp(dds.data() + kFourCcAt, "DXT", 3) == 0)
		return kLargeBlockBytes;

	return 0;
}

uint32_t Blocks(uint32_t side)
{
	return std::max(1u, (side + kBlockSide - 1) / kBlockSide);
}

}

uint32_t BbtagDds::FirstLevelBytes(const std::vector<uint8_t>& dds)
{
	if (!Parsed(dds))
		return 0;

	const uint32_t height = Read(dds, kHeightAt);

	if (Pitched(dds))
		return Read(dds, kPitchAt) * height;

	const uint32_t bits = Read(dds, kBitsAt);

	if (bits == 0)
		return Read(dds, kPitchAt);

	return std::max(kSmallestLevel, bits / kBitsPerByte * Read(dds, kWidthAt) * height);
}

void BbtagDds::StateLinearSize(std::vector<uint8_t>& dds)
{
	if (!Parsed(dds) || Pitched(dds) || Read(dds, kBitsAt) != 0)
		return;

	const uint32_t blockBytes = BlockBytes(dds);

	if (blockBytes == 0)
		return;

	Write(dds, kFlagsAt, Read(dds, kFlagsAt) | kLinearSizeFlag);
	Write(dds, kPitchAt, Blocks(Read(dds, kWidthAt)) * Blocks(Read(dds, kHeightAt)) * blockBytes);
}
