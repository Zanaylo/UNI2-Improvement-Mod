#include "Game/Menus/ColourPreview.h"

#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Menus/ColourPicker.h"
#include "Game/Menus/LobbyAvatar.h"
#include "Palette/ColourSlotStep.h"
#include "Palette/ColourSlots.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PalettePaint.h"

#include <cstring>
#include <string>

namespace {

constexpr int kNoCharacter = -1;
constexpr int kFileLength = 128;

struct Shown
{
	int chara;
	char file[kFileLength];
	bool loaded;
	uint8_t colours[PalettePaint::kBytes];
};

Shown g_shown[ColourPicker::kSides] = {
	{ kNoCharacter, "", false, {} },
	{ kNoCharacter, "", false, {} },
};

std::string WantedAtSelect(int side, int& chara)
{
	int colour = 0;

	if (!ColourPicker::ReadSide(side, chara, colour) || !PaletteControl::CanEdit(side))
		return "";

	ColourPicker::Shown extended = {};

	if (ColourPicker::Describe(side, extended) && extended.extended != ColourSlotStep::kNoExtended)
		return extended.file;

	return ColourSlots::BoundFile(chara, colour);
}

void Show(int side, int chara, const std::string& file)
{
	Shown& shown = g_shown[side];

	if (chara == shown.chara && file == shown.file)
		return;

	shown.chara = chara;
	strncpy_s(shown.file, file.c_str(), _TRUNCATE);
	shown.loaded = PaletteLibrary::LoadColours(shown.chara, shown.file, shown.colours);

	if (shown.loaded)
		PalettePaint::StageSelect(side, shown.colours);
	else
		PalettePaint::ClearSelect(side);
}

void Forget(int side)
{
	Shown& shown = g_shown[side];

	if (shown.chara == kNoCharacter && !shown.loaded)
		return;

	shown.chara = kNoCharacter;
	shown.file[0] = '\0';
	shown.loaded = false;

	PalettePaint::ClearSelect(side);
}

void ShowAtSelect(int side)
{
	int chara = kNoCharacter;
	const std::string file = WantedAtSelect(side, chara);

	Show(side, chara, file);
}

}

void ColourPreview::OnFrame()
{
	const uint32_t scene = SceneWatch::Current();

	if (scene == GameOffsets::kSceneCharaSelect)
	{
		LobbyAvatar::Forget();

		for (int side = 0; side < ColourPicker::kSides; ++side)
			ShowAtSelect(side);

		return;
	}

	for (int side = 0; side < ColourPicker::kSides; ++side)
		Forget(side);

	if (scene == GameOffsets::kSceneNetwork)
	{
		LobbyAvatar::OnFrame();
		return;
	}

	LobbyAvatar::Forget();
}

const uint8_t* ColourPreview::Colours(int side)
{
	if (side < 0 || side >= ColourPicker::kSides || !g_shown[side].loaded)
		return nullptr;

	return g_shown[side].colours;
}
