#include "Palette/RoomPalette.h"

#include "Palette/NetworkPick.h"
#include "Palette/PaletteCycle.h"
#include "Palette/PaletteLibrary.h"

namespace {

void Keep(const NetworkPick::Pick& pick, const char* file)
{
	NetworkPick::Remember(pick.chara, pick.colour, file, pick.hasSignature ? pick.signature : nullptr);
}

}

bool RoomPalette::Step(int steps)
{
	NetworkPick::Pick pick = {};

	if (steps == 0 || !NetworkPick::Recall(pick))
		return false;

	const int count = PaletteLibrary::GetCount(pick.chara);

	if (count <= 0)
		return false;

	const int target = PaletteCycle::Step(PaletteCycle::IndexOf(pick.chara, pick.file), steps, count);

	Keep(pick, target == PaletteCycle::kNone ? "" : PaletteLibrary::GetName(pick.chara, target));
	return true;
}

bool RoomPalette::Choose(const char* file)
{
	NetworkPick::Pick pick = {};

	if (file == nullptr || !NetworkPick::Recall(pick))
		return false;

	if (file[0] != '\0' && PaletteCycle::IndexOf(pick.chara, file) == PaletteCycle::kNone)
		return false;

	Keep(pick, file);
	return true;
}

int RoomPalette::Index()
{
	NetworkPick::Pick pick = {};

	if (!NetworkPick::Recall(pick))
		return PaletteCycle::kNone;

	return PaletteCycle::IndexOf(pick.chara, pick.file);
}
