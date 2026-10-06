#include "Game/Menus/ColourPicker.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/SceneWatch.h"
#include "Hooks/GameHook.h"
#include "Palette/ColourSlotMemory.h"
#include "Palette/ColourSlots.h"
#include "Palette/ColourSlotStep.h"
#include "Palette/NetworkPick.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSignature.h"

#include <cstdio>
#include <cstring>

namespace {

struct PickerInput
{
	uint8_t head[4];
	uint8_t lever;
	uint8_t pad[3];
	uint32_t buttons;
};

typedef int(__fastcall* PickerInputFn)(int side, PickerInput* input);
typedef int(__fastcall* PickerListFn)(int side, int* colours);

constexpr uint8_t kLeverLeft = 4;
constexpr uint8_t kLeverRight = 6;
constexpr int kParkStep = 1;
constexpr int kNoCharacter = -1;
constexpr int kNoColour = -1;
constexpr int kFileLength = 128;

struct Choice
{
	int chara;
	int extended;
	int parked;
	int count;
	char file[kFileLength];
};

GameHook<PickerInputFn> g_hook("ColourPickerInput");
PickerListFn g_list = nullptr;

Choice g_choices[ColourPicker::kSides] = {
	{ kNoCharacter, ColourSlotStep::kNoExtended, kNoColour, 0, "" },
	{ kNoCharacter, ColourSlotStep::kNoExtended, kNoColour, 0, "" },
};

bool g_closed[ColourPicker::kSides] = { true, true };

unsigned g_picks = 0;

char g_status[160] = "not installed";

bool IsSide(int side)
{
	return side >= 0 && side < ColourPicker::kSides;
}

bool IsCharacter(int chara)
{
	return chara >= 0 && chara < ColourSlots::kCharacters;
}

int* SideField(int side, uintptr_t field)
{
	const uintptr_t record = RvaToAddress(GameOffsets::kCharaSelectSides) +
		static_cast<uintptr_t>(side) * GameOffsets::kCharaSelectSideStride;

	return reinterpret_cast<int*>(record + field);
}

bool ReadWindow(int side, uintptr_t field, uint32_t& value)
{
	uint32_t screen = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kCharaSelectScreen)), screen) ||
		screen == 0)
	{
		return false;
	}

	const uintptr_t at = screen + static_cast<uintptr_t>(side) * GameOffsets::kColourSelectStride + field;

	return TryReadDword(reinterpret_cast<const void*>(at), value);
}

bool InAltMode(int side)
{
	uint32_t mode = 0;

	return !ReadWindow(side, GameOffsets::kColourSelectAltMode, mode) || mode != 0;
}

bool Eligible(int side)
{
	return g_modVals.paletteExtendedSlots && g_list != nullptr && PaletteControl::CanEdit(side);
}

int IndexOf(const int* colours, int count, int colour)
{
	for (int i = 0; i < count; ++i)
	{
		if (colours[i] == colour)
			return i;
	}

	return 0;
}

void Settle(Choice& choice, int extended)
{
	choice.extended = extended;
	choice.count = ColourSlots::ExtendedCount(choice.chara);
	strncpy_s(choice.file, ColourSlots::ExtendedFile(choice.chara, extended), _TRUNCATE);
}

int RememberedExtended(int side, int chara)
{
	const int own = ColourSlots::ExtendedOf(chara, ColourSlotMemory::Recall(side, chara));

	if (own != ColourSlotStep::kNoExtended || !GameState::IsNetworkSelection())
		return own;

	NetworkPick::Pick pick = {};

	if (!NetworkPick::Recall(pick) || pick.chara != chara)
		return ColourSlotStep::kNoExtended;

	return ColourSlots::ExtendedOf(chara, pick.file);
}

int LandOn(int side, PickerInput* input, const int* colours, int count, int target, int delta)
{
	int* const colour = SideField(side, GameOffsets::kCharaSelectSideColour);
	*colour = colours[ColourSlotStep::LeadIn(count, target, delta)];

	const uint8_t lever = input->lever;
	input->lever = delta < 0 ? kLeverLeft : kLeverRight;

	const int result = g_hook.Original()(side, input);

	input->lever = lever;
	g_choices[side].parked = *colour;

	return result;
}

bool Opens(int side)
{
	return g_closed[side] || *SideField(side, GameOffsets::kCharaSelectSideChara) != g_choices[side].chara;
}

int Open(int side, PickerInput* input)
{
	Choice& choice = g_choices[side];
	const int chara = *SideField(side, GameOffsets::kCharaSelectSideChara);

	g_closed[side] = false;
	choice = { chara, ColourSlotStep::kNoExtended, kNoColour, 0, "" };

	if (!IsCharacter(chara))
		return g_hook.Original()(side, input);

	PaletteLibrary::Rescan(chara);
	Settle(choice, ColourSlotStep::kNoExtended);

	const int extended = RememberedExtended(side, chara);
	int colours[GameOffsets::kColourPickerListCapacity] = {};
	const int count = g_list(side, colours);

	if (extended == ColourSlotStep::kNoExtended || count <= 0 || InAltMode(side))
		return g_hook.Original()(side, input);

	Settle(choice, extended);
	LOG("colour picker: p%d opens on its last pick, extended slot %d, '%s'", side + 1, choice.extended, choice.file);

	return LandOn(side, input, colours, count, ColourSlotStep::kParkIndex, kParkStep);
}

Choice& Follow(int side)
{
	Choice& choice = g_choices[side];

	if (choice.extended != ColourSlotStep::kNoExtended &&
		*SideField(side, GameOffsets::kCharaSelectSideColour) != choice.parked)
	{
		Settle(choice, ColourSlotStep::kNoExtended);
	}

	return choice;
}

const char* PickedFile(int side)
{
	ColourPicker::Shown shown = {};

	return ColourPicker::Describe(side, shown) ? shown.file : "";
}

void RememberPick(int side)
{
	const char* const file = PickedFile(side);
	const int chara = *SideField(side, GameOffsets::kCharaSelectSideChara);

	ColourSlotMemory::Remember(side, chara, file);

	if (!GameState::IsNetworkSelection())
		return;

	const int colour = *SideField(side, GameOffsets::kCharaSelectSideColour);

	uint8_t shown[PalettePaint::kBytes] = {};
	uint8_t signature[PaletteSignature::kBytes] = {};
	const bool seen = PalettePaint::ReadGameColours(side, shown);

	if (seen)
		PaletteSignature::Take(shown, signature);

	NetworkPick::Remember(chara, colour, file, seen ? signature : nullptr);
	LOG("colour picker: network pick is character %d colour %d '%s', colours %s", chara, colour, file,
		seen ? "signed" : "not seen");
}

bool Closes(int result)
{
	return result == GameOffsets::kColourPickerConfirmed || result == GameOffsets::kColourPickerCancelled;
}

void NotePick(int side, int result)
{
	if (!Closes(result))
		return;

	g_closed[side] = true;

	if (result != GameOffsets::kColourPickerConfirmed)
		return;

	++g_picks;
	RememberPick(side);

	const Choice& choice = g_choices[side];

	if (choice.extended == ColourSlotStep::kNoExtended)
		return;

	LOG("colour picker: p%d took extended slot %d, '%s'", side + 1, choice.extended, choice.file);
}

int Route(int side, PickerInput* input)
{
	if (!Eligible(side))
		return g_hook.Original()(side, input);

	if (Opens(side))
		return Open(side, input);

	Choice& choice = Follow(side);
	const int delta = ColourSlotStep::DeltaOf(input->lever);

	if (delta == 0 || !IsCharacter(choice.chara) || InAltMode(side))
		return g_hook.Original()(side, input);

	int colours[GameOffsets::kColourPickerListCapacity] = {};
	const int count = g_list(side, colours);
	const int current = IndexOf(colours, count, *SideField(side, GameOffsets::kCharaSelectSideColour));

	const ColourSlotStep::Result move = ColourSlotStep::Step(count, { current, choice.extended },
		ColourSlots::ExtendedCount(choice.chara), delta);

	Settle(choice, move.extended);

	if (move.gameHandles)
		return g_hook.Original()(side, input);

	return LandOn(side, input, colours, count, move.listIndex, delta);
}

int __fastcall HookedInput(int side, PickerInput* input)
{
	if (input == nullptr || !IsSide(side))
		return g_hook.Original()(side, input);

	const int result = Route(side, input);
	NotePick(side, result);

	return result;
}

}

bool ColourPicker::Install()
{
	if (g_hook.IsLive())
		return true;

	const uintptr_t list = CodeSignatures::Address(GameOffsets::kFnColourPickerList);

	if (!IsAddressInGameModule(list))
	{
		strncpy_s(g_status, "the colour list is not where this game version expects it", _TRUNCATE);
		LOG("colour picker: %s", g_status);
		return false;
	}

	g_list = reinterpret_cast<PickerListFn>(list);

	if (!g_hook.InstallRva(GameOffsets::kFnColourPickerInput, &HookedInput))
	{
		strncpy_s(g_status, "the colour picker is not where this game version expects it", _TRUNCATE);
		LOG("colour picker: %s", g_status);
		return false;
	}

	strncpy_s(g_status, "on", _TRUNCATE);
	LOG("colour picker: hooked, extended slots follow the last colour");
	return true;
}

int ColourPicker::ExtendedFor(int side, int chara)
{
	if (!g_modVals.paletteExtendedSlots || !IsSide(side) || !IsCharacter(chara))
		return ColourSlotStep::kNoExtended;

	const Choice& own = g_choices[side];

	return own.chara == chara ? own.extended : ColourSlotStep::kNoExtended;
}

bool ColourPicker::Describe(int side, Shown& out)
{
	if (!IsSide(side) || SceneWatch::Current() != GameOffsets::kSceneCharaSelect)
		return false;

	const Choice& choice = g_choices[side];

	if (!g_modVals.paletteExtendedSlots || !IsCharacter(choice.chara) ||
		*SideField(side, GameOffsets::kCharaSelectSideChara) != choice.chara)
	{
		return false;
	}

	const bool parked = choice.extended != ColourSlotStep::kNoExtended &&
		*SideField(side, GameOffsets::kCharaSelectSideColour) == choice.parked && choice.file[0] != '\0';

	out.chara = choice.chara;
	out.extended = parked ? choice.extended : ColourSlotStep::kNoExtended;
	out.count = choice.count;
	out.file = parked ? choice.file : "";

	return true;
}

bool ColourPicker::ReadSide(int side, int& chara, int& colour)
{
	if (!IsSide(side))
		return false;

	chara = *SideField(side, GameOffsets::kCharaSelectSideChara);
	colour = *SideField(side, GameOffsets::kCharaSelectSideColour);

	return IsCharacter(chara);
}

unsigned ColourPicker::Picks()
{
	return g_picks;
}

const char* ColourPicker::StatusText()
{
	if (!g_hook.IsLive())
		return g_status;

	sprintf_s(g_status, "on, %ld picker call%s seen", g_hook.Calls(), g_hook.Calls() == 1 ? "" : "s");
	return g_status;
}
