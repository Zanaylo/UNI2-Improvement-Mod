#include "Overlay/Window/MainWindow.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/KeyboardCapture.h"
#include "Core/keycodes.h"
#include "Core/Settings.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/GameTables.h"
#include "Game/OnlineState.h"
#include "Network/PaletteShare.h"
#include "Overlay/FrameMeterHud.h"
#include "Overlay/ComboNav.h"
#include "Overlay/NotificationBar.h"
#include "Overlay/Window/HitboxOverlay.h"
#include "Overlay/WindowManager.h"
#include "Palette/PaletteControl.h"
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

int g_bindCapture = -1;

void SetBindCapture(int index)
{
	g_bindCapture = index;
	KeyboardCapture::SetKeyCaptureActive(index >= 0);
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

	ImGui::SetNextWindowSizeConstraints(ImVec2(max(titleWidth + decorations, base), 0.0f),
		ImVec2(FLT_MAX, FLT_MAX));

	ImGui::SetNextWindowSize(ImVec2(base, 0.0f), ImGuiCond_FirstUseEver);
}

void MainWindow::Draw()
{
	DrawTrainingSection();
	ImGui::Separator();
	DrawCustomSection();
	ImGui::Separator();
	DrawConfigSection();
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

		DrawPaletteOptions();

		ImGui::EndTabItem();
	}

	const bool colorCustomize = ImGui::BeginTabItem("Palette Nativa");

	if (colorCustomize)
	{
		m_colorCustomize.Draw();
		ImGui::EndTabItem();
	}

	// The first palette system is kept for its machinery but no longer named anywhere in the
	// interface; the tab that replaces it is not built yet. docs/PALETTES.md has the plan.
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

// The editor's own switches live here rather than inside it: they are about how you work, not
// about a particular character, and the editor wants its room for colours.
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
		ImGui::SetNextItemWidth(220.0f);

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

// Kept out of the two checkboxes above and drawn at the bottom of the section instead, so the panel
// does not reflow every time the viewer or the meter is switched on.
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

	if (ImGui::BeginTable("boxtypes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Outline", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableHeadersRow();

		for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
		{
			HitboxOverlay::CategorySettings& settings = overlay->GetCategory(i);

			ImGui::PushID(i);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::ColorButton("##swatch",
				ImGui::ColorConvertU32ToFloat4(HitboxOverlay::GetCategoryColor(i)),
				ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14.0f, 14.0f));

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

	ImGui::SetNextItemWidth(160.0f);
	ImGui::SliderFloat("Size", &g_modVals.frameMeterScale, 0.5f, 4.0f, "%.2fx");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveFloat("FrameMeter", "Scale", g_modVals.frameMeterScale);

	ImGui::SetNextItemWidth(160.0f);
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
	if (ImGui::Checkbox("Hitstun and gap", &g_modVals.frameMeterTotals))
		Settings::SaveInt("FrameMeter", "LineTotals", g_modVals.frameMeterTotals ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Blockstun, hitstun and the gap for the whole exchange, on a line of its "
			"own - above P1's numbers and below P2's, so each side's readouts sit together.");
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

		ImGui::SetNextItemWidth(160.0f);
		ImGui::DragInt("X", &g_modVals.frameMeterX, 2.0f, 0, 4096);
		if (ImGui::IsItemDeactivatedAfterEdit())
			Settings::SaveInt("FrameMeter", "PositionX", g_modVals.frameMeterX);

		ImGui::SetNextItemWidth(160.0f);
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
		ImGui::SetNextItemWidth(70.0f);

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

	ImGui::SetNextItemWidth(90.0f);
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
	ImGui::SetNextItemWidth(260.0f);

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
	ImGui::SetNextItemWidth(220.0f);
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

		ImGui::SetNextItemWidth(110.0f);
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

	ImGui::SetNextItemWidth(130.0f);
	ImGui::InputText("##scriptname", tab.name, sizeof(tab.name));

	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		DummyScript::Save(player, tab.name, tab.text);
		tab.selected = -1;
	}

	const int count = DummyScript::GetLibraryCount();
	const int chara = PaletteManager::GetCharaNumber(player);

	ImGui::SetNextItemWidth(200.0f);
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

	ImGui::SetNextItemWidth(160.0f);
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

void MainWindow::DrawConfigSection()
{
	if (!ImGui::CollapsingHeader("Config"))
	{
		SetBindCapture(-1);
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
		SetBindCapture(-1);
	}

	ImGui::EndTabBar();
}

void MainWindow::DrawConfigGeneralTab()
{
	ImGui::Text("%s %s", UNI2_IM_NAME, UNI2_IM_VERSION);
	ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

	bool& blockMouse = WindowManager::GetInstance().GetBlockGameMouse();
	if (ImGui::Checkbox("Block mouse input to the game", &blockMouse))
		Settings::SaveInt("Overlay", "BlockGameMouse", blockMouse ? 1 : 0);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Stops the game from seeing the mouse at all, so clicking the overlay can "
			"never disturb it.\nSaved to the ini as soon as it changes.");

	ImGui::SeparatorText("Holding the next-frame key");

	ImGui::SetNextItemWidth(160.0f);
	ImGui::SliderInt("Wait before repeating", &g_modVals.stepRepeatDelayMs, 0, 1000, "%d ms");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Training", "StepRepeatDelayMs", g_modVals.stepRepeatDelayMs);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How long the key has to be held before it starts stepping on its own. A "
			"tap is always one frame, whatever this says.");
	}

	ImGui::SetNextItemWidth(160.0f);
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

	if (WindowContainer* const container = WindowManager::GetInstance().GetContainer())
	{
		IWindow* const performance = container->GetWindow(WindowType_Performance);

		if (performance != nullptr && ImGui::Button("Performance View"))
			performance->Open();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Frame pacing, POTATO MODE, and where the time in each frame goes. "
				"Close it from its own title bar.");
		}
	}

}

void MainWindow::DrawKeybindsTab()
{
	struct Bind
	{
		int* value;
		const char* key;
		const char* label;
	};

	const Bind binds[] = {
		{ &g_modVals.toggleOverlayKey, "ToggleOverlay", "Open this window" },
		{ &g_modVals.toggleHitboxKey, "ToggleHitboxOverlay", "Hitbox viewer" },
		{ &g_modVals.toggleFrameMeterKey, "ToggleFrameMeter", "Frame meter" },
		{ &g_modVals.freezeFrameKey, "FreezeFrame", "Pause and resume" },
		{ &g_modVals.stepForwardKey, "StepForward", "Next frame" },
	};

	const int count = static_cast<int>(sizeof(binds) / sizeof(binds[0]));

	if (g_bindCapture >= 0)
	{
		KeyboardCapture::SetKeyCaptureActive(true);

		const int pressed = PollPressedKey();

		if (pressed == VK_ESCAPE)
		{
			SetBindCapture(-1);
		}
		else if (pressed != 0 && g_bindCapture < count)
		{
			const Bind& bind = binds[g_bindCapture];
			*bind.value = pressed;
			Settings::SaveString("Keybinds", bind.key, GetNameFromVirtualKey(pressed));
			SetBindCapture(-1);
		}
	}

	if (!ImGui::BeginTable("##keybinds", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("What");
	ImGui::TableSetupColumn("Key");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();

	for (int i = 0; i < count; ++i)
	{
		const Bind& bind = binds[i];
		const bool capturing = g_bindCapture == i;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(bind.label);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(capturing ? "press a key" : GetNameFromVirtualKey(*bind.value));

		ImGui::TableNextColumn();
		ImGui::PushID(bind.key);

		if (ImGui::SmallButton(capturing ? "Cancel" : "Change"))
			SetBindCapture(capturing ? -1 : i);

		ImGui::PopID();
	}

	ImGui::EndTable();

	if (g_bindCapture >= 0)
		ImGui::TextDisabled("Press the key you want, or Escape to cancel.");
	else
		ImGui::TextDisabled("Saved to UNI2_IM.ini as soon as a key is bound.");

	for (int i = 0; i < count; ++i)
	{
		for (int k = i + 1; k < count; ++k)
		{
			if (*binds[i].value == 0 || *binds[i].value != *binds[k].value)
				continue;

			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.45f, 1.0f), "%s and %s are both %s.",
				binds[i].label, binds[k].label, GetNameFromVirtualKey(*binds[i].value));
		}
	}
}
