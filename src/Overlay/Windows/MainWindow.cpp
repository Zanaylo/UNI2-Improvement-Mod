#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Windows/MainWindow.h"

#include "Overlay/Panels/ConfigPanel.h"
#include "Overlay/Panels/GraphicsPanel.h"
#include "Overlay/Panels/PalettesPanel.h"
#include "Overlay/Panels/ReplayPanel.h"
#include "Overlay/Panels/TrainingPanel.h"

#include "Core/Config/Hotkeys.h"
#include "Core/info.h"
#include "Core/Config/interfaces.h"
#include "Core/Input/KeyboardCapture.h"
#include "Core/Config/keycodes.h"
#include "Core/Input/PadInput.h"
#include "Core/Config/Settings.h"
#include "Core/utils.h"
#include "Game/Lobby/NameCensor.h"
#include "Game/Patches/GamePatches.h"
#include "Game/Audio/BgmControl.h"
#include "Network/PlayerCount.h"
#include "Game/Audio/BgmNames.h"
#include "Screens/ScreenDirector.h"
#include "Screens/ScreenTheme.h"
#include "Overlay/Widgets/UiText.h"
#include "Overlay/Framework/WindowManager.h"
#include "Game/Files/ModPacks.h"
#include "Game/Stages/BgCeiling.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Audio/SoundPacks.h"
#include "Game/Subtitles/SubtitleWatch.h"

#include <Windows.h>

MainWindow::MainWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void MainWindow::BeforeDraw()
{
	const ImGuiStyle& style = ImGui::GetStyle();

	const float titleWidth = ImGui::CalcTextSize(m_title.c_str()).x;
	const float decorations = ImGui::GetFontSize() * 2.0f + style.FramePadding.x * 4.0f +
		style.ItemInnerSpacing.x * 2.0f;

	const float base = ImGui::GetFontSize() * 32.0f;

	const float ceiling = ImGui::GetIO().DisplaySize.y * 0.85f;

	ImGui::SetNextWindowSizeConstraints(ImVec2(max(titleWidth + decorations, base), 0.0f),
		ImVec2(FLT_MAX, ceiling > 0.0f ? ceiling : FLT_MAX));

	ImGui::SetNextWindowSize(ImVec2(base, 0.0f), ImGuiCond_FirstUseEver);
}

void MainWindow::DrawPlayerCount()
{
	ImGui::TextUnformatted("Current online players:");
	ImGui::SameLine();

	if (!PlayerCount::IsKnown())
	{
		UiText::Muted("%s", PlayerCount::GetStatusText());
		return;
	}

	UiText::Good("%d", PlayerCount::Get());
}

void MainWindow::Draw()
{
	if (!IsMeasuredGameBuild())
	{
		UiText::Warn("The game was updated. Game features stay off until the mod is updated too.");
		ImGui::Separator();
	}

	DrawPlayerCount();
	ImGui::Separator();
	TrainingPanel::Draw();
	ImGui::Separator();
	PalettesPanel::Draw();
	ImGui::Separator();
	ReplayPanel::Draw();
	ImGui::Separator();
	DrawMusicSection();
	ImGui::Separator();
	DrawSoundSection();
	ImGui::Separator();
	DrawSubtitlesSection();
	ImGui::Separator();
	DrawStagesSection();
	ImGui::Separator();
	DrawOnlineSection();
	ImGui::Separator();
	DrawPerformanceSection();
	ImGui::Separator();
	DrawPatchSection();
	ImGui::Separator();
	DrawThemeSection();
	ImGui::Separator();
	DrawModsSection();
	ImGui::Separator();
	ConfigPanel::Draw();
}

void MainWindow::DrawMusicSection()
{
	if (!ImGui::CollapsingHeader("Music"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Music) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close music" : "Open music"))
		window->Toggle();

	ImGui::TextWrapped("Soundpacks, the track list and the rules for what plays where are all in "
		"that window.");

	if (!BgmControl::IsHooked())
	{
		UiText::Warn("Music control is not active: %s", BgmControl::GetStatusText());
		return;
	}

	if (ImGui::Checkbox("Keep the menu music playing", &g_modVals.keepMenuMusic))
		Settings::SaveInt("Music", "KeepMenuMusic", g_modVals.keepMenuMusic ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Normally the menu music starts over every time you come back from "
			"Options, Customize or Gallery. Turn this on to keep the same track playing across "
			"those screens.");
	}

	char playing[224] = {};

	if (!BgmNames::Describe(BgmControl::Current(), playing, sizeof(playing)))
		strncpy_s(playing, "silence", _TRUNCATE);

	UiText::Muted("Playing: %s", playing);
}

void MainWindow::DrawSoundSection()
{
	if (!ImGui::CollapsingHeader("Voices and sound"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Sound) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close voices" : "Open voices"))
		window->Toggle();

	ImGui::TextWrapped("Give one character another game's voice without changing anyone else, or "
		"replace the shared sound effects. Packs are folders you can zip and share.");

	UiText::Muted("%s", SoundPacks::StatusText());
}

void MainWindow::DrawSubtitlesSection()
{
	if (!ImGui::CollapsingHeader("Subtitles"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Subtitles) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close subtitles" : "Open subtitles"))
		window->Toggle();

	ImGui::TextWrapped("Shows what each character says on screen, in the game's own font. Write "
		"the lines for each character, save them to a file and share it.");

	if (!g_modVals.subtitles)
	{
		UiText::Muted("Subtitles are off.");
		return;
	}

	UiText::Good("Subtitles are on. %d line(s) shown so far.", SubtitleWatch::Shows());
}

void MainWindow::DrawPerformanceSection()
{
	if (!ImGui::CollapsingHeader("Performance"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Performance) : nullptr;

	if (window == nullptr)
	{
		UiText::Warn("The performance editor could not be created.");
		return;
	}

	if (ImGui::Button(window->IsOpen() ? "Close performance editor" : "Open performance editor"))
		window->Toggle();

	ImGui::TextWrapped("Frame pacing, POTATO MODE and where each frame's time goes. All their "
		"settings are in that window.");
}

void MainWindow::DrawPatchSection()
{
	if (!ImGui::CollapsingHeader("Game patches"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Patches) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close patches" : "Open patches"))
		window->Toggle();

	ImGui::TextWrapped("Play with the battle data of an older game version, so replays recorded "
		"on it play back the way they were made.");

	const GamePatches::Patch* const active = GamePatches::Get(GamePatches::ActiveIndex());

	if (active == nullptr)
	{
		UiText::Muted("Playing the installed game.");
		return;
	}

	UiText::Good("Playing %s.", active->name.c_str());
}

void MainWindow::DrawThemeSection()
{
	if (ScreenDirector::kOnHold)
		return;

	if (!ImGui::CollapsingHeader("Theme"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Theme) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close theme" : "Open theme"))
		window->Toggle();

	ImGui::TextWrapped("A theme draws another French-Bread game's screens over UNI2's. Every UNI2 "
		"option stays where it is.");

	const ScreenTheme::Theme* const theme = ScreenTheme::Active();

	if (theme == nullptr)
	{
		UiText::Muted("Using the game's own screens.");
		return;
	}

	UiText::Good("Applied: %s", theme->name.c_str());
	UiText::Muted("%s", ScreenDirector::StatusText());
}

void MainWindow::DrawStagesSection()
{
	if (!ImGui::CollapsingHeader("Stages"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Stages) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close stages" : "Open stages"))
		window->Toggle();

	ImGui::TextWrapped("Play the two stages the game hides from its lists, and install stages from "
		"other French-Bread games you own.");

	UiText::Good("%d/%d stages.", StageLibrary::Total(), BgCeiling::Numbers());
}

void MainWindow::DrawOnlineSection()
{
	if (!ImGui::CollapsingHeader("Online"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Netplay) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close netplay" : "Open netplay"))
		window->Toggle();

	ImGui::TextWrapped("Rollback and ping as the game counts them, who is in the room and everyone "
		"you have played. The Privacy tab replaces the other player's name everywhere, and room "
		"names in the room search, for screenshots or streams.");

	if (g_modVals.censorNames)
		UiText::Good("Opponent names are being replaced with \"%s\".",
			NameCensor::Mask());
}

void MainWindow::DrawModsSection()
{
	if (!ImGui::CollapsingHeader("Mods"))
		return;

	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	IWindow* const window = container != nullptr
		? container->GetWindow(WindowType_Mods) : nullptr;

	if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close mods" : "Open mods"))
		window->Toggle();

	ImGui::TextWrapped("Folders in UNI2-IM\\Packs that replace the game's own files, like a voice, "
		"a screen or a stage. Turn them on or off without restarting.");

	if (ModPacks::Count() > 0)
	{
		UiText::Good("%d of %d mod(s) on, %d file(s).", ModPacks::EnabledCount(),
			ModPacks::Count(), ModPacks::FileCount());
	}
}
