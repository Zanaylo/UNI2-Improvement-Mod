#include "Overlay/Window/ThemeWindow.h"

#include "Game/CharaSelectProbe.h"
#include "Game/SceneWatch.h"
#include "Screens/CharaGrid.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"
#include "Screens/ScreenDirector.h"
#include "Screens/ScreenTheme.h"

#include <imgui.h>

ThemeWindow::ThemeWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void ThemeWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(600.0f), Ui::Scaled(420.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(440.0f), Ui::Scaled(260.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void ThemeWindow::Draw()
{
	UiText::Help("A theme draws another French-Bread game's screens over UNI2's. The game's own "
		"screen keeps running underneath and keeps every input, so the theme changes what is seen "
		"and nothing else. Themes live in UNI2-IM\\Screens; each carries that game's .pat files "
		"and one screen.ini saying which patterns make up a screen.");

	const int count = ScreenTheme::Count();
	const int active = ScreenTheme::ActiveIndex();

	if (count == 0)
	{
		UiText::Muted("No themes installed.");
		ImGui::TextWrapped("Looked in: %s", ScreenTheme::Root());

		if (ImGui::Button("Rescan folder"))
		{
			ScreenTheme::Reload();
			ScreenDirector::Invalidate();
		}

		return;
	}

	DrawThemeList(count, active);
	DrawControls(active);
	DrawScreens(active);
	DrawCursorSearch();
}

void ThemeWindow::DrawCursorSearch()
{
	const int found = CharaSelectProbe::CandidateCount();

	if (found == 0)
		return;

	ImGui::Separator();
	UiText::Help("Which of these follows the character-select cursor is not known yet. Move the "
		"cursor and watch: the one whose value tracks the slot you are on is the answer, and it "
		"goes in the log too.");

	if (!ImGui::BeginTable("##cursorsearch", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(110.0f));
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(70.0f));
	ImGui::TableSetupColumn("Changes");
	ImGui::TableHeadersRow();

	for (int i = 0; i < found; ++i)
	{
		CharaSelectProbe::Candidate candidate = {};

		if (!CharaSelectProbe::GetCandidate(i, candidate))
			continue;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("0x%06x", static_cast<unsigned>(candidate.rva));
		ImGui::TableNextColumn();
		ImGui::Text("%u", candidate.value);
		ImGui::TableNextColumn();
		ImGui::Text("%d", candidate.changes);
	}

	ImGui::EndTable();
	UiText::Muted("Grid: %s", CharaGrid::StatusText());
}

void ThemeWindow::DrawThemeList(int count, int active)
{
	if (!ImGui::BeginTable("##screenthemes", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(150.0f))))
	{
		return;
	}

	ImGui::TableSetupColumn("Theme");
	ImGui::TableSetupColumn("Screens", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(70.0f));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(90.0f));
	ImGui::TableHeadersRow();

	int apply = -1;

	for (int i = 0; i < count; ++i)
	{
		const ScreenTheme::Theme* const theme = ScreenTheme::Get(i);

		if (theme == nullptr)
			continue;

		ImGui::PushID(i);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		if (i == active)
			UiText::Good("%s", theme->name.c_str());
		else
			ImGui::TextUnformatted(theme->name.c_str());

		if (!theme->source.empty())
			UiText::Muted("%s", theme->source.c_str());

		ImGui::TableNextColumn();
		ImGui::Text("%d", static_cast<int>(theme->screens.size()));

		ImGui::TableNextColumn();

		if (ImGui::SmallButton(i == active ? "Reapply" : "Apply"))
			apply = i;

		ImGui::PopID();
	}

	ImGui::EndTable();

	if (apply < 0)
		return;

	ScreenTheme::Apply(apply);
	ScreenDirector::Invalidate();
}

void ThemeWindow::DrawControls(int active)
{
	bool drawn = ScreenDirector::IsDrawn();

	if (ImGui::Checkbox("Draw the theme", &drawn))
		ScreenDirector::SetDrawn(drawn);

	ImGui::SameLine();

	if (ImGui::Button("Rescan folder"))
	{
		ScreenTheme::Reload();
		ScreenDirector::Invalidate();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(active < 0);

	if (ImGui::Button("Back to the game's screens"))
	{
		ScreenTheme::Clear();
		ScreenDirector::Invalidate();
	}

	ImGui::EndDisabled();

	UiText::Muted("Scene %u, %s", SceneWatch::Current(), ScreenDirector::StatusText());
}

void ThemeWindow::DrawScreens(int active)
{
	const ScreenTheme::Theme* const theme = ScreenTheme::Get(active);

	if (theme == nullptr)
		return;

	ImGui::Separator();

	if (!ImGui::BeginTable("##screens", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("Screen", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(110.0f));
	ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(60.0f));
	ImGui::TableSetupColumn("Layers", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(60.0f));
	ImGui::TableSetupColumn("File");
	ImGui::TableHeadersRow();

	const uint32_t scene = SceneWatch::Current();

	for (const ScreenTheme::Screen& screen : theme->screens)
	{
		bool showing = false;

		for (uint32_t candidate : screen.scenes)
			showing = showing || candidate == scene;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		if (showing)
			UiText::Good("%s", screen.name.c_str());
		else
			ImGui::TextUnformatted(screen.name.c_str());

		ImGui::TableNextColumn();

		if (screen.scenes.empty())
			UiText::Muted("-");
		else
			ImGui::Text("%u", screen.scenes[0]);

		ImGui::TableNextColumn();
		ImGui::Text("%d", static_cast<int>(screen.layers.size()));

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(screen.pat.c_str());
	}

	ImGui::EndTable();
}
