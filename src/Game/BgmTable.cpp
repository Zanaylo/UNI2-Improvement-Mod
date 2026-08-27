#include "Game/BgmTable.h"

#include "Game/GameOffsets.h"
#include "Core/utils.h"

#include <cstring>

namespace {

uintptr_t RecordAddress(int id)
{
	if (id < 0 || id >= BgmTable::kSlotCount)
		return 0;

	return RvaToAddress(GameOffsets::kBgmTableBase) + id * GameOffsets::kBgmTableStride;
}

struct SlotName
{
	int first;
	int last;
	const char* text;
};
//Foi o que deu para achar.
constexpr SlotName kSlotNames[] = {
	{ 1, 27, "Character battle theme" },
	{ 40, 40, "Main menu" },
	{ 41, 41, "Network menu" },
	{ 50, 58, "Story conversation" },
	{ 60, 62, "Opening" },
	{ 70, 71, "Ending" },
	{ 82, 82, "Continue" },
	{ 83, 83, "Game over" },
	{ 84, 84, "VS screen" },
	{ 91, 93, "Matchup theme" },
	{ 98, 98, "Win demo" },
	{ 99, 99, "Character select" },
	{ 100, 139, "Story talk (reserved)" },
	{ 140, 199, "Free slot" },
};

}

bool BgmTable::Read(int id, Entry& out)
{
	memset(&out, 0, sizeof(out));

	const uintptr_t record = RecordAddress(id);
	if (record == 0)
		return false;

	uint32_t present = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(record + GameOffsets::kBgmPresent), present))
		return false;

	out.present = present != 0;

	uint8_t loops = 0;
	if (TryReadMemory(&loops, reinterpret_cast<const void*>(record + GameOffsets::kBgmIsLoop),
		sizeof(loops)))
	{
		out.loops = loops != 0;
	}

	TryReadMemory(&out.loopPos, reinterpret_cast<const void*>(record + GameOffsets::kBgmLoopPos),
		sizeof(out.loopPos));

	uint32_t volume = 0;
	if (TryReadDword(reinterpret_cast<const void*>(record + GameOffsets::kBgmVolume), volume))
		out.volume = static_cast<int>(volume);

	uint32_t noRecording = 0;
	if (TryReadDword(reinterpret_cast<const void*>(record + GameOffsets::kBgmNoRecording),
		noRecording))
	{
		out.noRecording = static_cast<int>(noRecording);
	}

	char file[GameOffsets::kBgmFileMax] = {};
	if (TryReadMemory(file, reinterpret_cast<const void*>(record + GameOffsets::kBgmFile),
		sizeof(file)))
	{
		memcpy(out.file, file, sizeof(file));
		out.file[GameOffsets::kBgmFileMax] = '\0';
	}

	return true;
}

bool BgmTable::Bind(int id, const Entry& entry)
{
	const uintptr_t record = RecordAddress(id);

	if (record == 0)
		return false;

	uint32_t source = 0;

	for (int donor = 1; donor < 100; ++donor)
	{
		Entry existing = {};

		if (donor == id || !Read(donor, existing) || !existing.present)
			continue;

		TryReadDword(reinterpret_cast<const void*>(RecordAddress(donor) + GameOffsets::kBgmSource),
			source);
		break;
	}

	const uint8_t loops = entry.loops ? 1 : 0;
	const double loopPos = entry.loopPos;

	char file[GameOffsets::kBgmFileMax] = {};
	strncpy_s(file, sizeof(file), entry.file, _TRUNCATE);

	if (!TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kBgmSource), source))
		return false;

	if (!TryWriteMemory(reinterpret_cast<void*>(record + GameOffsets::kBgmFile), file, sizeof(file)))
		return false;

	TryWriteMemory(reinterpret_cast<void*>(record + GameOffsets::kBgmIsLoop), &loops, sizeof(loops));
	TryWriteMemory(reinterpret_cast<void*>(record + GameOffsets::kBgmLoopPos), &loopPos,
		sizeof(loopPos));
	TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kBgmVolume),
		static_cast<uint32_t>(entry.volume > 0 ? entry.volume : 10000));
	TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kBgmNoRecording), 0);

	return TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kBgmPresent), 1);
}

bool BgmTable::IsPresent(int id)
{
	Entry entry = {};
	if (!Read(id, entry))
		return false;

	return entry.present;
}

int BgmTable::CollectPresent(int* outIds, int maxIds)
{
	if (outIds == nullptr || maxIds <= 0)
		return 0;

	int count = 0;

	for (int id = 0; id < kSlotCount && count < maxIds; ++id)
	{
		if (!IsPresent(id))
			continue;

		outIds[count] = id;
		++count;
	}

	return count;
}

const char* BgmTable::DescribeSlot(int id)
{
	for (const SlotName& name : kSlotNames)
	{
		if (id >= name.first && id <= name.last)
			return name.text;
	}

	return "";
}
