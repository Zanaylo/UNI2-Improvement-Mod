#include "Game/ColorOverride.h"

#include "Game/CharaTables.h"
#include "Game/ColorCustomize.h"

#include <cstring>
#include <map>

namespace {

struct Pick
{
	uint8_t rgb[3];
	bool set;
};

struct Slot
{
	Pick parts[ColorCustomize::kPartCount];
};

std::map<int, Slot> g_picks;

int Key(int chara, int slot)
{
	return chara * ColorCustomize::kSlotCount + slot;
}

bool Valid(int chara, int slot, int part)
{
	return chara >= 0 && chara < CharaTables::GetCharaCount()
		&& slot >= 0 && slot < ColorCustomize::kSlotCount
		&& part >= 0 && part < ColorCustomize::kPartCount;
}

int Luminance(const uint8_t* rgb)
{
	return (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) / 1000;
}

}

bool ColorOverride::Has(int chara, int slot, int part)
{
	if (!Valid(chara, slot, part))
		return false;

	const auto found = g_picks.find(Key(chara, slot));
	return found != g_picks.end() && found->second.parts[part].set;
}

bool ColorOverride::Get(int chara, int slot, int part, uint8_t* outRgb)
{
	if (!Has(chara, slot, part) || outRgb == nullptr)
		return false;

	memcpy(outRgb, g_picks[Key(chara, slot)].parts[part].rgb, 3);
	return true;
}

void ColorOverride::Set(int chara, int slot, int part, const uint8_t* rgb)
{
	if (!Valid(chara, slot, part) || rgb == nullptr)
		return;

	Pick& pick = g_picks[Key(chara, slot)].parts[part];

	memcpy(pick.rgb, rgb, 3);
	pick.set = true;
}

void ColorOverride::Clear(int chara, int slot, int part)
{
	if (!Valid(chara, slot, part))
		return;

	const auto found = g_picks.find(Key(chara, slot));
	if (found == g_picks.end())
		return;

	found->second.parts[part].set = false;
}

void ColorOverride::ClearSlot(int chara, int slot)
{
	if (chara < 0 || slot < 0 || slot >= ColorCustomize::kSlotCount)
		return;

	g_picks.erase(Key(chara, slot));
}

bool ColorOverride::AnyInSlot(int chara, int slot)
{
	if (chara < 0 || slot < 0 || slot >= ColorCustomize::kSlotCount)
		return false;

	const auto found = g_picks.find(Key(chara, slot));
	if (found == g_picks.end())
		return false;

	for (const Pick& pick : found->second.parts)
	{
		if (pick.set)
			return true;
	}

	return false;
}

void ColorOverride::Retint(uint8_t* palette, const int* entries, int count,
	const uint8_t* reference, const uint8_t* rgb)
{
	if (palette == nullptr || entries == nullptr || reference == nullptr || rgb == nullptr)
		return;

	const int base = Luminance(reference);
	if (base <= 0)
		return;

	for (int i = 0; i < count; ++i)
	{
		const int entry = entries[i];
		if (entry < 0 || entry >= 256)
			continue;

		uint8_t* const target = palette + entry * 4;

		const int ratio = Luminance(target) * 255 / base;

		for (int c = 0; c < 3; ++c)
		{
			const int lit = rgb[c] * ratio / 255;
			target[c] = static_cast<uint8_t>(lit > 255 ? 255 : lit);
		}
	}
}
