#include "Overlay/UiScale.h"
#include "Overlay/Window/MainWindow.h"

#include "Overlay/Window/GraphicsPanel.h"

#include "Core/Hotkeys.h"
#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/KeyboardCapture.h"
#include "Core/keycodes.h"
#include "Core/PadInput.h"
#include "Core/Settings.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/GameTables.h"
#include "Game/KeyboardSeat.h"
#include "Game/ReplayFiles.h"
#include "Game/ScreenShake.h"
#include "Game/SteamNames.h"
#include "Game/OnlineState.h"
#include "Network/PaletteShare.h"
#include "Overlay/FrameMeterHud.h"
#include "Overlay/ComboNav.h"
#include "Overlay/NotificationBar.h"
#include "Overlay/Window/HitboxOverlay.h"
#include "Game/GamePatches.h"
#include "Game/BgmControl.h"
#include "Network/PlayerCount.h"
#include "Game/BgmNames.h"
#include "Screens/ScreenDirector.h"
#include "Screens/ScreenTheme.h"
#include "Overlay/UiText.h"
#include "Overlay/WindowManager.h"
#include "Web/UpdateCheck.h"
#include "Web/UpdateInstall.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteReport.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTexture.h"
#include "Training/FrameMeter.h"
#include "Training/FrameStepper.h"
#include "Training/DummyScript.h"
#include "Training/PlayerControl.h"
#include "Training/StageColor.h"

#include <Windows.h>

namespace {

constexpr const char* kDefaultPalette = "Default";

bool Contains(const std::string& text, const char* needle)
{
	if (needle == nullptr || needle[0] == '\0')
		return true;

	const auto fold = [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); };

	const size_t length = strlen(needle);
	if (length > text.size())
		return false;

	for (size_t at = 0; at + length <= text.size(); ++at)
	{
		size_t i = 0;
		while (i < length && fold(text[at + i]) == fold(needle[i]))
			++i;

		if (i == length)
			return true;
	}

	return false;
}

constexpr int kFunctionRow = Hotkeys::Action_Count;

int g_bindCapture = -1;
bool g_bindPad = false;

void SetBindCapture(int index, bool pad)
{
	g_bindCapture = index;
	g_bindPad = pad;
	KeyboardCapture::SetKeyCaptureActive(index >= 0 && !pad);
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
	DrawPlayerCount();
	ImGui::Separator();
	DrawTrainingSection();
	ImGui::Separator();
	DrawCustomSection();
	ImGui::Separator();
	DrawReplaySection();
	ImGui::Separator();
	DrawMusicSection();
	ImGui::Separator();
	DrawPerformanceSection();
	ImGui::Separator();
	DrawPatchSection();
	ImGui::Separator();
	DrawThemeSection();
	ImGui::Separator();
	DrawConfigSection();
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

	ImGui::TextWrapped("Soundpacks, the whole track list and the rules that decide what plays "
		"where all live in that window.");

	if (!BgmControl::IsHooked())
	{
		UiText::Warn("Music control is not active: %s", BgmControl::GetStatusText());
		return;
	}

	if (ImGui::Checkbox("Keep the menu music playing", &g_modVals.keepMenuMusic))
		Settings::SaveInt("Music", "KeepMenuMusic", g_modVals.keepMenuMusic ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The game's menu music chooser rebuilds the track from the start unless "
			"it is still running when you come back, and a trip into Options, Customize or Gallery "
			"always pauses it first - so it always restarts. On, the mod holds the paused track "
			"for the chooser and resumes it, and the music carries across those screens. Off is "
			"the game's own behaviour.");
	}

	char playing[224] = {};

	if (!BgmNames::Describe(BgmControl::Current(), playing, sizeof(playing)))
		strncpy_s(playing, "silence", _TRUNCATE);

	UiText::Muted("Playing: %s", playing);
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

	ImGui::TextWrapped("Frame pacing, POTATO MODE and where the time in each frame goes, with the "
		"knobs for all three, live in that window.");
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

	ImGui::TextWrapped("Play the game's battle data from an older build, so a replay recorded on "
		"it runs against the logic it was made under instead of today's.");

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

	ImGui::TextWrapped("A theme draws another French-Bread game's screens over UNI2's, leaving "
		"every UNI2 option where it is.");

	const ScreenTheme::Theme* const theme = ScreenTheme::Active();

	if (theme == nullptr)
	{
		UiText::Muted("Using the game's own screens.");
		return;
	}

	UiText::Good("Applied: %s", theme->name.c_str());
	UiText::Muted("%s", ScreenDirector::StatusText());
}

void MainWindow::DrawReplayPatchWarning()
{
	const int wanted = GamePatches::ReplayWanted();

	if (wanted < 0 || GamePatches::TablesAgreeWith(wanted))
		return;

	const GamePatches::Patch* const needs = GamePatches::Get(wanted);

	if (needs == nullptr)
		return;

	UiText::Muted("The last replay wanted %s. Start Replay loads it and plays on its own.",
		needs->name.c_str());
}

void MainWindow::DrawReplayAccounts()
{
	const int accounts = ReplayFiles::AccountCount();

	if (accounts < 2)
		return;

	const int selected = ReplayFiles::SelectedAccount();

	Ui::SetItemWidth(300.0f);

	if (ImGui::BeginCombo("Save folder", ReplayFiles::AccountLabel(selected).c_str()))
	{
		for (int i = 0; i < accounts; ++i)
		{
			if (ImGui::Selectable(ReplayFiles::AccountLabel(i).c_str(), i == selected))
				ReplayFiles::SelectAccount(i);
		}

		ImGui::EndCombo();
	}

	UiText::Help("The install carries more than one Steam account's saves. Export all reads the one "
		"picked here. An account that is not yours is read only - loading a file into the replay "
		"list always writes to your own.");

	if (!ReplayFiles::IsOwnAccount())
	{
		UiText::Warn("Reading another account's replays. Nothing is written to it, and new matches "
			"still save to yours.");
	}

	ImGui::Spacing();
}

void MainWindow::DrawReplaySection()
{
	if (!ImGui::CollapsingHeader("Replays"))
		return;

	const bool readable = ReplayFiles::IsAvailable();
	const bool live = ReplayFiles::IsLive();

	ImGui::TextWrapped("Every replay the game records from now on is written to UNI2-IM\\Replays as "
		"a file of its own, so one match can be sent to somebody without sending them all of "
		"them. Files are named after the two players; a name Steam cannot resolve is written as "
		"P1 or P2. The replays already in REP-DATA are left where they are until Export all is "
		"pressed.");

	ImGui::Spacing();

	bool automatic = ReplayFiles::GetAutoExport();
	if (ImGui::Checkbox("Save each new replay to a file", &automatic))
	{
		ReplayFiles::SetAutoExport(automatic);
		Settings::SaveInt("Replays", "AutoExport", automatic ? 1 : 0);
	}

	DrawReplayPatchWarning();
	DrawReplayAccounts();

	ImGui::BeginDisabled(!readable);

	if (ImGui::Button("Export all"))
	{
		std::string error;
		ReplayFiles::ExportAll(error);
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Anything already in the folder is left alone.");

	ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button("Open folder"))
		ShellExecuteA(nullptr, "open", ReplayFiles::GetFolder().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	ImGui::SameLine();

	if (ImGui::Button("Refresh"))
		ReplayFiles::Refresh();

	ImGui::Spacing();

	const std::vector<std::string>& files = ReplayFiles::ListFiles();
	static int picked = 0;

	if (picked >= static_cast<int>(files.size()))
		picked = 0;

	if (files.empty())
	{
		ImGui::TextDisabled("No replay files yet.");
	}
	else
	{
		static char search[64] = "";

		Ui::SetItemWidth(-1.0f);

		if (ImGui::BeginCombo("##replayfile", files[picked].c_str()))
		{
			if (ImGui::IsWindowAppearing())
			{
				search[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}

			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##replaysearch", "Search", search, sizeof(search));

			ImGui::Separator();

			for (int i = 0; i < static_cast<int>(files.size()); ++i)
			{
				if (!Contains(files[i], search))
					continue;

				const bool selected = i == picked;

				ImGui::PushID(i);

				if (ImGui::Selectable(files[i].c_str(), selected))
					picked = i;

				ComboNav::KeepSelectedInView(selected);

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		const int steps = ComboNav::WheelSteps();
		const int target = picked + steps;

		if (steps != 0 && target >= 0 && target < static_cast<int>(files.size()))
			picked = target;

		const bool canPlay = ReplayFiles::CanPlay();

		ImGui::BeginDisabled(!canPlay);

		if (ImGui::Button("Start Replay"))
		{
			std::string error;
			if (!ReplayFiles::RequestPlayback(ReplayFiles::GetFolder() + files[picked], error))
				NotificationBar::Add("%s", error.c_str());
		}

		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(canPlay
				? "Plays this file straight away, through the same call the Replay list's own "
				  "Playback runs, so both names load with it. It uses no slot and does not touch "
				  "REP-DATA."
				: "Not while a match is running.");
		}

		ImGui::SameLine();

		ImGui::BeginDisabled(!readable);

		if (ImGui::Button("Load into the game's replay list"))
		{
			std::string error;
			if (!ReplayFiles::Import(ReplayFiles::GetFolder() + files[picked], error))
				NotificationBar::Add("%s", error.c_str());
		}

		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Writes it into the oldest unprotected slot. The Replay screen sorts by date, so it "
				"appears at its own timestamp. Protect anything you want to keep there first.");
		}
	}

	if (!readable)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("No replays could be read - neither the game's own nor REP-DATA.");
		return;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("%d of %d slots used, replay format version %d%s",
		ReplayFiles::CountUsed(), ReplayFiles::kSlotCount, ReplayFiles::CurrentVersion(),
		live ? "" : " (read from REP-DATA - a load shows up after a restart)");

	ImGui::TextDisabled("Steam names: %s", SteamNames::GetStatus());

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Kept in UNI2-IM\\SteamNames.txt and reused every session. An account "
			"Steam cannot answer for is asked again at most once every 20 seconds.");
	}

	if (ReplayFiles::GetStatus()[0] != 0)
		ImGui::TextDisabled("%s", ReplayFiles::GetStatus());

}

void MainWindow::DrawCustomSection()
{
	if (!ImGui::CollapsingHeader("Custom"))
		return;

	if (!ImGui::BeginTabBar("##custom"))
		return;

	const bool live = ImGui::BeginTabItem("Palette");

	if (live)
	{
		WindowContainer* const container = WindowManager::GetInstance().GetContainer();
		IWindow* const window = container != nullptr
			? container->GetWindow(WindowType_Palette) : nullptr;

		if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close editor" : "Open editor"))
			window->Toggle();

		DrawPaletteChoosers();
		DrawPaletteOptions();

		ImGui::EndTabItem();
	}

	const bool colorCustomize = ImGui::BeginTabItem("Palette Nativa");

	if (colorCustomize)
	{
		m_colorCustomize.Draw();
		ImGui::EndTabItem();
	}

	if (g_modVals.showLegacyPalettes && ImGui::BeginTabItem("Palette (legacy)"))
	{
		DrawPalettesTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Player Card"))
	{
		m_playerCard.Draw();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void MainWindow::DrawPaletteChoosers()
{
	ImGui::SeparatorText("Character Palette");

	for (int player = 0; player < PaletteControl::kPlayers; ++player)
		DrawPaletteChooser(player);
}

void MainWindow::DrawPaletteChooser(int player)
{
	const int chara = PaletteMemory::GetCharaNumber(player);

	if (chara < 0)
	{
		ImGui::TextDisabled("P%d: nobody there yet", player + 1);
		return;
	}

	ImGui::PushID(player);

	ImGui::Text("P%d", player + 1);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", PaletteManager::GetCharaName(chara));

	const char* const worn = PaletteChoice::WornFile(player);
	const bool bare = worn[0] == '\0';

	ImGui::BeginDisabled(!PaletteControl::CanEdit(player));

	Ui::SetItemWidth(170.0f);

	if (ImGui::BeginCombo("##worn", bare ? kDefaultPalette : worn))
	{
		if (ImGui::Selectable(kDefaultPalette, bare))
			PaletteChoice::Bare(player);

		ComboNav::KeepSelectedInView(bare);

		for (int i = 0; i < PaletteLibrary::GetCount(chara); ++i)
		{
			const char* const file = PaletteLibrary::GetName(chara, i);
			const bool selected = !bare && strcmp(file, worn) == 0;

			ImGui::PushID(i);

			if (ImGui::Selectable(file, selected))
				PaletteChoice::Wear(player, file);

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();

	if (steps != 0)
		PaletteChoice::Step(player, steps);

	ImGui::SameLine();

	if (ImGui::Button("Rescan"))
		PaletteLibrary::Rescan(chara);

	ImGui::EndDisabled();

	ImGui::PopID();
}

void MainWindow::DrawPaletteOptions()
{
	if (ImGui::Checkbox("Group by part", &g_modVals.paletteGroupByPart))
		Settings::SaveInt("Palette", "GroupByPart", g_modVals.paletteGroupByPart ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Groups the entries the way the game's own colour screen does - hair, "
			"skin, boots - out of its colour-edit table rather than by guessing.");
	}

	if (ImGui::Checkbox("Flash the entry on the character", &g_modVals.paletteFlashEntry))
		Settings::SaveInt("Palette", "FlashEntry", g_modVals.paletteFlashEntry ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Picking an entry darkens everything else and blinks that entry on the "
			"character, so what it owns is unmistakable before you change it.");
	}

	if (ImGui::Checkbox("Filter junk colours", &g_modVals.paletteFilterJunk))
		Settings::SaveInt("Palette", "FilterJunk", g_modVals.paletteFilterJunk ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Hides the entries that are not really colours: the black padding, the "
			"green the unused slots are filled with, and anything that repeats an entry above it.");
	}

	if (ImGui::Checkbox("See the other player's colours", &g_modVals.showOnlinePalettes))
		Settings::SaveInt("Palette", "ShowOnlinePalettes", g_modVals.showOnlinePalettes ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("On, you see the palette they picked. Off, their side is left the way the "
			"game gives it. Yours is sent either way.");
	}

	ImGui::TextDisabled("%s", PaletteControl::IsSpectating()
		? "watching - the colours are the players' own"
		: (PaletteControl::LocalPlayer() >= 0
			? (PaletteControl::LocalPlayer() == 0 ? "you are playing P1" : "you are playing P2")
			: "both characters are yours to dress"));
}

void MainWindow::DrawPalettesTab()
{
	if (!GameState::AllowsPalettes())
	{
		ImGui::TextDisabled("In a match only.");
		return;
	}

	if (ImGui::Button("Load Palettes"))
		PaletteManager::Refresh();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Reads UNI2-IM\\Palettes again. One folder per character, named for the "
			"character. Drop a .pal in it - the game's own format, which Hantei-kun writes too - and "
			"it appears here. The folders are created the first time this runs.");
	}

	ImGui::SameLine();

	WindowContainer* palettes = WindowManager::GetInstance().GetContainer();
	IWindow* editor = palettes != nullptr ? palettes->GetWindow(WindowType_PaletteEditor)
		: nullptr;

	if (editor != nullptr && ImGui::Button(editor->IsOpen() ? "Close editor" : "Open editor"))
		editor->Toggle();

	ImGui::SameLine();

	if (ImGui::Button("Log palettes"))
	{
		std::string path;

		if (PaletteReport::Write(path))
			NotificationBar::Add("Palette report written to %s", path.c_str());
		else
			NotificationBar::Add("Could not write the palette report");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Writes everything the palette system currently believes to a text file in "
			"UNI2-IM\\Logs: which side the mod thinks it is, what each side is wearing, every "
			"texture it is tracking, and what the renderer was seen drawing.\n\n"
			"Take one on each machine at the same moment to find out where a shared palette went "
			"wrong. It works in this build - it does not need a logging one.");
	}

	ImGui::TextDisabled("(%s)", PaletteShare::GetStatusText());

	for (int player = 0; player < 2; ++player)
	{
		ImGui::PushID(player);

		const int chara = PaletteManager::GetCharaNumber(player);
		const int count = PaletteManager::GetCount(player);
		const int applied = PaletteManager::GetApplied(player);

		ImGui::Text("P%d  %s", player + 1, PaletteManager::GetCharaName(chara));

		if (PaletteTexture::FindForPlayer(player) < 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("- no palette texture for this side yet");
			ImGui::PopID();
			continue;
		}

		if (count == 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("- no palettes in its folder");
			ImGui::PopID();
			continue;
		}

		ImGui::SameLine();
		Ui::SetItemWidth(220.0f);

		int chosen = applied + 1;

		const char* const worn = GameTables::PaletteName(chara,
			PaletteMemory::GetPlayerSlot(player));

		if (ImGui::BeginCombo("##palette", chosen == 0 ? "Default"
			: PaletteManager::GetName(player, applied)))
		{
			if (ImGui::Selectable("Default", chosen == 0))
				PaletteManager::Restore(player);

			if (ImGui::IsItemHovered() && worn[0] != '\0')
				ImGui::SetTooltip("%s - the colours this character was picked with", worn);

			ComboNav::KeepSelectedInView(chosen == 0);

			for (int i = 0; i < count; ++i)
			{
				const bool selected = i == applied;

				ImGui::PushID(i);

				if (ImGui::Selectable(PaletteManager::GetName(player, i), selected))
					PaletteManager::Apply(player, i);

				const char* creator = PaletteManager::GetCreator(player, i);
				if (ImGui::IsItemHovered() && creator[0] != '\0')
					ImGui::SetTooltip("by %s", creator);

				ComboNav::KeepSelectedInView(selected);

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		StepPalette(player, applied, count, ComboNav::WheelSteps());

		ImGui::PopID();
	}
}

void MainWindow::StepPalette(int player, int applied, int count, int steps)
{
	if (steps == 0 || count == 0)
		return;

	int target = applied + steps;

	if (target < -1)
		target = -1;

	if (target >= count)
		target = count - 1;

	if (target == applied)
		return;

	if (target < 0)
	{
		PaletteManager::Restore(player);
		return;
	}

	PaletteManager::Apply(player, target);
}

void MainWindow::DrawTrainingSection()
{
	if (!ImGui::CollapsingHeader("Training", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (!GameState::AllowsTrainingTools())
	{
		if (OnlineState::IsBlind())
		{
			ImGui::TextDisabled("Steam networking never came up, so the mod cannot tell an online "
				"match from a local one. Battle modes it is sure about still work; the one that "
				"might be online is refused.");
		}
		else
		{
			ImGui::TextDisabled(OnlineState::IsOnline()
				? "Not while you are online."
				: "In a match only - training, replay, single player or local versus.");
		}
		return;
	}

	DrawHitboxControls();

	if (WindowContainer* const container = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const panel = container->GetWindow(WindowType_PlayerControl);

		bool open = panel != nullptr && panel->IsOpen();
		if (panel != nullptr && ImGui::Checkbox("Player Control", &open))
			open ? panel->Open() : panel->Close();
	}

	ImGui::Spacing();
	DrawFrameStepControls();
}

void MainWindow::DrawHitboxControls()
{
	WindowContainer* container = WindowManager::GetInstance().GetContainer();
	if (container == nullptr)
		return;

	HitboxOverlay* overlay = container->GetWindow<HitboxOverlay>(WindowType_HitboxOverlay);
	if (overlay == nullptr)
		return;

	bool open = overlay->IsOpen();
	if (ImGui::Checkbox("Hitbox viewer", &open))
		open ? overlay->Open() : overlay->Close();

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.toggleHitboxKey));

	bool meterVisible = FrameMeterHud::IsVisible();
	if (ImGui::Checkbox("Frame meter", &meterVisible))
		FrameMeterHud::SetVisible(meterVisible);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.toggleFrameMeterKey));
}

void MainWindow::DrawHitboxTypeControls()
{
	WindowContainer* container = WindowManager::GetInstance().GetContainer();
	if (container == nullptr)
		return;

	HitboxOverlay* overlay = container->GetWindow<HitboxOverlay>(WindowType_HitboxOverlay);
	if (overlay == nullptr)
		return;

	if (!ImGui::TreeNode("Hitbox types"))
		return;

	ImGui::Checkbox("Show Origin", &overlay->GetShowOrigin());
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("A cross at each object's own position - the point its boxes are measured "
			"from, and the point the game means when it talks about where a character is. Use it to "
			"tell whether a projectile's boxes belong to the projectile or to whoever fired it.");
	}

	const ImGuiStyle& style = ImGui::GetStyle();

	float widestName = 0.0f;
	for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
	{
		const float width = ImGui::CalcTextSize(HitboxOverlay::GetCategoryName(i)).x;

		if (width > widestName)
			widestName = width;
	}

	const float swatch = ImGui::GetFontSize();
	const float typeWidth = widestName + swatch + style.ItemSpacing.x + style.CellPadding.x * 2.0f;
	const float toggleWidth = ImGui::GetFrameHeight() + style.CellPadding.x * 2.0f;

	if (ImGui::BeginTable("boxtypes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, typeWidth);
		ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, toggleWidth);
		ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Outline", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
		{
			HitboxOverlay::CategorySettings& settings = overlay->GetCategory(i);

			ImGui::PushID(i);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::ColorButton("##swatch",
				ImGui::ColorConvertU32ToFloat4(HitboxOverlay::GetCategoryColor(i)),
				ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(swatch, swatch));

			ImGui::SameLine();
			ImGui::TextUnformatted(HitboxOverlay::GetCategoryName(i));

			ImGui::TableNextColumn();
			ImGui::Checkbox("##on", &settings.enabled);

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::SliderFloat("##fill", &settings.fillAlpha, 0.0f, 1.0f, "%.2f");

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::SliderFloat("##outline", &settings.outlineAlpha, 0.0f, 1.0f, "%.2f");

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::Spacing();

	if (WindowContainer* const legendContainer = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const legend = legendContainer->GetWindow(WindowType_HitboxLegend);
		if (legend != nullptr && ImGui::Button("Hitbox Doc."))
			legend->IsOpen() ? legend->Close() : legend->Open();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("What each box type is, what it can and cannot touch, and which moves "
				"actually use it. The swatches are the same colours the viewer draws with.");
		}
	}

	ImGui::TreePop();
}

void MainWindow::DrawFrameMeterControls()
{
	if (!ImGui::TreeNode("Frame meter options"))
		return;

	Ui::SetItemWidth(160.0f);
	ImGui::SliderFloat("Size", &g_modVals.frameMeterScale, 0.5f, 4.0f, "%.2fx");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveFloat("FrameMeter", "Scale", g_modVals.frameMeterScale);

	Ui::SetItemWidth(160.0f);
	ImGui::SliderInt("Opacity", &g_modVals.frameMeterOpacity, 10, 100, "%d%%");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("FrameMeter", "Opacity", g_modVals.frameMeterOpacity);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How solid the whole meter is drawn, bars, numbers and all. Turn it down to "
			"read the fight through it.");
	}

	if (ImGui::Checkbox("Count band", &g_modVals.frameMeterCounts))
		Settings::SaveInt("FrameMeter", "BandCounts", g_modVals.frameMeterCounts ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Prints how many frames each finished band lasted, at the end of the band. "
			"A band too short to hold its own number is left blank rather than drawn over the one "
			"beside it.");
	}

	ImGui::SameLine();
	if (ImGui::Checkbox("Hitstun, gap and flash", &g_modVals.frameMeterTotals))
		Settings::SaveInt("FrameMeter", "LineTotals", g_modVals.frameMeterTotals ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Blockstun, hitstun and the gap for the whole exchange, and the super flash "
			"inside the move above it, on a line of its own - above P1's numbers and below P2's, so "
			"each side's readouts sit together. The flash is the one of those the bar cannot show, "
			"since no cell is drawn for it.");
	}

	if (ImGui::Checkbox("Status Bar", &g_modVals.frameMeterAttributes))
		Settings::SaveInt("FrameMeter", "AttributeRow", g_modVals.frameMeterAttributes ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("A thin row under each bar naming everything the character is invincible to "
			"on that frame - throw, projectile, head, legs, air dive, and the two partial heights the "
			"frame data declares. White means nothing can connect at all, and it is drawn on its own. "
			"Turning this off removes the invincibility display entirely; the row is the only place "
			"it goes. Frame Meter Doc. names every colour.");
	}

	if (ImGui::Checkbox("Place automatically", &g_modVals.frameMeterAuto))
		Settings::SaveInt("FrameMeter", "PlaceAutomatically", g_modVals.frameMeterAuto ? 1 : 0);

	const bool automatic = g_modVals.frameMeterAuto;

	if (!automatic)
	{
		if (ImGui::Checkbox("Allow move meter with mouse", &g_modVals.frameMeterDrag))
			Settings::SaveInt("FrameMeter", "MouseDrag", g_modVals.frameMeterDrag ? 1 : 0);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("The meter is drawn straight onto the back buffer and has none of the "
				"overlay's hit testing, so a click that lands on it moves it whatever else you were "
				"doing.");
		}

		Ui::SetItemWidth(160.0f);
		ImGui::DragInt("X", &g_modVals.frameMeterX, 2.0f, 0, 4096);
		if (ImGui::IsItemDeactivatedAfterEdit())
			Settings::SaveInt("FrameMeter", "PositionX", g_modVals.frameMeterX);

		Ui::SetItemWidth(160.0f);
		ImGui::DragInt("Y", &g_modVals.frameMeterY, 2.0f, 0, 4096);
		if (ImGui::IsItemDeactivatedAfterEdit())
			Settings::SaveInt("FrameMeter", "PositionY", g_modVals.frameMeterY);
	}

	ImGui::Spacing();

	if (WindowContainer* const legendContainer = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const legend = legendContainer->GetWindow(WindowType_FrameMeterLegend);
		if (legend != nullptr && ImGui::Button("Frame Meter Doc."))
			legend->IsOpen() ? legend->Close() : legend->Open();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Every band and every status slice, with a sample meter and what the "
				"numbers are measuring. The swatches are drawn from the same colours the bar uses, so "
				"the page cannot go stale.");
		}
	}

	ImGui::TreePop();
}

void MainWindow::DrawFrameStepControls()
{
	if (!FrameStepper::IsImplemented())
		return;

	bool paused = FrameStepper::IsPaused();
	if (ImGui::Checkbox("Pause", &paused))
		FrameStepper::SetPaused(paused);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.freezeFrameKey));

	ImGui::BeginDisabled(!paused);

	if (ImGui::Button("Next frame"))
		FrameStepper::RequestStep(1);

	ImGui::SameLine();
	ImGui::TextDisabled("(%s)", GetNameFromVirtualKey(g_modVals.stepForwardKey));

	ImGui::EndDisabled();

	DrawFreezeModeCombo();
	DrawAutoPauseControls();
	DrawTimingControls();
	DrawDummyScriptControls();
	DrawExtrasControls();
	DrawHitboxTypeControls();
	DrawFrameMeterControls();
	PersistAutoPause();
}

namespace {

void DrawStopList(const char* label, const char* tooltip, bool& enabled, int* stops)
{
	ImGui::PushID(label);
	ImGui::Checkbox(label, &enabled);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(340.0f);
		ImGui::TextUnformatted(tooltip);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::BeginDisabled(!enabled);

	int used = 0;
	int highest = 0;
	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (stops[i] <= 0)
			continue;

		++used;
		if (stops[i] > highest)
			highest = stops[i];
	}

	int removeAt = -1;

	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (stops[i] <= 0)
			continue;

		ImGui::PushID(i);
		Ui::SetItemWidth(70.0f);

		if (ImGui::InputInt("##stop", &stops[i], 0))
		{
			if (stops[i] < 1)
				stops[i] = 1;
			if (stops[i] > FrameMeter::kMaxComboHits)
				stops[i] = FrameMeter::kMaxComboHits;
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("-"))
			removeAt = i;

		ImGui::PopID();
	}

	if (removeAt >= 0)
	{
		for (int i = removeAt; i + 1 < FrameMeter::kComboStops; ++i)
			stops[i] = stops[i + 1];

		stops[FrameMeter::kComboStops - 1] = 0;
	}

	if (used < FrameMeter::kComboStops && ImGui::SmallButton("Add"))
	{
		for (int i = 0; i < FrameMeter::kComboStops; ++i)
		{
			if (stops[i] > 0)
				continue;

			stops[i] = highest + 1 > FrameMeter::kMaxComboHits
				? FrameMeter::kMaxComboHits : highest + 1;
			break;
		}
	}

	ImGui::EndDisabled();
	ImGui::PopID();
}

}

int ManualResumeDelay(const FrameMeter::AutoPauseConfig& config)
{
	return config.resumeDelay && config.delayOnManualPause ? config.resumeDelayFrames : 0;
}

void MainWindow::DrawTimingControls()
{
	FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const FrameMeter::AutoPauseConfig before = config;

	if (!ImGui::TreeNode("Timing"))
		return;

	Ui::SetItemWidth(90.0f);
	if (ImGui::InputInt("frames", &config.resumeDelayFrames, 0))
	{
		if (config.resumeDelayFrames < FrameMeter::kMinResumeFrames)
			config.resumeDelayFrames = FrameMeter::kMinResumeFrames;
		if (config.resumeDelayFrames > FrameMeter::kMaxResumeFrames)
			config.resumeDelayFrames = FrameMeter::kMaxResumeFrames;
	}

	ImGui::TextDisabled("Dummy lead-in");
	ImGui::Checkbox("Recording", &config.onDummyRecord);

	ImGui::BeginDisabled(!config.onDummyRecord);
	Ui::SetItemWidth(260.0f);

	static const char* kLeadInModes[] =
	{
		"Warm up",
		"Still"
	};

	ImGui::Combo("##leadinmode", &config.leadInMode, kLeadInModes, 2);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Warm up leaves everyone free to move; Still stops the tick. Both drop "
			"the countdown from the take.");

	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::TextDisabled("Delay before resuming a pause");

	ImGui::Checkbox("Move starts##delay", &config.delayOnMoveStarts);
	ImGui::SameLine();
	ImGui::Checkbox("Hit lands##delay", &config.delayOnHitLands);
	ImGui::SameLine();
	ImGui::Checkbox("Manual pause##delay", &config.delayOnManualPause);

	ImGui::Checkbox("Combo count##delay", &config.delayOnComboReaches);
	ImGui::SameLine();
	ImGui::Checkbox("Block count##delay", &config.delayOnBlockReaches);

	config.resumeDelay = config.delayOnMoveStarts || config.delayOnHitLands ||
		config.delayOnComboReaches || config.delayOnBlockReaches || config.delayOnManualPause;

	ImGui::TreePop();

	if (memcmp(&config, &before, sizeof(config)) != 0)
		FrameMeter::SetAutoPause(config);
}

void MainWindow::DrawAutoPauseControls()
{
	FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	FrameStepper::SetManualResumeDelay(ManualResumeDelay(config));
	const FrameMeter::AutoPauseConfig before = config;

	if (!ImGui::TreeNode("Auto pause"))
		return;

	ImGui::Checkbox("P1", &config.player[0]);
	ImGui::SameLine();
	ImGui::Checkbox("P2", &config.player[1]);
	ImGui::SameLine();
	ImGui::TextDisabled("Watch");

	ImGui::BeginTable("autopausetriggers", 2, ImGuiTableFlags_SizingStretchSame);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGui::Checkbox("Move starts", &config.onMoveStarts);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(340.0f);
		ImGui::TextUnformatted("The move's first startup frame. Jumps, dashes and assaults are drawn "
			"as movement rather than as a move, so they cannot trigger this.");
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::TableNextColumn();
	ImGui::Checkbox("Hit lands", &config.onHit);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(340.0f);
		ImGui::TextUnformatted("Every connected hit, read from the game's own combo counter. A "
			"blocked hit does not count.");
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::EndTable();

	if (ImGui::BeginTable("autopausestops", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		DrawStopList("Combo count", "Stops on each of these hit counts. Set 3 and 20 to stop "
			"twice in the same combo.", config.onComboHits, config.comboStop);

		ImGui::TableNextColumn();
		DrawStopList("Block count", "Stops on each of these blocked hit counts, counted the way the "
			"option below selects.\n\n"
			"Note: a multi-hit attack on block is not consistent - sometimes the whole move "
			"registers as one blocked hit, sometimes as several.", config.onBlockedHits,
			config.blockStop);

		ImGui::EndTable();
	}

	int blockedMode = config.blockedAllowsGaps ? 1 : 0;
	Ui::SetItemWidth(220.0f);
	static const char* kBlockedModes[] = { "True blockstring only", "Any blocked hits" };
	if (ImGui::Combo("##blockcounting", &blockedMode, kBlockedModes, 2))
		config.blockedAllowsGaps = blockedMode == 1;

	ImGui::TreePop();

	if (memcmp(&config, &before, sizeof(config)) != 0)
		FrameMeter::SetAutoPause(config);
}

void MainWindow::DrawExtrasControls()
{
	FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const FrameMeter::AutoPauseConfig before = config;

	if (!ImGui::TreeNode("Extras"))
		return;

	ImGui::Checkbox("Reversal action after restart", &config.reversalAfterRestart);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("After the training restart, the dummy performs whatever the game's "
			"Reversal action page has ticked, at the first frame it can act. Aerial actions need "
			"the dummy to be airborne, so they will not come out from a standing restart.");

	ImGui::BeginDisabled(!config.reversalAfterRestart);
	ImGui::Checkbox("Count down first", &config.reversalAfterCountdown);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The restart freezes with hitstop for the frame count in Timing, and the "
			"reversal is armed only once that has run out.");

	ImGui::EndDisabled();

	ImGui::Separator();
	DrawStageColourControls();

	ImGui::TreePop();

	if (memcmp(&config, &before, sizeof(config)) != 0)
		FrameMeter::SetAutoPause(config);
}

namespace {

struct ScriptTab
{
	char text[1024];
	char error[128];
	char name[64];
	int slot;
	int selected;
};

ScriptTab g_tabs[2] =
{
	{ "", "", "script", 0, -1 },
	{ "W12\n[4]\nW30\n]4[\n", "", "script", 1, -1 },
};

}

void MainWindow::DrawDummyScriptControls()
{
	if (!ImGui::TreeNode("Dummy script"))
		return;

	DummyScript::RefreshLibrary();

	if (ImGui::BeginTabBar("##scriptsides"))
	{
		for (int i = 0; i < 2; ++i)
		{

			const int player = i == 0 ? 1 : 0;

			char label[32] = {};
			sprintf_s(label, "P%d  %s", player + 1,
				PaletteManager::GetCharaName(PaletteManager::GetCharaNumber(player)));

			if (ImGui::BeginTabItem(label))
			{
				ImGui::PushID(player);
				DrawScriptTab(player);
				ImGui::PopID();

				ImGui::EndTabItem();
			}
		}

		ImGui::EndTabBar();
	}

	ImGui::TreePop();
}

void MainWindow::DrawScriptTab(int player)
{
	ScriptTab& tab = g_tabs[player];

	ImGui::InputTextMultiline("##dummyscript", tab.text, sizeof(tab.text),
		ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8.0f));

	const bool slotSide = player == 1;
	const bool running = slotSide ? DummyScript::IsPlaying() : PlayerControl::IsScriptRunning(player);

	if (slotSide)
	{
		ImGui::Text("Slot");
		ImGui::SameLine();

		Ui::SetItemWidth(110.0f);
		ImGui::InputInt("##slot", &tab.slot);
		tab.slot = tab.slot < 1 ? 1 : (tab.slot > GameOffsets::kRecorderSlotCount
			? GameOffsets::kRecorderSlotCount : tab.slot);

		ImGui::SameLine();
	}

	if (ImGui::Button(running ? "Stop" : "Play"))
	{
		if (running)
		{
			slotSide ? DummyScript::Stop() : PlayerControl::StopScript(player);
		}
		else if (DummyScript::Parse(tab.text, tab.error, sizeof(tab.error)))
		{
			if (slotSide)
			{
				if (DummyScript::WriteToSlot(tab.slot - 1))
					DummyScript::Play(tab.slot - 1);
			}
			else
			{
				PlayerControl::RunScript(player,
					DummyScript::GetFrames(),
					DummyScript::GetFrameCount());
			}
		}
		else if (tab.error[0] == '\0')
		{

			sprintf_s(tab.error, "nothing to play - the script is empty");
		}
	}

	if (slotSide)
	{
		if (ImGui::Button("Write to slot"))
		{
			if (DummyScript::Parse(tab.text, tab.error, sizeof(tab.error)))
				DummyScript::WriteToSlot(tab.slot - 1);
		}

		ImGui::SameLine();
		if (ImGui::Button("Read from slot"))
		{
			if (DummyScript::ReadFromSlot(tab.slot - 1, tab.text, sizeof(tab.text)))
				tab.error[0] = '\0';
		}
	}

	ImGui::SameLine();
	if (tab.error[0] != '\0')
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", tab.error);
	else if (running)
		ImGui::TextDisabled("frame %d", slotSide ? DummyScript::GetPlaybackFrame()
			: PlayerControl::GetScriptFrame(player));
	else
		ImGui::TextDisabled("%d frames", DummyScript::GetFrameCount());

	Ui::SetItemWidth(130.0f);
	ImGui::InputText("##scriptname", tab.name, sizeof(tab.name));

	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		DummyScript::Save(player, tab.name, tab.text);
		tab.selected = -1;
	}

	const int count = DummyScript::GetLibraryCount();
	const int chara = PaletteManager::GetCharaNumber(player);

	Ui::SetItemWidth(200.0f);
	if (ImGui::BeginCombo("##scriptlibrary",
		tab.selected >= 0 && tab.selected < count ? DummyScript::GetLibraryName(tab.selected)
		: "Saved scripts"))
	{
		int shown = 0;

		for (int i = 0; i < count; ++i)
		{

			if (DummyScript::GetLibraryChara(i) != chara)
				continue;

			++shown;

			if (!ImGui::Selectable(DummyScript::GetLibraryName(i), i == tab.selected))
				continue;

			tab.selected = i;
			strncpy_s(tab.name, DummyScript::GetLibraryName(i), _TRUNCATE);

			if (DummyScript::LoadFromLibrary(i, tab.text, sizeof(tab.text)))
				tab.error[0] = '\0';
		}

		if (shown == 0)
			ImGui::TextDisabled("nothing saved for this character");

		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(tab.selected < 0 || tab.selected >= count);

	if (ImGui::Button("Delete"))
	{
		DummyScript::DeleteFromLibrary(tab.selected);
		tab.selected = -1;
	}

	ImGui::EndDisabled();

	if (WindowContainer* const guideContainer = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const guide = guideContainer->GetWindow(WindowType_DummyScriptGuide);
		if (guide != nullptr && ImGui::Button("Dummy Script Doc."))
			guide->IsOpen() ? guide->Close() : guide->Open();
	}
}

void MainWindow::DrawStageColourControls()
{

	constexpr uint32_t kGreenScreen = 0x00ff00;

	bool enabled = StageColor::IsEnabled();

	if (ImGui::Checkbox("Flat colour stage", &enabled))
	{
		StageColor::SetEnabled(enabled);
		Settings::SaveInt("Video", "FlatStage", enabled ? 1 : 0);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Replaces the stage with a solid colour for chroma keying. Characters, "
			"effects and the HUD are untouched.");
	}

	if (!enabled)
		return;

	const uint32_t rgb = StageColor::GetColor();
	float colour[3] = {
		((rgb >> 16) & 0xff) / 255.0f,
		((rgb >> 8) & 0xff) / 255.0f,
		(rgb & 0xff) / 255.0f
	};

	if (ImGui::ColorEdit3("##stagecolour", colour,
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
	{
		const uint32_t packed =
			(static_cast<uint32_t>(colour[0] * 255.0f + 0.5f) << 16) |
			(static_cast<uint32_t>(colour[1] * 255.0f + 0.5f) << 8) |
			static_cast<uint32_t>(colour[2] * 255.0f + 0.5f);

		StageColor::SetColor(packed);
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Video", "FlatStageColour", static_cast<int>(StageColor::GetColor()));

	ImGui::SameLine();
	if (ImGui::Button("Green"))
	{
		StageColor::SetColor(kGreenScreen);
		Settings::SaveInt("Video", "FlatStageColour", static_cast<int>(kGreenScreen));
	}
}

void MainWindow::PersistAutoPause()
{
	const FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const int packed = FrameMeter::PackAutoPause(config);

	static bool s_known = false;
	static int s_lastPacked = 0;
	static int s_lastFrames = 0;

	if (s_known && packed == s_lastPacked && config.resumeDelayFrames == s_lastFrames)
		return;

	s_known = true;
	s_lastPacked = packed;
	s_lastFrames = config.resumeDelayFrames;

	FrameStepper::SetManualResumeDelay(ManualResumeDelay(config));

	Settings::SaveInt("Training", "AutoPauseOnAttack", packed);
	Settings::SaveInt("Training", "ResumeDelayFrames", config.resumeDelayFrames);

	char stops[64] = {};
	int written = 0;
	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (config.comboStop[i] <= 0)
			continue;

		written += sprintf_s(stops + written, sizeof(stops) - written, written == 0 ? "%d" : ",%d",
			config.comboStop[i]);
	}

	Settings::SaveString("Training", "AutoPauseComboStops", stops);

	char blockStops[64] = {};
	written = 0;
	for (int i = 0; i < FrameMeter::kComboStops; ++i)
	{
		if (config.blockStop[i] <= 0)
			continue;

		written += sprintf_s(blockStops + written, sizeof(blockStops) - written,
			written == 0 ? "%d" : ",%d", config.blockStop[i]);
	}

	Settings::SaveString("Training", "AutoPauseBlockStops", blockStops);
}

void MainWindow::DrawFreezeModeCombo()
{
	const FrameStepper::FreezeMode modes[] =
	{
		FrameStepper::FreezeMode::TickSuppress,
		FrameStepper::FreezeMode::StopTime
	};

	const bool forced = FrameStepper::IsModeForced();
	const FrameStepper::FreezeMode current = FrameStepper::GetEffectiveMode();

	Ui::SetItemWidth(160.0f);
	ImGui::BeginDisabled(forced);

	if (ImGui::BeginCombo("Pause mode", FrameStepper::GetModeName(current)))
	{
		for (FrameStepper::FreezeMode mode : modes)
		{
			const bool supported = FrameStepper::IsModeSupported(mode);

			ImGui::BeginDisabled(!supported);
			if (ImGui::Selectable(FrameStepper::GetModeName(mode), mode == current))
			{
				FrameStepper::SetMode(mode);
				Settings::SaveInt("Training", "FreezeMode",
					mode == FrameStepper::FreezeMode::StopTime ? 1 : 0);
			}
			ImGui::EndDisabled();
		}

		ImGui::EndCombo();
	}

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(forced
			? "A replay always pauses with Tick stop.\nHitstun Stop leaves the tick running, and in a "
			  "replay the tick is what plays the recorded inputs - the recording would keep going "
			  "behind a still picture."
			: "Tick stop halts the whole game, menus included, and replays the last rendered frame.\n"
			  "Hitstun Stop uses the engine's own hitstop: the menus keep working, but visual effects "
			  "render wrong while held.");
	}

	if (forced)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(forced - replay or watch mode)");
	}
}

void MainWindow::DrawKeyboardTab()
{
	ImGui::TextWrapped("The game gives the keyboard and the first controller the same player "
		"number, so in local versus they drive the same character. Pick a side for the keyboard "
		"here and the controller moves to the other one. Your key settings are never touched.");

	ImGui::Spacing();

	if (!KeyboardSeat::IsAvailable())
	{
		ImGui::TextDisabled("The game is not mapped yet.");
		return;
	}

	const int seats[] = { KeyboardSeat::Seat_Default, KeyboardSeat::Seat_P1, KeyboardSeat::Seat_P2 };
	const int current = KeyboardSeat::GetSeat();

	Ui::SetItemWidth(160.0f);

	if (ImGui::BeginCombo("Keyboard plays", KeyboardSeat::GetSeatName(current)))
	{
		for (int seat : seats)
		{
			if (ImGui::Selectable(KeyboardSeat::GetSeatName(seat), seat == current))
				KeyboardSeat::SetSeat(seat);
		}

		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Game default leaves the keyboard where the game puts it.\n"
			"1P and 2P keep your keys exactly as configured and move the controller to the other side.");
	}

	bool route = KeyboardSeat::GetRouteSides();
	if (ImGui::Checkbox("Hold the side during a match", &route))
		KeyboardSeat::SetRouteSides(route);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Writes both sides' controller slots every frame, so the side you picked is "
			"the side you get. Off, the game decides who joins where and only the controller moves.");
	}

	ImGui::Spacing();
	ImGui::TextWrapped("If you have a second keyboard player configured in the game's own options, "
		"those keys will answer on the controller's side. Set Keyboard Player Number to 1 there to "
		"switch them off.");

	ImGui::Spacing();
	ImGui::TextDisabled("%s", KeyboardSeat::GetStatus());

	if (OnlineState::IsOnline())
		ImGui::TextDisabled("Online: the sides are left alone until the match ends.");
}

void MainWindow::DrawConfigSection()
{
	if (!ImGui::CollapsingHeader("Config"))
	{
		SetBindCapture(-1, false);
		return;
	}

	if (!ImGui::BeginTabBar("##config"))
		return;

	if (ImGui::BeginTabItem("General"))
	{
		DrawConfigGeneralTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Keybinds"))
	{
		DrawKeybindsTab();
		ImGui::EndTabItem();
	}
	else
	{
		SetBindCapture(-1, false);
	}

	if (ImGui::BeginTabItem("Keyboard"))
	{
		DrawKeyboardTab();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void MainWindow::DrawConfigGeneralTab()
{
	ImGui::Text("%s %s", UNI2_IM_NAME, UNI2_IM_VERSION);
	ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

	if (ImGui::Checkbox("Check for updates on start", &g_modVals.checkForUpdates))
		Settings::SaveInt("Mod", "CheckForUpdates", g_modVals.checkForUpdates ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Asks GitHub once, on a thread of its own, whether a newer release exists. "
			"Nothing is downloaded until you ask for it.");
	}

	ImGui::BeginDisabled(UpdateCheck::IsChecking() || UpdateInstall::IsBusy());

	if (ImGui::Button("Check now"))
		UpdateCheck::Refresh();

	ImGui::EndDisabled();

	if (UpdateCheck::HasNewer())
	{
		ImGui::SameLine();

		if (ImGui::Button("Show the update"))
			WindowManager::GetInstance().OpenUpdateNotifier();
	}

	UiText::Muted("%s", UpdateCheck::GetStatusText());

	if (ImGui::Checkbox("Keep the hitboxes and the meter up in the game's pause",
		&g_modVals.drawWhilePaused))
	{
		Settings::SaveInt("Overlay", "DrawWhileGamePaused", g_modVals.drawWhilePaused ? 1 : 0);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The game's own pause menu stops the battle tick, and both overlays hide "
			"with it because a stopped tick usually means the match has ended. On, they stay up "
			"while the match does - the characters are frozen behind the menu, not gone.");
	}

	bool& blockMouse = WindowManager::GetInstance().GetBlockGameMouse();
	if (ImGui::Checkbox("Block mouse input to the game", &blockMouse))
		Settings::SaveInt("Overlay", "BlockGameMouse", blockMouse ? 1 : 0);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Stops the game from seeing the mouse at all, so clicking the overlay can "
			"never disturb it.\nSaved to the ini as soon as it changes.");

	int shake = ScreenShake::GetIntensity();

	ImGui::BeginDisabled(!ScreenShake::IsAvailable());

	Ui::SetItemWidth(160.0f);

	if (ImGui::SliderInt("Screen shake strength", &shake, 0, ScreenShake::kFullPercent,
		"%d%%"))
		ScreenShake::SetIntensity(shake);

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		g_modVals.screenShake = ScreenShake::GetIntensity();
		Settings::SaveInt("Video", "ScreenShake", g_modVals.screenShake);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Every shake in the game - Wald's walk, the heavy hits, the cutscenes - "
			"is one call asking the camera to quake, and the slot it fills carries a percentage "
			"the engine multiplies the amplitude by. This rescales that, so a shake keeps its "
			"shape, its length and its timing and only moves less. 100%% is the game's own; 0 "
			"answers the call with a duration of zero, which is how the engine cancels a shake "
			"itself.");
	}

	ImGui::EndDisabled();

	if (!ScreenShake::IsAvailable())
		UiText::Warn("%s", ScreenShake::StatusText());

	GraphicsPanel::DrawOverlayAppearance();

	ImGui::SeparatorText("Holding the next-frame key");

	Ui::SetItemWidth(160.0f);
	ImGui::SliderInt("Wait before repeating", &g_modVals.stepRepeatDelayMs, 0, 1000, "%d ms");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Training", "StepRepeatDelayMs", g_modVals.stepRepeatDelayMs);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How long the key has to be held before it starts stepping on its own. A "
			"tap is always one frame, whatever this says.");
	}

	Ui::SetItemWidth(160.0f);
	ImGui::SliderInt("Between steps", &g_modVals.stepRepeatIntervalMs, 16, 500, "%d ms");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Training", "StepRepeatIntervalMs", g_modVals.stepRepeatIntervalMs);

	if (ImGui::IsItemHovered())
	{

		ImGui::SetTooltip("How long between steps once it is repeating - %d a second. It is checked "
			"once a frame, so the real spacing rounds to whole frames: at 60 Hz anything from 34 to "
			"49 ms steps every third one.",
			1000 / (g_modVals.stepRepeatIntervalMs > 0 ? g_modVals.stepRepeatIntervalMs : 1));
	}

}

void MainWindow::DrawKeybindsTab()
{
	if (g_bindCapture >= 0)
		CaptureBind();

	DrawFunctionBinds();

	if (!ImGui::BeginTable("##keybinds", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("What");
	ImGui::TableSetupColumn("Key");
	ImGui::TableSetupColumn("Pad");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();

	for (int i = 0; i < Hotkeys::Action_Count; ++i)
		DrawBindRow(static_cast<Hotkeys::Action>(i));

	ImGui::EndTable();

	if (g_bindCapture >= 0)
		ImGui::TextDisabled("%s", g_bindPad ? "Press a pad button, or Escape to cancel."
			: "Press the key you want, or Escape to cancel.");
	else
		ImGui::TextDisabled("Saved to UNI2_IM.ini as soon as something is bound.");

	DrawBindConflicts();
}

void MainWindow::CaptureBind()
{
	const Hotkeys::Action action = static_cast<Hotkeys::Action>(g_bindCapture);

	if (!g_bindPad)
		KeyboardCapture::SetKeyCaptureActive(true);

	const int pressed = PollPressedKey();

	if (pressed == VK_ESCAPE)
	{
		SetBindCapture(-1, false);
		return;
	}

	if (g_bindPad)
	{
		const int button = PadInput::PollPressedButton();

		if (button == PadInput::kNone)
			return;

		if (g_bindCapture == kFunctionRow)
			Hotkeys::SetFunctionButton(button);
		else
			Hotkeys::SetPadButton(action, button);

		SetBindCapture(-1, false);
		return;
	}

	if (pressed == 0)
		return;

	if (g_bindCapture == kFunctionRow)
		Hotkeys::SetFunctionKey(pressed);
	else
		Hotkeys::SetKey(action, pressed, Hotkeys::GetKeyNeedsFunction(action));

	SetBindCapture(-1, false);
}

void MainWindow::DrawFunctionBinds()
{
	ImGui::TextUnformatted("Function");
	ImGui::SameLine();
	ImGui::TextDisabled("held with another key or button, the way a fighting game does shortcuts");

	const bool keyCapturing = g_bindCapture == kFunctionRow && !g_bindPad;
	const bool padCapturing = g_bindCapture == kFunctionRow && g_bindPad;

	const int key = Hotkeys::GetFunctionKey();

	ImGui::PushID("function");

	if (ImGui::SmallButton(keyCapturing ? "Cancel" : "Change key"))
		SetBindCapture(keyCapturing ? -1 : kFunctionRow, false);

	ImGui::SameLine();
	ImGui::TextUnformatted(keyCapturing ? "press a key"
		: (key != 0 ? GetNameFromVirtualKey(key) : "none"));

	ImGui::SameLine();

	if (ImGui::SmallButton(padCapturing ? "Cancel##pad" : "Change button"))
		SetBindCapture(padCapturing ? -1 : kFunctionRow, true);

	ImGui::SameLine();
	ImGui::TextUnformatted(padCapturing ? "press a button"
		: PadInput::GetButtonName(Hotkeys::GetFunctionButton()));

	ImGui::SameLine();
	ImGui::TextDisabled(PadInput::IsConnected() ? "(pad found)" : "(no pad)");

	ImGui::PopID();

	if (key == 0)
		ImGui::TextDisabled("Without a function key, a keyboard bind marked Fn cannot fire.");
}

void MainWindow::DrawBindRow(Hotkeys::Action action)
{
	const bool keyCapturing = g_bindCapture == action && !g_bindPad;
	const bool padCapturing = g_bindCapture == action && g_bindPad;

	const int key = Hotkeys::GetKey(action);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(Hotkeys::GetLabel(action));

	ImGui::TableNextColumn();
	ImGui::PushID(Hotkeys::GetSettingKey(action));

	ImGui::TextUnformatted(keyCapturing ? "press a key"
		: (key != 0 ? GetNameFromVirtualKey(key) : "none"));

	ImGui::SameLine();

	bool needsFunction = Hotkeys::GetKeyNeedsFunction(action);

	if (ImGui::Checkbox("Fn", &needsFunction))
		Hotkeys::SetKey(action, key, needsFunction);

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(padCapturing ? "press a button"
		: PadInput::GetButtonName(Hotkeys::GetPadButton(action)));

	ImGui::TableNextColumn();

	if (ImGui::SmallButton(keyCapturing ? "Cancel" : "Key"))
		SetBindCapture(keyCapturing ? -1 : action, false);

	ImGui::SameLine();

	if (ImGui::SmallButton(padCapturing ? "Cancel##pad" : "Pad"))
		SetBindCapture(padCapturing ? -1 : action, true);

	ImGui::SameLine();

	if (ImGui::SmallButton("Clear"))
	{
		Hotkeys::SetKey(action, 0, false);
		Hotkeys::SetPadButton(action, PadInput::kNone);
	}

	ImGui::PopID();
}

void MainWindow::DrawBindConflicts()
{
	for (int i = 0; i < Hotkeys::Action_Count; ++i)
	{
		const Hotkeys::Action mine = static_cast<Hotkeys::Action>(i);

		for (int k = i + 1; k < Hotkeys::Action_Count; ++k)
		{
			const Hotkeys::Action theirs = static_cast<Hotkeys::Action>(k);

			if (Hotkeys::GetKey(mine) == 0 || Hotkeys::GetKey(mine) != Hotkeys::GetKey(theirs))
				continue;

			if (Hotkeys::GetKeyNeedsFunction(mine) != Hotkeys::GetKeyNeedsFunction(theirs))
				continue;

			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.45f, 1.0f), "%s and %s are both %s.",
				Hotkeys::GetLabel(mine), Hotkeys::GetLabel(theirs),
				GetNameFromVirtualKey(Hotkeys::GetKey(mine)));
		}
	}
}
