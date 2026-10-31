#include "Training/QuickTraining.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/SceneExit.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Menus/ColourPicker.h"
#include "Network/OnlineContext.h"
#include "Palette/ColourSlots.h"

#include <cstdint>
#include <string>

namespace {

constexpr const char* kSection = "Training";
constexpr const char* kEnabledKey = "QuickTrainingPick";
constexpr const char* kCharaKeys[QuickTraining::kPlayers] = {
	"QuickTrainingP1Character", "QuickTrainingP2Character" };
constexpr const char* kColourKeys[QuickTraining::kPlayers] = {
	"QuickTrainingP1Color", "QuickTrainingP2Color" };
constexpr const char* kPaletteKeys[QuickTraining::kPlayers] = {
	"QuickTrainingP1Palette", "QuickTrainingP2Palette" };

constexpr uint8_t kPickReady = 1;
constexpr int kParkColour = 0;

struct TrainingPick
{
	int32_t bgm;
	int32_t chara[QuickTraining::kPlayers];
	int32_t colour[QuickTraining::kPlayers];
	int32_t stage;
};

static_assert(sizeof(TrainingPick) ==
	GameOffsets::kTrainingStageLive + sizeof(int32_t) - GameOffsets::kTrainingPick,
	"TrainingPick must cover the game's block up to the training stage");

int* const g_charas[QuickTraining::kPlayers] = {
	&g_modVals.quickTrainingChara1, &g_modVals.quickTrainingChara2 };
int* const g_colours[QuickTraining::kPlayers] = {
	&g_modVals.quickTrainingColour1, &g_modVals.quickTrainingColour2 };
std::string* const g_palettes[QuickTraining::kPlayers] = {
	&g_settings.quickTrainingPalette1, &g_settings.quickTrainingPalette2 };

SceneExit g_bootExit(GameOffsets::kSceneBoot);
bool g_applied = false;
bool g_holdingOriginal = false;
TrainingPick g_original = {};
uint8_t g_originalReady = 0;

void* At(uintptr_t rva)
{
	return reinterpret_cast<void*>(RvaToAddress(rva));
}

bool IsValidPlayer(int player)
{
	return player >= 0 && player < QuickTraining::kPlayers;
}

bool ReadGameChoices(TrainingPick& pick)
{
	return TryReadMemory(&pick.stage, At(GameOffsets::kTrainingStageSaved), sizeof(pick.stage)) &&
		TryReadMemory(&pick.bgm, At(GameOffsets::kBattleBgmPick), sizeof(pick.bgm));
}

QuickTraining::Wear WearFor(int player)
{
	const int chara = *g_charas[player];
	const char* const file = g_palettes[player]->c_str();
	const QuickTraining::Wear chosenColour = { *g_colours[player], ColourSlots::kNoExtended };

	if (file[0] == '\0')
		return chosenColour;

	const int bound = ColourSlots::ColourOf(chara, file);

	if (bound != ColourSlots::kNoColour)
		return { bound, ColourSlots::kNoExtended };

	const int extended = ColourSlots::ExtendedOf(chara, file);

	if (extended == ColourSlots::kNoExtended)
		return chosenColour;

	return { kParkColour, extended };
}

bool Apply()
{
	void* const pickAt = At(GameOffsets::kTrainingPick);
	void* const readyAt = At(GameOffsets::kTrainingPickReady);

	TrainingPick pick = {};
	uint8_t ready = 0;

	if (!TryReadMemory(&pick, pickAt, sizeof(pick)) || !TryReadMemory(&ready, readyAt, sizeof(ready)))
		return false;

	if (!g_holdingOriginal)
	{
		g_original = pick;
		g_originalReady = ready;
	}

	if (ready != kPickReady && !ReadGameChoices(pick))
		return false;

	QuickTraining::Wear wears[QuickTraining::kPlayers] = {};

	for (int player = 0; player < QuickTraining::kPlayers; ++player)
	{
		wears[player] = WearFor(player);
		pick.chara[player] = *g_charas[player];
		pick.colour[player] = wears[player].colour;
	}

	if (!TryWriteMemory(pickAt, &pick, sizeof(pick)) || !TryWriteMemory(readyAt, &kPickReady, sizeof(kPickReady)))
		return false;

	for (int player = 0; player < QuickTraining::kPlayers; ++player)
		ColourPicker::Choose(player, pick.chara[player], wears[player].extended);

	g_holdingOriginal = true;
	LOG("QuickTraining: %d (colour %d, EX %d) vs %d (colour %d, EX %d)", pick.chara[0], pick.colour[0],
		wears[0].extended, pick.chara[1], pick.colour[1], wears[1].extended);
	return true;
}

void Restore()
{
	if (!g_holdingOriginal)
		return;

	g_holdingOriginal = false;
	TryWriteMemory(At(GameOffsets::kTrainingPick), &g_original, sizeof(g_original));
	TryWriteMemory(At(GameOffsets::kTrainingPickReady), &g_originalReady, sizeof(g_originalReady));

	for (int player = 0; player < QuickTraining::kPlayers; ++player)
		ColourPicker::Choose(player, g_original.chara[player], ColourSlots::kNoExtended);

	LOG("QuickTraining: the game's pick restored");
}

bool CanApply()
{
	return g_modVals.quickTrainingPick && !OnlineContext::IsActive();
}

void SavePalette(int player, const char* file)
{
	*g_palettes[player] = file;
	Settings::SaveString(kSection, kPaletteKeys[player], file);
}

}

bool QuickTraining::IsEnabled()
{
	return g_modVals.quickTrainingPick;
}

void QuickTraining::SetEnabled(bool enabled)
{
	g_modVals.quickTrainingPick = enabled;
	Settings::SaveInt(kSection, kEnabledKey, enabled ? 1 : 0);
	g_applied = false;

	if (!enabled)
		Restore();
}

int QuickTraining::GetChara(int player)
{
	return IsValidPlayer(player) ? *g_charas[player] : 0;
}

QuickTraining::Wear QuickTraining::GetWear(int player)
{
	if (!IsValidPlayer(player))
		return { 0, ColourSlots::kNoExtended };

	return WearFor(player);
}

void QuickTraining::SetChara(int player, int chara)
{
	if (!IsValidPlayer(player))
		return;

	*g_charas[player] = chara;
	Settings::SaveInt(kSection, kCharaKeys[player], chara);
	SavePalette(player, "");
	g_applied = false;
}

void QuickTraining::SetColour(int player, int colour)
{
	if (!IsValidPlayer(player))
		return;

	*g_colours[player] = colour;
	Settings::SaveInt(kSection, kColourKeys[player], colour);
	SavePalette(player, "");
	g_applied = false;
}

void QuickTraining::SetPalette(int player, const char* file)
{
	if (!IsValidPlayer(player) || file == nullptr)
		return;

	SavePalette(player, file);
	g_applied = false;
}

void QuickTraining::OnFrame()
{
	const uint32_t scene = SceneWatch::Raw();

	if (g_bootExit.Observe(scene) && CanApply())
		Apply();

	if (scene != GameOffsets::kSceneMainMenu)
	{
		g_applied = false;
		g_holdingOriginal = false;
		return;
	}

	if (g_applied || !CanApply())
		return;

	g_applied = true;
	Apply();
}
