#include "Game/Audio/SoundOwners.h"

#include "Game/Tables/CharaTables.h"

namespace {

struct Announcer
{
	int owner;
	const char* name;
};

constexpr Announcer kAnnouncers[] = {
	{ 100, "Default announcer" },
	{ 101, "Silvaria announcer" },
	{ 102, "Tsukuyomi announcer" },
};

constexpr int kAnnouncerCount = sizeof(kAnnouncers) / sizeof(kAnnouncers[0]);

const Announcer* FindAnnouncer(int owner)
{
	for (const Announcer& announcer : kAnnouncers)
	{
		if (announcer.owner == owner)
			return &announcer;
	}

	return nullptr;
}

}

int SoundOwners::Count()
{
	return CharaTables::GetCharaCount() + kAnnouncerCount;
}

int SoundOwners::At(int index)
{
	const int fighters = CharaTables::GetCharaCount();

	if (index < 0 || index >= Count())
		return -1;

	return index < fighters ? index : kAnnouncers[index - fighters].owner;
}

int SoundOwners::IndexOf(int owner)
{
	if (IsFighter(owner))
		return owner;

	for (int i = 0; i < kAnnouncerCount; ++i)
	{
		if (kAnnouncers[i].owner == owner)
			return CharaTables::GetCharaCount() + i;
	}

	return -1;
}

bool SoundOwners::Has(int owner)
{
	return IndexOf(owner) >= 0;
}

bool SoundOwners::IsFighter(int owner)
{
	return owner >= 0 && owner < CharaTables::GetCharaCount();
}

const char* SoundOwners::Name(int owner)
{
	const Announcer* const announcer = FindAnnouncer(owner);

	return announcer != nullptr ? announcer->name : CharaTables::Name(owner);
}
