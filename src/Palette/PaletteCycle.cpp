#include "Palette/PaletteCycle.h"

#include "Palette/PaletteLibrary.h"

#include <cstring>

int PaletteCycle::Step(int index, int steps, int count)
{
	if (count <= 0)
		return kNone;

	const int slots = count + 1;
	const int target = ((index + 1 + steps) % slots + slots) % slots;

	return target - 1;
}

int PaletteCycle::IndexOf(int chara, const char* file)
{
	if (file == nullptr || file[0] == '\0')
		return kNone;

	for (int i = 0; i < PaletteLibrary::GetCount(chara); ++i)
	{
		if (_stricmp(PaletteLibrary::GetName(chara, i), file) == 0)
			return i;
	}

	return kNone;
}
