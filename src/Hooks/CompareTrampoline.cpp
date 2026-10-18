#include "Hooks/CompareTrampoline.h"

#include "Core/utils.h"

#include <cstring>

namespace {

constexpr uint8_t kNop = 0x90;
constexpr uint8_t kJumpNear = 0xe9;
constexpr uint8_t kCompareGroup = 0x83;
constexpr uint8_t kCompareWide = 0x81;
constexpr uint8_t kTwoByte = 0x0f;
constexpr uint8_t kNearCondition = 0x80;
constexpr uint8_t kFirstRegisterForm = 0xf8;
constexpr uint8_t kFirstShortCondition = 0x7c;
constexpr uint8_t kLastShortCondition = 0x7f;
constexpr uint8_t kFirstNearCondition = 0x8c;
constexpr uint8_t kLastNearCondition = 0x8f;
constexpr uint8_t kConditionMask = 0x0f;
constexpr size_t kCompareBytes = 3;
constexpr size_t kShortJumpBytes = 2;
constexpr size_t kNearJumpBytes = 6;
constexpr size_t kJumpBytes = 5;

bool DecodeJump(uint8_t* jump, CompareTrampoline::Site& out)
{
	if (jump[0] >= kFirstShortCondition && jump[0] <= kLastShortCondition)
	{
		out.condition = static_cast<uint8_t>(jump[0] & kConditionMask);
		out.jumpBytes = kShortJumpBytes;
		out.target = jump + kShortJumpBytes + static_cast<int8_t>(jump[1]);
		return true;
	}

	if (jump[0] != kTwoByte || jump[1] < kFirstNearCondition || jump[1] > kLastNearCondition)
		return false;

	int32_t relative = 0;
	memcpy(&relative, jump + 2, sizeof(relative));

	out.condition = static_cast<uint8_t>(jump[1] & kConditionMask);
	out.jumpBytes = kNearJumpBytes;
	out.target = jump + kNearJumpBytes + relative;
	return true;
}

}

bool CompareTrampoline::Decode(uint8_t* at, Site& out)
{
	if (at[0] != kCompareGroup || at[1] < kFirstRegisterForm)
		return false;

	if (!DecodeJump(at + kCompareBytes, out))
		return false;

	out.at = at;
	out.modrm = at[1];
	out.counted = at[2];

	return true;
}

bool CompareTrampoline::Emit(const Site& site, uint8_t* trampoline, int32_t bound)
{
	uint8_t body[kBytes] = {};
	size_t at = 0;

	body[at++] = kCompareWide;
	body[at++] = site.modrm;
	memcpy(body + at, &bound, sizeof(bound));
	at += sizeof(bound);

	body[at++] = kTwoByte;
	body[at++] = static_cast<uint8_t>(kNearCondition | site.condition);

	const int32_t toTarget = static_cast<int32_t>(site.target - (trampoline + at + 4));
	memcpy(body + at, &toTarget, sizeof(toTarget));
	at += sizeof(toTarget);

	uint8_t* const resume = site.at + kCompareBytes + site.jumpBytes;

	body[at++] = kJumpNear;

	const int32_t toResume = static_cast<int32_t>(resume - (trampoline + at + 4));
	memcpy(body + at, &toResume, sizeof(toResume));
	at += sizeof(toResume);

	memcpy(trampoline, body, kBytes);

	uint8_t patch[kLongestSite];
	memset(patch, kNop, sizeof(patch));
	patch[0] = kJumpNear;

	const int32_t toTrampoline = static_cast<int32_t>(trampoline - (site.at + kJumpBytes));
	memcpy(patch + 1, &toTrampoline, sizeof(toTrampoline));

	return WriteCodeBytes(site.at, patch, kCompareBytes + site.jumpBytes);
}
