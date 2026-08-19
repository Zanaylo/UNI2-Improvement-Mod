#include "Overlay/Window/DummyScriptGuideWindow.h"

namespace {

void Step(const char* notation, const char* meaning)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(notation);
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(meaning);
}

}

DummyScriptGuideWindow::DummyScriptGuideWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void DummyScriptGuideWindow::BeforeDraw()
{
	const ImGuiViewport* const viewport = ImGui::GetMainViewport();
	const float maxWidth = viewport->WorkSize.x;
	const float maxHeight = viewport->WorkSize.y;

	const float width = maxWidth * 0.46f < 460.0f ? maxWidth : maxWidth * 0.46f;
	const float height = maxHeight * 0.70f < 320.0f ? maxHeight : maxHeight * 0.70f;

	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(380.0f, 240.0f), ImVec2(maxWidth, maxHeight));
}

void DummyScriptGuideWindow::Draw()
{
	ImGui::SeparatorText("What this is");

	ImGui::TextWrapped("Type what a character should do instead of recording it. There is a tab for "
		"each side, each with its own script, and Play runs it one step per frame.");

	ImGui::Spacing();
	ImGui::SeparatorText("The notation");

	ImGui::TextWrapped("One step per line. Anything after a step on the same line is ignored, so "
		"notes are free.");

	ImGui::Spacing();

	if (ImGui::BeginTable("##steps", 2, ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("step", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("meaning", ImGuiTableColumnFlags_WidthStretch);

		Step("W12", "Wait twelve frames, holding whatever is held.");
		Step("[4]", "Hold back. Stays held until released.");
		Step("]4[", "Release it.");
		Step("[A]  ]A[", "The same for a button.");
		Step("4B", "Back and B together, for one frame.");
		Step("5A", "Neutral and A. 5 is neutral, as on a numpad.");
		Step("236C", "A motion, one frame per direction, then C.");

		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::TextWrapped("Directions are numpad numbers from the dummy's point of view - 4 is back, "
		"6 is forward, and they swap when the sides do. Buttons are A, B, C and D.");

	ImGui::Spacing();
	ImGui::SeparatorText("An example");

	ImGui::TextUnformatted("W12\n[4]\nW30\n]4[\n236C");
	ImGui::TextWrapped("Wait twelve frames, hold back for thirty, let go, then quarter-circle C.");

	ImGui::Spacing();
	ImGui::SeparatorText("One tab per side");

	ImGui::TextWrapped("Each side has its own script, name and saved list, and the two run "
		"independently - set both going and a whole scenario plays out.");

	ImGui::Spacing();

	ImGui::BulletText("Play: runs the script on that side.");
	ImGui::BulletText("Save: keeps it under that side's character, by the name in the box.");
	ImGui::BulletText("The list: the scripts saved for that side's character.");
	ImGui::BulletText("Delete: removes the one picked in the list.");

	ImGui::Spacing();
	ImGui::TextWrapped("P2 also has a Slot, with Write to slot and Read from slot. Its Play writes "
		"the slot and hands it to the game's own playback, so everything the playback menu gives it "
		"comes too - repeat, on reversal, after a restart. P1 has no slot because the game's takes "
		"only ever reach the dummy; its Play runs through Player Control instead.");

	ImGui::Spacing();
	ImGui::TextWrapped("The number beside Play is how many frames the script is, or what went wrong "
		"if it would not parse.");

	ImGui::Spacing();
	ImGui::SeparatorText("Playing it back");

	ImGui::TextWrapped("Play is usually all you need. Write to slot is for when you want the game's "
		"own playback settings on it: write the slot, then set the dummy's action to a recorded "
		"playback in the training menu. A slot the game has never recorded into is filled in for you, "
		"so it stops being greyed out.");
}
