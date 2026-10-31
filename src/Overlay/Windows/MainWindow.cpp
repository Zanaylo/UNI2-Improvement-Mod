#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Windows/MainWindow.h"
#include "Overlay/Windows/MainTabs.h"

#include "Overlay/Panels/ConfigPanel.h"
#include "Overlay/Panels/PalettesPanel.h"
#include "Overlay/Panels/ReplayPanel.h"
#include "Overlay/Panels/TrainingPanel.h"

#include "Core/info.h"
#include "Core/logger.h"
#include "Core/Config/Features.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/Settings.h"
#include "Core/utils.h"
#include "Game/Lobby/NameCensor.h"
#include "Game/Patches/GamePatches.h"
#include "Game/Audio/BgmControl.h"
#include "Network/PlayerCount.h"
#include "Game/Audio/BgmNames.h"
#include "Screens/ScreenDirector.h"
#include "Screens/ScreenTheme.h"
#include "Overlay/Widgets/Cards.h"
#include "Overlay/Widgets/UiText.h"
#include "Overlay/Framework/WindowManager.h"
#include "Game/Files/InstallCheck.h"
#include "Game/Files/ModPacks.h"
#include "Game/Stages/BgCeiling.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Audio/SoundPacks.h"
#include "Game/Subtitles/SubtitleWatch.h"
#include "Core/Text/Language.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr float kBaseWidthInFonts = 32.0f;
constexpr float kHeightShareOfScreen = 0.85f;

IWindow* WindowOf(WindowType type)
{
	WindowContainer* const container = WindowManager::GetInstance().GetContainer();
	return container != nullptr ? container->GetWindow(type) : nullptr;
}

IWindow* LauncherTitle(const char* title, WindowType type)
{
	IWindow* const window = WindowOf(type);

	if (window == nullptr)
		return nullptr;

	if (Cards::TitleWithToggle(title, window->IsOpen()))
		window->Toggle();

	return window;
}

struct ModuleWindow
{
	FeatureCatalog::Id module;
	WindowType window;
};

const ModuleWindow kModuleWindows[] = {
	{ FeatureCatalog::Id_Music, WindowType_Music },
	{ FeatureCatalog::Id_VoicesAndSound, WindowType_Sound },
	{ FeatureCatalog::Id_Subtitles, WindowType_Subtitles },
	{ FeatureCatalog::Id_GamePatches, WindowType_Patches },
	{ FeatureCatalog::Id_Palettes, WindowType_Palette },
	{ FeatureCatalog::Id_Palettes, WindowType_PaletteEditor },
	{ FeatureCatalog::Id_Stages, WindowType_Stages },
	{ FeatureCatalog::Id_Stages, WindowType_StageFields },
	{ FeatureCatalog::Id_NetplayWindow, WindowType_Netplay },
	{ FeatureCatalog::Id_CharacterArt, WindowType_Misc },
	{ FeatureCatalog::Id_ModPacks, WindowType_Mods },
	{ FeatureCatalog::Id_Training, WindowType_PlayerControl },
};

void CloseHiddenModuleWindows()
{
	for (const ModuleWindow& entry : kModuleWindows)
	{
		if (Features::Shows(entry.module))
			continue;

		IWindow* const window = WindowOf(entry.window);

		if (window != nullptr)
			window->Close();
	}
}

MainTabs::Shown TabContents()
{
	return {
		Features::Shows(FeatureCatalog::Id_Training),
		Features::Shows(FeatureCatalog::Id_Music),
		Features::Shows(FeatureCatalog::Id_VoicesAndSound),
		Features::Shows(FeatureCatalog::Id_Subtitles),
		Features::Shows(FeatureCatalog::Id_NetplayWindow),
		Features::Shows(FeatureCatalog::Id_GamePatches),
		Features::Shows(FeatureCatalog::Id_Replays),
		Features::Shows(FeatureCatalog::Id_ModPacks),
	};
}

void Description(const char* text)
{
	UiText::Muted("%s", text);
}

}

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

	const float base = ImGui::GetFontSize() * kBaseWidthInFonts;

	const float ceiling = ImGui::GetIO().DisplaySize.y * kHeightShareOfScreen;

	ImGui::SetNextWindowSizeConstraints(ImVec2(max(titleWidth + decorations, base), 0.0f),
		ImVec2(FLT_MAX, ceiling > 0.0f ? ceiling : FLT_MAX));

	ImGui::SetNextWindowSize(ImVec2(base, 0.0f), ImGuiCond_FirstUseEver);
}

void MainWindow::Draw()
{
	DrawPlayerCount();
	DrawSteamFolderWarning();
	DrawGameBuildWarning();
	DrawTabs();
}

void MainWindow::DrawPlayerCount()
{
	if (!PlayerCount::IsKnown())
	{
		UiText::Muted("%s", PlayerCount::GetStatusText());
		return;
	}

	UiText::Good("%d players online", PlayerCount::Get());
}

void MainWindow::DrawSteamFolderWarning()
{
	const char* const steamFolder = InstallCheck::SteamFolderElsewhere();

	if (steamFolder == nullptr)
		return;

	char text[512] = {};
	sprintf_s(text, "Steam starts the game from another folder, so the mod does not load when you play "
		"from Steam. Install the mod in:\n%s", steamFolder);
	Cards::Problem(text);
}

void MainWindow::DrawGameBuildWarning()
{
	const GameBuild::Age age = GetGameBuildAge();

	if (age == GameBuild::Age::Supported)
		return;

	Cards::Problem(age == GameBuild::Age::Older
		? "This copy of the game is out of date. Game features stay off until it is updated."
		: "The game was updated. Game features stay off until the mod is updated too.");
}

void MainWindow::DrawTabs()
{
	struct Tab
	{
		MainTabs::Tab id;
		const char* label;
		void (MainWindow::*draw)();
		void (*hidden)();
	};

	static constexpr Tab kTabs[] = {
		{ MainTabs::Tab_Training, "Training", &MainWindow::DrawTrainingTab, nullptr },
		{ MainTabs::Tab_Visuals, "Visuals", &MainWindow::DrawVisualsTab, nullptr },
		{ MainTabs::Tab_Audio, "Audio", &MainWindow::DrawAudioTab, nullptr },
		{ MainTabs::Tab_Online, "Online", &MainWindow::DrawOnlineTab, nullptr },
		{ MainTabs::Tab_Mod, "Mod", &MainWindow::DrawModTab, nullptr },
		{ MainTabs::Tab_Debug, "Debug", &MainWindow::DrawDebugTab, nullptr },
		{ MainTabs::Tab_Config, "Config", &MainWindow::DrawConfigTab, &ConfigPanel::Hide },
	};

	CloseHiddenModuleWindows();
	const MainTabs::Shown contents = TabContents();

	if (!ImGui::BeginTabBar("##main", ImGuiTabBarFlags_FittingPolicyShrink))
		return;

	for (const Tab& tab : kTabs)
	{
		if (!MainTabs::Visible(tab.id, contents))
			continue;

		if (!ImGui::BeginTabItem(TrLabel(tab.label)))
		{
			if (tab.hidden != nullptr)
				tab.hidden();

			continue;
		}

		ImGui::Spacing();
		(this->*tab.draw)();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void MainWindow::DrawTrainingTab()
{
	TrainingPanel::Draw();
}

void MainWindow::DrawVisualsTab()
{
	DrawPerformanceSection();
	DrawStagesSection();
	DrawThemeSection();
	DrawMiscSection();
	DrawPalettesSection();
}

void MainWindow::DrawPalettesSection()
{
	if (!Features::Shows(FeatureCatalog::Id_Palettes))
		return;

	PalettesPanel::Draw();
}

void MainWindow::DrawAudioTab()
{
	DrawMusicSection();
	DrawSoundSection();
	DrawSubtitlesSection();
}

void MainWindow::DrawOnlineTab()
{
	DrawOnlineSection();
	DrawPatchSection();

	if (Features::Shows(FeatureCatalog::Id_Replays))
		ReplayPanel::Draw();
}

void MainWindow::DrawModTab()
{
	DrawModsSection();
}

void MainWindow::DrawDebugTab()
{
	DrawLogSection();
	DrawDebugViewSection();
}

void MainWindow::DrawLogSection()
{
	const Cards::Scope card("##log");

	if (!card)
		return;

	const bool forced = IsLoggingForced();
	bool logging = forced || IsLoggerOpen();

	ImGui::BeginDisabled(forced);

	if (ImGui::Checkbox(TrLabel("Write the mod's log"), &logging))
		SetLogging(logging);

	ImGui::EndDisabled();
	Description(forced ? "Always on in this build." :
		"Keeps the full log in the Logs folder. Turn it on before sending a bug report. Default: off.");
}

void MainWindow::DrawDebugViewSection()
{
	const Cards::Scope card("##debugView");

	if (!card)
		return;

	LauncherTitle("Debug view", WindowType_Debug);
	Description("Raw readouts of the game's memory and the search tools used to build the mod. "
		"Ctrl+F1 opens it too.");
}

void MainWindow::DrawConfigTab()
{
	ConfigPanel::Draw();
}

void MainWindow::DrawMusicSection()
{
	if (!Features::Shows(FeatureCatalog::Id_Music))
		return;

	const Cards::Scope card("##music");

	if (!card)
		return;

	LauncherTitle("Music", WindowType_Music);
	Description("Soundpacks, the track list and what plays where.");

	if (!BgmControl::IsHooked())
	{
		UiText::Warn("Music control is not active: %s", BgmControl::GetStatusText());
		return;
	}

	if (ImGui::Checkbox(TrLabel("Keep the menu music playing"), &g_modVals.keepMenuMusic))
		Settings::SaveInt("Music", "KeepMenuMusic", g_modVals.keepMenuMusic ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Normally the menu music starts over every time you come back from "
			"Options, Customize or Gallery. Turn this on to keep the same track playing across "
			"those screens."));
	}

	char playing[224] = {};

	if (!BgmNames::Describe(BgmControl::Current(), playing, sizeof(playing)))
		strncpy_s(playing, "silence", _TRUNCATE);

	UiText::Muted("Playing: %s", playing);
}

void MainWindow::DrawSoundSection()
{
	if (!Features::Shows(FeatureCatalog::Id_VoicesAndSound))
		return;

	const Cards::Scope card("##sound");

	if (!card)
		return;

	LauncherTitle("Voices and sound", WindowType_Sound);
	Description("Swap a character's voice or the shared sound effects.");
	UiText::Muted("%s", SoundPacks::StatusText());
}

void MainWindow::DrawSubtitlesSection()
{
	if (!Features::Shows(FeatureCatalog::Id_Subtitles))
		return;

	const Cards::Scope card("##subtitles");

	if (!card)
		return;

	LauncherTitle("Subtitles", WindowType_Subtitles);
	Description("On-screen lines for what each character says.");

	if (!g_modVals.subtitles)
	{
		UiText::Muted("Subtitles are off.");
		return;
	}

	UiText::Good("Subtitles are on. %d line(s) shown so far.", SubtitleWatch::Shows());
}

void MainWindow::DrawPerformanceSection()
{
	const Cards::Scope card("##performance");

	if (!card)
		return;

	if (LauncherTitle("Performance and graphics", WindowType_Performance) == nullptr)
	{
		UiText::Warn("The performance editor could not be created.");
		return;
	}

	Description("Resolution, shaders, frame pacing and POTATO MODE.");
}

void MainWindow::DrawPatchSection()
{
	if (!Features::Shows(FeatureCatalog::Id_GamePatches))
		return;

	const Cards::Scope card("##patches");

	if (!card)
		return;

	LauncherTitle("Game patches", WindowType_Patches);
	Description("An older version's battle data, so its replays play back right.");

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

	const Cards::Scope card("##theme");

	if (!card)
		return;

	LauncherTitle("Theme", WindowType_Theme);
	Description("Another French-Bread game's screens over UNI2's.");

	const ScreenTheme::Theme* const theme = ScreenTheme::Active();

	if (theme == nullptr)
	{
		UiText::Muted("Using UNI2's screens.");
		return;
	}

	UiText::Good("Applied: %s", theme->name.c_str());
	UiText::Muted("%s", ScreenDirector::StatusText());
}

void MainWindow::DrawStagesSection()
{
	if (!Features::Shows(FeatureCatalog::Id_Stages))
		return;

	const Cards::Scope card("##stages");

	if (!card)
		return;

	LauncherTitle("Stages", WindowType_Stages);
	Description("Hidden stages, and stages from other French-Bread games you have.");
	UiText::Good("%d/%d stages.", StageLibrary::Total(), BgCeiling::Numbers());
}

void MainWindow::DrawOnlineSection()
{
	if (!Features::Shows(FeatureCatalog::Id_NetplayWindow))
		return;

	const Cards::Scope card("##online");

	if (!card)
		return;

	LauncherTitle("Netplay", WindowType_Netplay);
	Description("Rollback, ping, the room, your match history and privacy.");

	if (g_modVals.censorNames)
		UiText::Good("Opponent names are being replaced with \"%s\".", NameCensor::Mask());
}

void MainWindow::DrawMiscSection()
{
	if (!Features::Shows(FeatureCatalog::Id_CharacterArt))
		return;

	const Cards::Scope card("##misc");

	if (!card)
		return;

	LauncherTitle("Character art and announcers", WindowType_Misc);
	Description("Alternate character art and the MELTY BLOOD TYPE LUMINA announcers.");
}


void MainWindow::DrawModsSection()
{
	if (!Features::Shows(FeatureCatalog::Id_ModPacks))
		return;

	const Cards::Scope card("##mods");

	if (!card)
		return;

	LauncherTitle("Mods", WindowType_Mods);
	Description("Folders in UNI2-IM\\Packs that replace game files.");

	if (ModPacks::Count() > 0)
	{
		UiText::Good("%d of %d mod(s) on, %d file(s).", ModPacks::EnabledCount(), ModPacks::Count(),
			ModPacks::FileCount());
	}
}
