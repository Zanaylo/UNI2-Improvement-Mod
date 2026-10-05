#include "Game/Customize/AnnouncerTicks.h"

AnnouncerTicks::Slots AnnouncerTicks::Imported(const std::vector<int>& stockIds)
{
	Slots imported = {};

	if (stockIds.empty())
		return imported;

	imported.fill(true);

	for (const int id : stockIds)
	{
		if (id >= 0 && id < kSlots)
			imported[id] = false;
	}

	return imported;
}

void AnnouncerTicks::Keep(const Slots& imported, const Flags& before, uint8_t* ticked, uint8_t* excluded)
{
	for (int id = 0; id < kSlots; ++id)
	{
		if (!imported[id])
			continue;

		ticked[id] = before[id];
		excluded[id] = before[id] != 0 ? 0 : 1;
	}
}
