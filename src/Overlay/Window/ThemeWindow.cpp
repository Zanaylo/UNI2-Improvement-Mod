#include "Overlay/Window/ThemeWindow.h"

#include "Game/ModFiles.h"
#include "Game/VisualThemes.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

ThemeWindow::ThemeWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void ThemeWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(560.0f), Ui::Scaled(360.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(420.0f), Ui::Scaled(240.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void ThemeWindow::Draw()
{
	UiText::Warn("Visual themes do not work yet.");

	UiText::Help("Applying one does everything it is supposed to: the files are found, indexed and "
		"handed to the game in place of its own, and the count below says how many. What has not "
		"happened is the screen changing. The one thing still unknown is whether the game reads "
		"these files at all or takes them from its own archive instead, and the log now records "
		"the first redirects it serves - if none of them name the file a theme replaces, that is "
		"the answer. A theme needs a restart either way; nothing here can repaint art the game has "
		"already loaded.");

	if (VisualThemes::WasRolledBack())
	{
		UiText::Warn("A theme was switched off because the game did not finish loading with it on. "
			"Its files are still there; it is simply not applied.");
	}

	const int count = VisualThemes::Count();
	const int active = VisualThemes::ActiveIndex();

	if (count == 0)
	{
		UiText::Muted("No themes installed.");
		ImGui::TextWrapped("Looked in: %s", VisualThemes::Path());

		if (ImGui::Button("Rescan folder"))
			VisualThemes::Reload();

		return;
	}

	if (!ImGui::BeginTable("##visualthemes", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(180.0f))))
	{
		return;
	}

	ImGui::TableSetupColumn("Theme");
	ImGui::TableSetupColumn("By", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(120.0f));
	ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(70.0f));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(90.0f));
	ImGui::TableHeadersRow();

	int apply = -1;

	for (int i = 0; i < count; ++i)
	{
		const VisualThemes::Theme* theme = VisualThemes::Get(i);
		if (theme == nullptr)
			continue;

		ImGui::PushID(i);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();

		if (i == active)
			UiText::Good("%s", theme->name);
		else
			ImGui::TextUnformatted(theme->name);

		if (theme->notes[0] != 0)
			UiText::Muted("%s", theme->notes);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(theme->author[0] != 0 ? theme->author : "-");

		ImGui::TableNextColumn();
		ImGui::Text("%d", theme->fileCount);

		ImGui::TableNextColumn();

		if (ImGui::SmallButton(i == active ? "Reapply" : "Apply"))
			apply = i;

		ImGui::PopID();
	}

	ImGui::EndTable();

	if (ImGui::Button("Rescan folder"))
		VisualThemes::Reload();

	ImGui::SameLine();

	ImGui::BeginDisabled(active < 0);

	if (ImGui::Button("Back to the game's art"))
		VisualThemes::Clear();

	ImGui::EndDisabled();

	UiText::Muted("Mods folder: %s, %d read so far", ModFiles::StatusText(), ModFiles::Hits());

	if (apply >= 0)
		VisualThemes::Apply(apply);
}
